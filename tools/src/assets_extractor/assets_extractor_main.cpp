#include "assets_extractor_api.hpp"
#include "port_asset_log.hpp"
#include "port_rom_profile.h"
#include "global.h"

#include <fmt/format.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

/* Standalone binary owns these globals; in tmc_pc they're defined in
 * port_rom.c and Port_LoadRom populates them BEFORE the API runs.
 * port_gba_mem.h declares them with C++ linkage when compiled from a
 * .cpp TU, so match that here rather than re-introducing extern "C". */
u8* gRomData = nullptr;
u32 gRomSize = 0;

extern "C" const unsigned char kEmbeddedSoundsJson[];
extern "C" const std::size_t kEmbeddedSoundsJsonSize;

/* Profile-keyed cache subdir (assets/<sub>/), matching the runtime loader. The
 * standalone binary has no loaded engine profile, so identify the ROM from disk
 * before selecting an output directory. */
static std::string DetectProfileSubdir(const std::filesystem::path& rom_path, PortRomHashes* hashes_out)
{
    PortRomHashes hashes = {};
    const PortRomProfile* profile = Port_IdentifyRomFile(rom_path.string().c_str(), &hashes);
    if (hashes_out != nullptr) {
        *hashes_out = hashes;
    }
    if (profile != nullptr) {
        Port_SetActiveRomProfile(profile);
        return Port_GetAssetCacheSubdir();
    }
    return {};
}

int main(int argc, char* argv[])
{
    bool verbose = false;
    bool runtime_only = false;
    bool force = false;
    bool pack_runtime = false;
    std::string region;  /* empty => exact profile detection from the ROM */
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--runtime-only") {
            runtime_only = true;
        } else if (arg == "--force" || arg == "-f") {
            force = true;
        } else if (arg == "--pak") {
            pack_runtime = true;
        } else if (arg == "--region" && i + 1 < argc) {
            region = argv[++i];
        } else if (arg.rfind("--region=", 0) == 0) {
            region = std::string(arg.substr(std::string_view("--region=").size()));
        } else if (arg == "--help" || arg == "-h") {
            fmt::print("Usage: asset_extractor [--verbose] [--runtime-only] [--force] [--pak] [--region usa|eu|jp|jp-angel-sp4]\n"
                       "  --verbose       Print per-asset notes/warnings.\n"
                       "  --runtime-only  Skip writing the editable assets_src/ tree.\n"
                       "  --force         Re-extract even if assets/ are already up to date.\n"
                       "  --pak           Pack runtime assets into per-category .pak archives\n"
                       "                  instead of writing thousands of loose files.\n"
                       "  --region R      Assert the detected cache subdir (usa|eu|jp|jp-angel-sp4).\n");
            return 0;
        }
    }

    std::filesystem::path executable_dir;
    if (argc > 0) {
        executable_dir = AssetExtractorApi::FindExecutableDirectory(argv[0]);
    }
    if (executable_dir.empty()) {
        executable_dir = std::filesystem::current_path();
    }

    AssetExtractorApi::Options opt;
    opt.rom_path = executable_dir / "baserom.gba";

    /* Per-profile cache: assets/<profile>/ + assets_src/<profile>, so clean JP
     * and Angel SP4 never reuse each other's extracted tree. */
    PortRomHashes hashes = {};
    const std::string detected_subdir = DetectProfileSubdir(opt.rom_path, &hashes);
    if (detected_subdir.empty()) {
        fmt::print(stderr,
                   "asset_extractor: baserom.gba is not a supported exact ROM profile.\n"
                   "SHA-1: {}\nSHA-256: {}\n",
                   hashes.sha1, hashes.sha256);
        return 1;
    }
    if (!region.empty() && region != detected_subdir) {
        fmt::print(stderr,
                   "asset_extractor: --region={} does not match this ROM's profile cache ({}).\n",
                   region, detected_subdir);
        return 1;
    }
    opt.editable_root = executable_dir / "assets_src" / detected_subdir;
    opt.runtime_root = executable_dir / "assets" / detected_subdir;
    opt.runtime_only = runtime_only;
    opt.pack_runtime = pack_runtime;
    opt.force = force;
    opt.verbose = verbose;

    std::string err;
    if (!AssetExtractorApi::ExtractAssets(opt, &err)) {
        PortAssetLog::Reporter::Instance().Error(err);
        return 1;
    }

    /* Emit sounds.json next to the binary (= where tmc_pc launches
     * from). The data is baked in at build time via embedded_sounds_json.cpp
     * so the extractor is fully self-contained. */
    {
        const std::filesystem::path sounds_dst = executable_dir / "sounds.json";
        std::ofstream f(sounds_dst, std::ios::binary);
        if (f) {
            f.write(reinterpret_cast<const char*>(kEmbeddedSoundsJson),
                    static_cast<std::streamsize>(kEmbeddedSoundsJsonSize));
        }
    }

    /* Mirror the extractor's loaded ROM bytes into the gRomData /
     * gRomSize globals so any standalone-linked TU that still consults
     * them sees the loaded bytes. The cast drops `const` because the
     * legacy globals predate the const audit; the standalone binary
     * never writes through them. */
    const std::span<const uint8_t> rom = AssetExtractorApi::LoadedRomBytes();
    gRomData = const_cast<u8*>(rom.data());
    gRomSize = static_cast<u32>(rom.size());
    return 0;
}
