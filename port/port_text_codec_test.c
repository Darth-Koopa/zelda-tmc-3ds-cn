#include "port_text_codec.h"

#include <stdio.h>

static int sFailures;

#define CHECK_EQ(actual, expected)                                                                                     \
    do {                                                                                                               \
        unsigned int got_ = (unsigned int)(actual);                                                                    \
        unsigned int expected_ = (unsigned int)(expected);                                                             \
        if (got_ != expected_) {                                                                                       \
            fprintf(stderr, "FAIL %s:%d: 0x%X != 0x%X\n", __FILE__, __LINE__, got_, expected_);                     \
            ++sFailures;                                                                                               \
        }                                                                                                              \
    } while (0)

int main(void) {
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_RETAIL, 0x0B, 0x34, 0), 0x434);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_RETAIL, 0x0C, 0x34, 0), 0x734);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_RETAIL, 0x0F, 0x34, 0), 0x334);

    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0B, 0x34, 0), 0x434);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0C, 0x34, 0), 0x534);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0E, 0x34, 0), 0x734);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x2F, 0), 0x22F);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x30, 0), 0x300);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x3F, 0), 0x30F);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x40, 0xA5), 0x9A5);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x43, 0x7E), 0xC7E);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x47, 0xA5), 0x0A5);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x48, 0xA5), 0x1A5);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x4F, 0xA5), 0x8A5);
    CHECK_EQ(Port_TextDecodeGlyph(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0xFF, 0xA5), 0x8A5);
    CHECK_EQ(Port_TextGlyphPayloadSize(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x3F), 1);
    CHECK_EQ(Port_TextGlyphPayloadSize(PORT_TEXT_CODEC_ANGEL_SP4, 0x0F, 0x40), 2);
    CHECK_EQ(Port_TextGlyphPayloadSize(PORT_TEXT_CODEC_RETAIL, 0x0F, 0x40), 1);

    if (sFailures != 0) {
        return 1;
    }
    puts("port_text_codec_test: ALL PASS");
    return 0;
}
