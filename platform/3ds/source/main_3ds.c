#include "platform_3ds.h"

#include "port_audio.h"
#include "port_ppu.h"
#include "port_rom.h"
#include "port_rom_profile.h"
#include "port_runtime_config.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define APP_DIR "sdmc:/3ds/The Minish Cap 3DS"
#define ROM_PATH_SIZE 512

extern void AgbMain(void);

static const PortRomProfile* sSelectedRomProfile;
static PortRomHashes sSelectedRomHashes;
static const PortRomProfile* sMismatchedRomProfile;

static const char* BuildBaselineName(void) {
#if defined(JP)
    return "JP";
#elif defined(EU)
    return "EU";
#else
    return "USA";
#endif
}

/* The original 3DS target is a proven USA/EU multi-region build. JP still
 * carries compile-time data/layout choices that require the JP asset baseline,
 * so do not silently boot either family with the other's binary. */
static int RomMatchesBuildFamily(const PortRomProfile* profile) {
#if defined(JP)
    return profile->region == ROM_REGION_JP;
#else
    return profile->region == ROM_REGION_USA || profile->region == ROM_REGION_EU;
#endif
}

static const char* RequiredBuildBaseline(const PortRomProfile* profile) {
    if (profile != NULL && profile->region == ROM_REGION_JP) return "JP";
    return "USA";
}

static int PrepareStorage(void) {
    mkdir("sdmc:/3ds", 0777);
    if (mkdir(APP_DIR, 0777) != 0 && errno != EEXIST) return 0;
    return chdir(APP_DIR) == 0;
}

static int HasGbaExtension(const char* name) {
    const size_t length = strlen(name);
    if (length < 5) return 0;
    const char* ext = name + length - 4;
    return tolower((unsigned char)ext[0]) == '.' &&
           tolower((unsigned char)ext[1]) == 'g' &&
           tolower((unsigned char)ext[2]) == 'b' &&
           tolower((unsigned char)ext[3]) == 'a';
}

static int RomIsSupported(const char* path) {
    PortRomHashes hashes = { 0 };
    const PortRomProfile* profile = Port_IdentifyRomFile(path, &hashes);
    if (!Port_RomProfileIsPlayable(profile)) {
        if (hashes.sha256[0] != '\0') {
            printf("Skipping unsupported ROM: %s\nSHA-256: %s\n", path, hashes.sha256);
        }
        return 0;
    }
    if (!RomMatchesBuildFamily(profile)) {
        if (sMismatchedRomProfile == NULL) sMismatchedRomProfile = profile;
        printf("Skipping ROM for a different build family: %s\n"
               "Profile: %s\nBuild baseline: %s\nRequired baseline: %s\n",
               path, profile->displayName, BuildBaselineName(), RequiredBuildBaseline(profile));
        return 0;
    }
    sSelectedRomProfile = profile;
    sSelectedRomHashes = hashes;
    return 1;
}

static int FindRom(char* out, size_t outSize) {
    DIR* dir = opendir(".");
    if (!dir) return 0;

    int foundGba = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (!HasGbaExtension(entry->d_name)) continue;

        struct stat info;
        if (stat(entry->d_name, &info) != 0 || !S_ISREG(info.st_mode)) continue;
        foundGba = 1;
        if (!RomIsSupported(entry->d_name)) continue;
        snprintf(out, outSize, "%s", entry->d_name);
        closedir(dir);
        return 1;
    }

    closedir(dir);
    if (sMismatchedRomProfile != NULL) return -2;
    return foundGba ? -1 : 0;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (!Platform3DS_Init()) return 1;
    Platform3DS_ShowSplash();

    printf("The Minish Cap 3DS v" TMC_PORT_VERSION "\n\n");
    printf("System: %s\n", Platform3DS_IsNew3DS() ? "New Nintendo 3DS" : "Nintendo 3DS");
    printf("Performance: %s\n",
           Platform3DS_IsNew3DS() ? "New 3DS full presentation"
                                  : "Old 3DS adaptive presentation skip (max 3)");
    printf("PPU worker core 1: %u%%\n", Platform3DS_Core1TimeLimit());
    printf("Extra New 3DS core: %s\n\n", Platform3DS_IsNew3DS() ? "enabled" : "unavailable");
    printf("Preparing storage...\n");
    if (!PrepareStorage()) {
        Platform3DS_ShowFatal("Storage error", "Could not open " APP_DIR ".");
        Platform3DS_Shutdown();
        return 1;
    }

    char romPath[ROM_PATH_SIZE];
    int romResult = FindRom(romPath, sizeof(romPath));
    if (romResult <= 0) {
        char message[512];
        if (romResult == -2) {
            snprintf(message, sizeof(message),
                     "Found %s (%s), but this package uses the %s compile-time baseline.\n\n"
                     "Use a build with TMC3DS_REGION=%s for this ROM. JP builds accept clean Japan and Angel SP4; "
                     "USA/EU builds accept the original USA and Europe ROMs.",
                     sMismatchedRomProfile->displayName, sMismatchedRomProfile->id, BuildBaselineName(),
                     RequiredBuildBaseline(sMismatchedRomProfile));
        } else if (romResult < 0) {
            snprintf(message, sizeof(message),
                     "None of the .gba files in:\n%s\n"
                     "matches a supported ROM profile.\n\n"
                     "Expected SHA-1:\nUSA: b4bd50e4131b027c334547b4524e2dbbd4227130\n"
                     "Europe: cff199b36ff173fb6faf152653d1bccf87c26fb7\n"
                     "Japan: 6c5404a1effb17f481f352181d0f1c61a2765c5d\n"
                     "Angel SP4: ba04cfbe93d12d2ad684c52234472fa12a5b53d7",
                     APP_DIR);
        } else {
            snprintf(message, sizeof(message),
                     "Copy a supported Minish Cap ROM to:\n%s\n\nAny .gba filename is accepted.\n\n"
                     "Supported: USA, Europe, clean Japan, and Angel Chinese SP4.",
                     APP_DIR);
        }
        Platform3DS_ShowFatal(romResult == -2 ? "ROM/build mismatch" : romResult < 0 ? "Unsupported ROM" : "ROM not found",
                              message);
        Platform3DS_Shutdown();
        return 1;
    }

    FILE* rom = fopen(romPath, "rb");
    if (!rom) {
        Platform3DS_ShowFatal("ROM error", "Could not open the selected .gba file.");
        Platform3DS_Shutdown();
        return 1;
    }
    fclose(rom);

    printf("ROM profile: %s (%s)\n", sSelectedRomProfile->displayName, sSelectedRomProfile->id);
    printf("ROM SHA-1: %s\n", sSelectedRomHashes.sha1);
    printf("Loading ROM and tables...\n");
    Port_Config_Load("tmc3ds.ini");
    Port_LoadRom(romPath);
    Port_PPU_Init(NULL);
    if (!Port_Audio_Init()) {
        printf("Warning: audio is unavailable.\n");
    }

    printf("Starting engine...\n");
    Platform3DS_EnterGameplayDisplay();
    AgbMain();

    Port_PPU_Shutdown();
    Platform3DS_Shutdown();
    return 0;
}
