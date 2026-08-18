/*
 * port/port_rom_picker.c — SDL3 file-picker fallback for "no ROM
 * found." Lets the user point at any .gba file without having to
 * manually rename and place it. We validate the picked file against
 * the shared playable-ROM profiles, then install it as baserom.gba —
 * next to the executable on desktop, in the app data dir (the CWD) on
 * Android — so subsequent launches skip the prompt.
 *
 * The picked path is read through SDL_LoadFile: on Android the SAF
 * picker returns content:// URIs that plain fopen cannot open, and
 * SDL_IOFromFile routes those through ContentResolver.
 */
#define _DEFAULT_SOURCE 1 /* readlink(2) on glibc */
#define _BSD_SOURCE 1
#define _POSIX_C_SOURCE 200809L

#include "port_rom_picker.h"
#include "port_rom_profile.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_messagebox.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#define _GNU_SOURCE
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

/* Outcome of the async picker callback, published cross-thread: on
 * Android the callback runs on the Java UI thread (onActivityResult)
 * while the SDL main thread spins in the wait loop below. SDL atomics
 * give the release/acquire ordering `volatile` does not. Path bytes
 * are written before the status flips to a terminal state. */
enum {
    PICK_PENDING = 0,
    PICK_CANCELLED,
    PICK_SUCCESS,
    PICK_FAILED,
};

static SDL_AtomicInt sPickStatus;
static char sPickPath[4096];

static void SDLCALL RomPickerCallback(void* userdata, const char* const* filelist, int filter) {
    (void)userdata;
    (void)filter;
    if (!filelist) {
        SDL_SetAtomicInt(&sPickStatus, PICK_FAILED);
        return;
    }
    if (!filelist[0]) {
        SDL_SetAtomicInt(&sPickStatus, PICK_CANCELLED);
        return;
    }
    strncpy(sPickPath, filelist[0], sizeof(sPickPath) - 1);
    sPickPath[sizeof(sPickPath) - 1] = '\0';
    SDL_SetAtomicInt(&sPickStatus, PICK_SUCCESS);
}

#ifndef __ANDROID__

static int GetExeDir(char* out, size_t out_len) {
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)out_len);
    if (n == 0 || n >= out_len)
        return -1;
    char* slash = strrchr(out, '\\');
    if (!slash)
        slash = strrchr(out, '/');
    if (!slash)
        return -1;
    *slash = '\0';
    return 0;
#elif defined(__APPLE__)
    uint32_t sz = (uint32_t)out_len;
    if (_NSGetExecutablePath(out, &sz) != 0)
        return -1;
    char* slash = strrchr(out, '/');
    if (!slash)
        return -1;
    *slash = '\0';
    return 0;
#else
    ssize_t n = readlink("/proc/self/exe", out, out_len - 1);
    if (n <= 0)
        return -1;
    out[n] = '\0';
    char* slash = strrchr(out, '/');
    if (!slash)
        return -1;
    *slash = '\0';
    return 0;
#endif
}

#endif /* !__ANDROID__ */

/* Resolve where the picked ROM gets installed. Desktop: next to the
 * executable, matching the release-tarball layout that
 * Port_FindBaseRomPath probes. Android: the CWD — port_main.c chdir'd
 * to the app's files dir before any of this runs, and the bare
 * "baserom.gba" candidate resolves there. The path must be ABSOLUTE:
 * SDL_IOFromFile on Android resolves relative paths against internal
 * storage (SDL_GetAndroidInternalStoragePath), not the CWD, so a bare
 * "baserom.gba" would be written where fopen-based probing never
 * looks. (The exe dir on Android is an unwritable system path, and
 * the picked file is a content:// URI that only SDL_IOFromFile can
 * read anyway.) */
static int ResolveInstallPath(char* dst, size_t dst_len) {
#ifdef __ANDROID__
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd)))
        return -1;
    snprintf(dst, dst_len, "%s/baserom.gba", cwd);
    return 0;
#else
    char exedir[4096];
    if (GetExeDir(exedir, sizeof(exedir)) != 0)
        return -1;
    snprintf(dst, dst_len, "%s%cbaserom.gba", exedir,
#ifdef _WIN32
             '\\'
#else
             '/'
#endif
    );
    return 0;
#endif
}

/* Returns 0 if the user picked a valid ROM and we installed it as
 * baserom.gba (so the caller can re-run Port_FindBaseRomPath and
 * find it). Returns -1 on cancel/error.
 *
 * The prelaunch UI shows the user-facing "Pick your ROM" prompt
 * before calling this, so we skip straight to the file dialog. */
int Port_RomPicker_PromptAndInstall(void) {
    fprintf(stderr, "[rom-picker] opening SDL file dialog...\n");

    SDL_DialogFileFilter filters[] = {
        { "Game Boy Advance ROM", "gba" },
        { "All files", "*" },
    };

    SDL_SetAtomicInt(&sPickStatus, PICK_PENDING);
    sPickPath[0] = '\0';
    SDL_ShowOpenFileDialog(RomPickerCallback, NULL, NULL, filters, 2, NULL, false);

    /* Block until the user picks (or cancels). The picker is async on
     * every backend — pump events here so platform-native code can
     * deliver the callback (on Android it arrives via onActivityResult
     * after a round-trip through the system documents UI). */
    while (SDL_GetAtomicInt(&sPickStatus) == PICK_PENDING) {
        SDL_PumpEvents();
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                SDL_SetAtomicInt(&sPickStatus, PICK_CANCELLED);
                break;
            }
        }
        SDL_Delay(16);
    }

    if (SDL_GetAtomicInt(&sPickStatus) != PICK_SUCCESS) {
        return -1;
    }

    /* One read, validate in memory, one write. SDL_LoadFile routes
     * through SDL_IOFromFile: plain paths everywhere, content:// URIs
     * on Android (SAF), UTF-8 -> UTF-16 paths on Windows. A TMC dump
     * is at most 16 MiB, so the buffer is cheap and transient. */
    size_t romLen = 0;
    void* romData = SDL_LoadFile(sPickPath, &romLen);
    if (!romData) {
        char msg[4608];
        snprintf(msg, sizeof(msg), "Could not read the picked file:\n%s\n\n%s", sPickPath, SDL_GetError());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Read failed", msg, NULL);
        return -1;
    }

    PortRomHashes hashes = { 0 };
    const PortRomProfile* profile = Port_IdentifyRomBuffer(romData, romLen, &hashes);
    if (!Port_RomProfileIsPlayable(profile)) {
        SDL_free(romData);
        char msg[5120];
        snprintf(msg, sizeof(msg),
                 "That file doesn't match a playable TMC ROM profile:\n\n"
                 "  Path:     %s\n"
                 "  SHA-1:    %s\n"
                 "  SHA-256:  %s\n\n"
                 "Accepted SHA-1 hashes:\n"
                 "  USA       b4bd50e4131b027c334547b4524e2dbbd4227130\n"
                 "  EU        cff199b36ff173fb6faf152653d1bccf87c26fb7\n"
                 "  JP        6c5404a1effb17f481f352181d0f1c61a2765c5d\n"
                 "  Angel SP4 ba04cfbe93d12d2ad684c52234472fa12a5b53d7",
                 sPickPath, hashes.sha1, hashes.sha256);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unsupported ROM", msg, NULL);
        return -1;
    }

    /* Install as baserom.gba (overwrite if already exists). Once
     * written, Port_FindBaseRomPath picks it up via the usual
     * candidate list. */
    char dst[4096];
    if (ResolveInstallPath(dst, sizeof(dst)) != 0) {
        SDL_free(romData);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Install failed",
                                 "Could not locate this executable's directory.", NULL);
        return -1;
    }

    const bool wrote = SDL_SaveFile(dst, romData, romLen);
    SDL_free(romData);
    if (!wrote) {
        char msg[4608];
        snprintf(msg, sizeof(msg), "Could not write %s:\n%s", dst, SDL_GetError());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Install failed", msg, NULL);
        return -1;
    }

    fprintf(stderr, "[rom-picker] installed %s ROM -> %s\n", profile->displayName, dst);
    return 0;
}
