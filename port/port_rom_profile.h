#pragma once

#include <stddef.h>
#include <stdint.h>

#include "port_config.h"
#include "port_rom_hash.h"
#include "port_text_codec.h"

#define PORT_MAX_GLYPH_BANKS 16u
#define PORT_TEXT_REMAP_CAPACITY 346u

typedef enum {
    PORT_ROM_VARIANT_UNKNOWN = 0,
    PORT_ROM_VARIANT_USA_RETAIL,
    PORT_ROM_VARIANT_EU_RETAIL,
    PORT_ROM_VARIANT_JP_RETAIL,
    PORT_ROM_VARIANT_JP_ANGEL_SP4,
    PORT_ROM_VARIANT_USA_DEMO,
    PORT_ROM_VARIANT_JP_DEMO,
} PortRomVariant;

typedef struct {
    PortRomVariant variant;
    RomRegion region;
    const char* id;
    const char* displayName;
    const char* gameCode;
    const char* sha1;
    const char* sha256;
    size_t expectedSize;
    int playable;
    PortTextCodec textCodec;
    uint32_t glyphTableOffset;
    uint32_t textRemapOffset;
    uint32_t textRemapSize;
    uint32_t glyphBankCount;
    uint32_t wideGlyphFirstBank;
    uint32_t specialPaletteBank;
    const char* saveFilename;
} PortRomProfile;

#ifdef __cplusplus
extern "C" {
#endif

const PortRomProfile* Port_IdentifyRomHashes(const PortRomHashes* hashes, const char gameCode[4]);
const PortRomProfile* Port_IdentifyRomBuffer(const void* data, size_t size, PortRomHashes* hashesOut);
const PortRomProfile* Port_IdentifyRomFile(const char* path, PortRomHashes* hashesOut);
int Port_RomProfileIsPlayable(const PortRomProfile* profile);

void Port_SetActiveRomProfile(const PortRomProfile* profile);
const PortRomProfile* Port_GetActiveRomProfile(void);
PortRomVariant Port_GetRomVariant(void);
PortTextCodec Port_GetTextCodec(void);
uint32_t Port_GetGlyphBankCount(void);
uint32_t Port_GetWideGlyphFirstBank(void);
uint32_t Port_GetSpecialPaletteBank(void);
uint32_t Port_GetGlyphTableOffset(uint32_t regionOffset);
uint32_t Port_GetTextRemapOffset(uint32_t regionOffset);
uint32_t Port_GetTextRemapSize(void);
const char* Port_GetVariantSaveFilename(void);
const char* Port_GetAssetCacheSubdir(void);

int Port_ValidateRomProfile(const PortRomProfile* profile, const RomOffsets* offsets, const uint8_t* romData,
                            size_t romSize, char* error, size_t errorSize);

#ifdef __cplusplus
}
#endif
