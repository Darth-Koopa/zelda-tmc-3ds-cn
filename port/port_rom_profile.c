#include "port_rom_profile.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define RETAIL_ROM_SIZE 0x01000000u
#define RETAIL_TEXT_REMAP_SIZE 346u

static const PortRomProfile kProfiles[] = {
    {
        PORT_ROM_VARIANT_USA_RETAIL,
        ROM_REGION_USA,
        "usa-retail",
        "USA retail",
        "BZME",
        "b4bd50e4131b027c334547b4524e2dbbd4227130",
        "bedc74df62755f705398273de8ed3bc59be610cf55760d0b9aa277f1f5035e73",
        RETAIL_ROM_SIZE,
        1,
        PORT_TEXT_CODEC_RETAIL,
        0,
        0,
        RETAIL_TEXT_REMAP_SIZE,
        9,
        5,
        7,
        NULL,
    },
    {
        PORT_ROM_VARIANT_EU_RETAIL,
        ROM_REGION_EU,
        "eu-retail",
        "Europe retail",
        "BZMP",
        "cff199b36ff173fb6faf152653d1bccf87c26fb7",
        "c84645731952b7677f514ae222683504066334ab2af904e64a8a84ffc1af46c6",
        RETAIL_ROM_SIZE,
        1,
        PORT_TEXT_CODEC_RETAIL,
        0,
        0,
        RETAIL_TEXT_REMAP_SIZE,
        9,
        5,
        7,
        NULL,
    },
    {
        PORT_ROM_VARIANT_JP_RETAIL,
        ROM_REGION_JP,
        "jp-retail",
        "Japan retail",
        "BZMJ",
        "6c5404a1effb17f481f352181d0f1c61a2765c5d",
        "16ac2572ba17e9cb2a70093d41f97ef8cff66c56417e45ea98adacdc51bb4b38",
        RETAIL_ROM_SIZE,
        1,
        PORT_TEXT_CODEC_RETAIL,
        0,
        0,
        RETAIL_TEXT_REMAP_SIZE,
        9,
        5,
        7,
        NULL,
    },
    {
        PORT_ROM_VARIANT_JP_ANGEL_SP4,
        ROM_REGION_JP,
        "jp-angel-sp4-zh-cn",
        "Japan Angel Team Chinese SP4",
        "BZMJ",
        "ba04cfbe93d12d2ad684c52234472fa12a5b53d7",
        "f51c6c2f90e18ee91203dd767307271e06901b5bff35c3a567d52f61a39d166d",
        RETAIL_ROM_SIZE,
        1,
        PORT_TEXT_CODEC_ANGEL_SP4,
        0x00DC9F00u,
        0x00E4F000u,
        0xA0u,
        16,
        3,
        3,
        "tmc_jp_angel_sp4.sav",
    },
    {
        PORT_ROM_VARIANT_USA_DEMO,
        ROM_REGION_USA,
        "usa-demo",
        "USA demo",
        "BZHE",
        "63fcad218f9047b6a9edbb68c98bd0dec322d7a1",
        "",
        0,
        0,
        PORT_TEXT_CODEC_RETAIL,
        0,
        0,
        RETAIL_TEXT_REMAP_SIZE,
        9,
        5,
        7,
        NULL,
    },
    {
        PORT_ROM_VARIANT_JP_DEMO,
        ROM_REGION_JP,
        "jp-demo",
        "Japan demo",
        "BZMJ",
        "9cdb56fa79bba13158b81925c1f3641251326412",
        "",
        0,
        0,
        PORT_TEXT_CODEC_RETAIL,
        0,
        0,
        RETAIL_TEXT_REMAP_SIZE,
        9,
        5,
        7,
        NULL,
    },
};

static const PortRomProfile* sActiveProfile;

static uint32_t ReadU32(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void SetError(char* error, size_t errorSize, const char* format, ...) {
    va_list args;
    if (error == NULL || errorSize == 0) {
        return;
    }
    va_start(args, format);
    vsnprintf(error, errorSize, format, args);
    va_end(args);
}

static int HashMatches(const char* expected, const char* actual) {
    return expected == NULL || expected[0] == '\0' || strcmp(expected, actual) == 0;
}

const PortRomProfile* Port_IdentifyRomHashes(const PortRomHashes* hashes, const char gameCode[4]) {
    if (hashes == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(kProfiles) / sizeof(kProfiles[0]); ++i) {
        const PortRomProfile* profile = &kProfiles[i];
        if (profile->expectedSize != 0 && hashes->size != profile->expectedSize) {
            continue;
        }
        if (gameCode != NULL && memcmp(gameCode, profile->gameCode, 4) != 0) {
            continue;
        }
        if (HashMatches(profile->sha1, hashes->sha1) && HashMatches(profile->sha256, hashes->sha256)) {
            return profile;
        }
    }
    return NULL;
}

const PortRomProfile* Port_IdentifyRomBuffer(const void* data, size_t size, PortRomHashes* hashesOut) {
    PortRomHashes hashes = { 0 };
    const char* gameCode = NULL;
    if (hashesOut != NULL) {
        memset(hashesOut, 0, sizeof(*hashesOut));
    }
    if (data == NULL) {
        return NULL;
    }
    Port_HashBuffer(data, size, &hashes);
    if (hashesOut != NULL) {
        *hashesOut = hashes;
    }
    if (size >= 0xB0u) {
        gameCode = (const char*)data + 0xACu;
    }
    return Port_IdentifyRomHashes(&hashes, gameCode);
}

const PortRomProfile* Port_IdentifyRomFile(const char* path, PortRomHashes* hashesOut) {
    PortRomHashes hashes = { 0 };
    char gameCode[4];
    FILE* file;
    const PortRomProfile* profile;
    if (hashesOut != NULL) {
        memset(hashesOut, 0, sizeof(*hashesOut));
    }
    if (!Port_HashFile(path, &hashes)) {
        return NULL;
    }
    file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0xAC, SEEK_SET) != 0 || fread(gameCode, 1, sizeof(gameCode), file) != sizeof(gameCode)) {
        if (file != NULL) {
            fclose(file);
        }
        return NULL;
    }
    fclose(file);
    if (hashesOut != NULL) {
        *hashesOut = hashes;
    }
    profile = Port_IdentifyRomHashes(&hashes, gameCode);
    return profile;
}

int Port_RomProfileIsPlayable(const PortRomProfile* profile) {
    return profile != NULL && profile->playable;
}

void Port_SetActiveRomProfile(const PortRomProfile* profile) {
    sActiveProfile = profile;
}

const PortRomProfile* Port_GetActiveRomProfile(void) {
    return sActiveProfile;
}

PortRomVariant Port_GetRomVariant(void) {
    return sActiveProfile != NULL ? sActiveProfile->variant : PORT_ROM_VARIANT_UNKNOWN;
}

PortTextCodec Port_GetTextCodec(void) {
    return sActiveProfile != NULL ? sActiveProfile->textCodec : PORT_TEXT_CODEC_RETAIL;
}

uint32_t Port_GetGlyphBankCount(void) {
    return sActiveProfile != NULL ? sActiveProfile->glyphBankCount : 9u;
}

uint32_t Port_GetWideGlyphFirstBank(void) {
    return sActiveProfile != NULL ? sActiveProfile->wideGlyphFirstBank : 5u;
}

uint32_t Port_GetSpecialPaletteBank(void) {
    return sActiveProfile != NULL ? sActiveProfile->specialPaletteBank : 7u;
}

uint32_t Port_GetGlyphTableOffset(uint32_t regionOffset) {
    return sActiveProfile != NULL && sActiveProfile->glyphTableOffset != 0 ? sActiveProfile->glyphTableOffset
                                                                          : regionOffset;
}

uint32_t Port_GetTextRemapOffset(uint32_t regionOffset) {
    return sActiveProfile != NULL && sActiveProfile->textRemapOffset != 0 ? sActiveProfile->textRemapOffset
                                                                          : regionOffset;
}

uint32_t Port_GetTextRemapSize(void) {
    return sActiveProfile != NULL ? sActiveProfile->textRemapSize : RETAIL_TEXT_REMAP_SIZE;
}

const char* Port_GetVariantSaveFilename(void) {
    return sActiveProfile != NULL ? sActiveProfile->saveFilename : NULL;
}

const char* Port_GetAssetCacheSubdir(void) {
    switch (Port_GetRomVariant()) {
        case PORT_ROM_VARIANT_EU_RETAIL:
            return "eu";
        case PORT_ROM_VARIANT_JP_RETAIL:
            return "jp";
        case PORT_ROM_VARIANT_JP_ANGEL_SP4:
            return "jp-angel-sp4";
        case PORT_ROM_VARIANT_USA_DEMO:
            return "usa-demo";
        case PORT_ROM_VARIANT_JP_DEMO:
            return "jp-demo";
        case PORT_ROM_VARIANT_USA_RETAIL:
        case PORT_ROM_VARIANT_UNKNOWN:
        default:
            return "usa";
    }
}

static int ValidateOffset(const char* name, uint32_t offset, size_t romSize, char* error, size_t errorSize) {
    if (offset == 0 || offset >= romSize) {
        SetError(error, errorSize, "%s offset 0x%X is outside the ROM", name, offset);
        return 0;
    }
    return 1;
}

static int ValidatePackedPointerTable(const char* name, const uint8_t* romData, size_t romSize, uint32_t tableOffset,
                                      uint32_t count, uint32_t targetBytes, char* error, size_t errorSize) {
    if (tableOffset > romSize || (size_t)count * sizeof(uint32_t) > romSize - tableOffset) {
        SetError(error, errorSize, "%s pointer table at 0x%X is truncated", name, tableOffset);
        return 0;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t gbaAddress = ReadU32(romData + tableOffset + i * sizeof(uint32_t)) & ~1u;
        if (gbaAddress < 0x08000000u) {
            SetError(error, errorSize, "%s[%u] is not a ROM pointer (0x%08X)", name, i, gbaAddress);
            return 0;
        }
        const uint32_t targetOffset = gbaAddress - 0x08000000u;
        if (targetOffset > romSize || targetBytes > romSize - targetOffset) {
            SetError(error, errorSize, "%s[%u] target 0x%08X is outside the ROM", name, i, gbaAddress);
            return 0;
        }
    }
    return 1;
}

int Port_ValidateRomProfile(const PortRomProfile* profile, const RomOffsets* offsets, const uint8_t* romData,
                            size_t romSize, char* error, size_t errorSize) {
    struct NamedOffset {
        const char* name;
        uint32_t value;
    };
    struct NamedOffset required[] = {
        { "gfxAndPalettes", offsets ? offsets->gfxAndPalettes : 0 },
        { "gfxGroups", offsets ? offsets->gfxGroups : 0 },
        { "paletteGroups", offsets ? offsets->paletteGroups : 0 },
        { "objPalettes", offsets ? offsets->objPalettes : 0 },
        { "frameObjLists", offsets ? offsets->frameObjLists : 0 },
        { "extraFrameOffsets", offsets ? offsets->extraFrameOffsets : 0 },
        { "fixedTypeGfx", offsets ? offsets->fixedTypeGfx : 0 },
        { "spritePtrs", offsets ? offsets->spritePtrs : 0 },
        { "collisionMatrix", offsets ? offsets->collisionMatrix : 0 },
        { "collisionShapePtrs", offsets ? offsets->collisionShapePtrs : 0 },
        { "tileTypeProperties", offsets ? offsets->tileTypeProperties : 0 },
        { "figurines", offsets ? offsets->figurines : 0 },
        { "fuserEnemyData", offsets ? offsets->fuserEnemyData : 0 },
        { "fuserNpcData", offsets ? offsets->fuserNpcData : 0 },
        { "fusionTextPtrs", offsets ? offsets->fusionTextPtrs : 0 },
        { "fuserFusionPtrs", offsets ? offsets->fuserFusionPtrs : 0 },
        { "lakeHyliaEnemies", offsets ? offsets->lakeHyliaEnemies : 0 },
        { "lakeHyliaCleared", offsets ? offsets->lakeHyliaCleared : 0 },
        { "lilypadRails", offsets ? offsets->lilypadRails : 0 },
        { "songTable", offsets ? offsets->songTable : 0 },
        { "translations", offsets ? offsets->translations : 0 },
        { "text09230", offsets ? offsets->text09230 : 0 },
        { "text09244", offsets ? offsets->text09244 : 0 },
        { "text09248", offsets ? offsets->text09248 : 0 },
        { "text0926C", offsets ? offsets->text0926C : 0 },
        { "text092AC", offsets ? offsets->text092AC : 0 },
        { "text092D4", offsets ? offsets->text092D4 : 0 },
        { "text0942E", offsets ? offsets->text0942E : 0 },
        { "text094CE", offsets ? offsets->text094CE : 0 },
        { "uiData", offsets ? offsets->uiData : 0 },
        { "fadeData", offsets ? offsets->fadeData : 0 },
        { "overlaySizeTable", offsets ? offsets->overlaySizeTable : 0 },
        { "mapDataBase", offsets ? offsets->mapDataBase : 0 },
        { "areaRoomHeaders", offsets ? offsets->areaRoomHeaders : 0 },
        { "areaTileSets", offsets ? offsets->areaTileSets : 0 },
        { "areaRoomMaps", offsets ? offsets->areaRoomMaps : 0 },
        { "areaTable", offsets ? offsets->areaTable : 0 },
        { "areaTiles", offsets ? offsets->areaTiles : 0 },
        { "exitLists", offsets ? offsets->exitLists : 0 },
        { "bgAnimTable", offsets ? offsets->bgAnimTable : 0 },
        { "localFlagBanks", offsets ? offsets->localFlagBanks : 0 },
        { "townspersonSpriteLoadPtrs", offsets ? offsets->townspersonSpriteLoadPtrs : 0 },
        { "guardPatrolData", offsets ? offsets->guardPatrolData : 0 },
        { "innWestEntities", offsets ? offsets->innWestEntities : 0 },
        { "innMiddleEntities", offsets ? offsets->innMiddleEntities : 0 },
        { "innEastEntities", offsets ? offsets->innEastEntities : 0 },
        { "simonEntityLists", offsets ? offsets->simonEntityLists : 0 },
        { "simonEnemyPatterns", offsets ? offsets->simonEnemyPatterns : 0 },
        { "simonChestPatterns", offsets ? offsets->simonChestPatterns : 0 },
        { "gustJarAnimTable", offsets ? offsets->gustJarAnimTable : 0 },
        { "gustJarHitbox", offsets ? offsets->gustJarHitbox : 0 },
    };
    uint32_t glyphTableOffset;
    uint32_t remapOffset;
    uint32_t glyphBankCount;
    uint32_t remapSize;

    if (error != NULL && errorSize != 0) {
        error[0] = '\0';
    }
    if (offsets == NULL || romData == NULL) {
        SetError(error, errorSize, "ROM profile validation received no ROM or offset table");
        return 0;
    }
    if (profile == NULL) {
        SetError(error, errorSize, "ROM identity is not a supported exact profile");
        return 0;
    }
    if (profile != NULL && profile->expectedSize != 0 && romSize != profile->expectedSize) {
        SetError(error, errorSize, "%s has size %zu; expected %zu", profile->displayName, romSize,
                 profile->expectedSize);
        return 0;
    }
    if (offsets->expectedRomSize > romSize || romSize < 0xB0u) {
        SetError(error, errorSize, "ROM is truncated (%zu bytes)", romSize);
        return 0;
    }
    if (memcmp(romData + 0xACu, offsets->gameCode, 4) != 0 ||
        (profile != NULL && memcmp(romData + 0xACu, profile->gameCode, 4) != 0)) {
        SetError(error, errorSize, "ROM game code does not match the selected profile");
        return 0;
    }
    for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
        if (!ValidateOffset(required[i].name, required[i].value, romSize, error, errorSize)) {
            return 0;
        }
    }

    glyphTableOffset = profile != NULL && profile->glyphTableOffset != 0 ? profile->glyphTableOffset
                                                                          : offsets->text09248;
    remapOffset = profile != NULL && profile->textRemapOffset != 0 ? profile->textRemapOffset : offsets->text092D4;
    glyphBankCount = profile != NULL ? profile->glyphBankCount : 9u;
    remapSize = profile != NULL ? profile->textRemapSize : RETAIL_TEXT_REMAP_SIZE;
    if (glyphBankCount == 0 || glyphBankCount > PORT_MAX_GLYPH_BANKS) {
        SetError(error, errorSize, "glyph bank count %u is invalid", glyphBankCount);
        return 0;
    }
    if (remapSize > PORT_TEXT_REMAP_CAPACITY || remapOffset > romSize || remapSize > romSize - remapOffset) {
        SetError(error, errorSize, "text remap table at 0x%X is truncated", remapOffset);
        return 0;
    }
    if (!ValidatePackedPointerTable("glyphTable", romData, romSize, glyphTableOffset, glyphBankCount, 64, error,
                                    errorSize) ||
        !ValidatePackedPointerTable("translations", romData, romSize, offsets->translations, 1, 4, error,
                                    errorSize) ||
        !ValidatePackedPointerTable("collisionShapePtrs", romData, romSize, offsets->collisionShapePtrs, 40, 32,
                                    error, errorSize) ||
        !ValidatePackedPointerTable("fusionTextPtrs", romData, romSize, offsets->fusionTextPtrs,
                                    PORT_FUSER_TABLE_COUNT, 6, error, errorSize) ||
        !ValidatePackedPointerTable("fuserFusionPtrs", romData, romSize, offsets->fuserFusionPtrs,
                                    PORT_FUSER_TABLE_COUNT, 6,
                                    error, errorSize)) {
        return 0;
    }
    return 1;
}
