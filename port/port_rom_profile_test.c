#include "port_rom_profile.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sFailures;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                     \
            ++sFailures;                                                                                               \
        }                                                                                                              \
    } while (0)

static void SetHashes(PortRomHashes* hashes, size_t size, const char* sha1, const char* sha256) {
    memset(hashes, 0, sizeof(*hashes));
    hashes->size = size;
    snprintf(hashes->sha1, sizeof(hashes->sha1), "%s", sha1);
    snprintf(hashes->sha256, sizeof(hashes->sha256), "%s", sha256);
}

static void WriteU32(unsigned char* data, size_t offset, unsigned int value) {
    data[offset] = (unsigned char)value;
    data[offset + 1] = (unsigned char)(value >> 8);
    data[offset + 2] = (unsigned char)(value >> 16);
    data[offset + 3] = (unsigned char)(value >> 24);
}

static void FillSyntheticOffsets(RomOffsets* offsets) {
    unsigned int value = 0x100u;
    memset(offsets, 0, sizeof(*offsets));
    for (size_t byte = 0; byte < offsetof(RomOffsets, gfxGroupsCount); byte += sizeof(value)) {
        memcpy((unsigned char*)offsets + byte, &value, sizeof(value));
    }
    offsets->gfxGroupsCount = 1;
    offsets->paletteGroupsCount = 1;
    offsets->objPalettesCount = 1;
    offsets->frameObjListsSize = 1;
    offsets->fixedTypeGfxCount = 1;
    offsets->spritePtrsCount = 1;
    offsets->expectedRomSize = 0x1000000u;
    memcpy(offsets->gameCode, "BZMJ", 5);
}

static void TestHashVectors(void) {
    PortRomHashes hashes;
    Port_HashBuffer("", 0, &hashes);
    CHECK(strcmp(hashes.sha1, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0);
    CHECK(strcmp(hashes.sha256, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);

    Port_HashBuffer("abc", 3, &hashes);
    CHECK(strcmp(hashes.sha1, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
    CHECK(strcmp(hashes.sha256, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
}

static void TestFailedIdentityOutputs(void) {
    PortRomHashes hashes;
    memset(&hashes, 0xA5, sizeof(hashes));
    CHECK(Port_IdentifyRomBuffer(NULL, 0, &hashes) == NULL);
    CHECK(hashes.size == 0);
    CHECK(hashes.sha1[0] == '\0' && hashes.sha256[0] == '\0');

    memset(&hashes, 0xA5, sizeof(hashes));
    CHECK(Port_IdentifyRomFile("/tmp/tmc-rom-profile-test-file-does-not-exist.gba", &hashes) == NULL);
    CHECK(hashes.size == 0);
    CHECK(hashes.sha1[0] == '\0' && hashes.sha256[0] == '\0');
}

static const PortRomProfile* TestProfileIdentity(void) {
    PortRomHashes hashes;
    const PortRomProfile* profile;
    const PortRomProfile* sp4;
    SetHashes(&hashes, 0x1000000u, "ba04cfbe93d12d2ad684c52234472fa12a5b53d7",
              "f51c6c2f90e18ee91203dd767307271e06901b5bff35c3a567d52f61a39d166d");
    profile = Port_IdentifyRomHashes(&hashes, "BZMJ");
    CHECK(profile != NULL);
    CHECK(profile != NULL && profile->variant == PORT_ROM_VARIANT_JP_ANGEL_SP4);
    CHECK(profile != NULL && profile->glyphTableOffset == 0xDC9F00u);
    CHECK(profile != NULL && profile->textRemapOffset == 0xE4F000u);
    CHECK(profile != NULL && profile->textRemapSize == 0xA0u);
    sp4 = profile;

    Port_SetActiveRomProfile(profile);
    CHECK(Port_GetTextCodec() == PORT_TEXT_CODEC_ANGEL_SP4);
    CHECK(Port_GetGlyphBankCount() == 16u);
    CHECK(Port_GetWideGlyphFirstBank() == 3u);
    CHECK(Port_GetSpecialPaletteBank() == 3u);
    CHECK(strcmp(Port_GetVariantSaveFilename(), "tmc_jp_angel_sp4.sav") == 0);
    CHECK(strcmp(Port_GetAssetCacheSubdir(), "jp-angel-sp4") == 0);

    hashes.sha256[0] = '0';
    CHECK(Port_IdentifyRomHashes(&hashes, "BZMJ") == NULL);
    CHECK(Port_IdentifyRomHashes(&hashes, "BZME") == NULL);

    SetHashes(&hashes, 0, "63fcad218f9047b6a9edbb68c98bd0dec322d7a1", "");
    profile = Port_IdentifyRomHashes(&hashes, "BZHE");
    CHECK(profile != NULL && profile->variant == PORT_ROM_VARIANT_USA_DEMO);
    CHECK(profile != NULL && !Port_RomProfileIsPlayable(profile));
    Port_SetActiveRomProfile(NULL);
    CHECK(Port_GetTextCodec() == PORT_TEXT_CODEC_RETAIL);
    CHECK(Port_GetGlyphBankCount() == 9u);
    CHECK(strcmp(Port_GetAssetCacheSubdir(), "usa") == 0);
    return sp4;
}

static void TestProfileValidation(const PortRomProfile* sp4) {
    const size_t romSize = 0x1000000u;
    unsigned char* rom = (unsigned char*)calloc(1, romSize);
    RomOffsets offsets;
    char error[256];
    CHECK(rom != NULL);
    if (rom == NULL) {
        return;
    }
    FillSyntheticOffsets(&offsets);
    memcpy(rom + 0xAC, "BZMJ", 4);
    for (unsigned int i = 0; i < PORT_FUSER_TABLE_COUNT; ++i) {
        WriteU32(rom, 0x100u + i * 4u, 0x08000200u);
    }
    for (unsigned int i = 0; i < 16; ++i) {
        WriteU32(rom, 0xDC9F00u + i * 4u, 0x08000200u);
    }
    CHECK(Port_ValidateRomProfile(sp4, &offsets, rom, romSize, error, sizeof(error)) == 1);
    CHECK(Port_ValidateRomProfile(NULL, &offsets, rom, romSize, error, sizeof(error)) == 0);
    CHECK(strstr(error, "exact profile") != NULL);
    offsets.collisionMatrix = 0;
    CHECK(Port_ValidateRomProfile(sp4, &offsets, rom, romSize, error, sizeof(error)) == 0);
    CHECK(strstr(error, "collisionMatrix") != NULL);
    free(rom);
}

static void TestRealRom(const char* path, PortRomVariant expected, const char* expectedCacheSubdir) {
    PortRomHashes hashes;
    const PortRomProfile* profile = Port_IdentifyRomFile(path, &hashes);
    CHECK(profile != NULL);
    CHECK(profile != NULL && profile->variant == expected);
    CHECK(hashes.size == 0x1000000u);
    Port_SetActiveRomProfile(profile);
    CHECK(strcmp(Port_GetAssetCacheSubdir(), expectedCacheSubdir) == 0);
}

int main(int argc, char** argv) {
    const PortRomProfile* sp4;
    TestHashVectors();
    TestFailedIdentityOutputs();
    sp4 = TestProfileIdentity();
    TestProfileValidation(sp4);
    if (argc == 3) {
        TestRealRom(argv[1], PORT_ROM_VARIANT_JP_RETAIL, "jp");
        TestRealRom(argv[2], PORT_ROM_VARIANT_JP_ANGEL_SP4, "jp-angel-sp4");
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [clean-jp.gba angel-sp4.gba]\n", argv[0]);
        return 2;
    }
    if (sFailures != 0) {
        fprintf(stderr, "port_rom_profile_test: %d failure(s)\n", sFailures);
        return 1;
    }
    puts("port_rom_profile_test: ALL PASS");
    return 0;
}
