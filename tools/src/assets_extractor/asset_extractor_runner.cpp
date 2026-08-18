#include "asset_extractor_runner.h"
#include "assets_extractor_api.hpp"
#include "port_rom_profile.h"

namespace {

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
}

bool CopySoundsJson(const std::filesystem::path& root, std::string* error) {
    const std::filesystem::path target = root / "sounds.json";
    if (std::filesystem::exists(target)) {
        return true;
    }

    for (std::filesystem::path probe = root; !probe.empty(); probe = probe.parent_path()) {
        const std::filesystem::path source = probe / "assets" / "sounds.json";
        if (std::filesystem::exists(source)) {
            std::error_code ec;
            std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                SetError(error, "failed to copy sounds.json: " + ec.message());
                return false;
            }
            return true;
        }

        const std::filesystem::path parent = probe.parent_path();
        if (parent == probe) {
            break;
        }
    }

    SetError(error, "sounds.json was not found");
    return false;
}

} // namespace

bool RunEmbeddedAssetExtractor(const std::filesystem::path& root, std::string* error) {
    const std::filesystem::path romPath = root / "baserom.gba";

    PortRomHashes hashes = {};
    const PortRomProfile* profile = Port_IdentifyRomFile(romPath.string().c_str(), &hashes);
    Port_SetActiveRomProfile(profile);
    if (!Port_RomProfileIsPlayable(profile)) {
        SetError(error, "unsupported ROM profile (SHA-256: " + std::string(hashes.sha256) + ")");
        return false;
    }

    const std::string cacheSubdir = Port_GetAssetCacheSubdir();
    AssetExtractorApi::Options options;
    options.rom_path = romPath;
    options.editable_root = root / "assets_src" / cacheSubdir;
    options.runtime_root = root / "assets" / cacheSubdir;
    options.runtime_only = false;
    options.pack_runtime = false;
    options.force = false;
    if (!AssetExtractorApi::ExtractAssets(options, error)) {
        return false;
    }

    return CopySoundsJson(root, error);
}
