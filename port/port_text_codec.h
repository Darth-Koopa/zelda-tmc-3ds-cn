#pragma once

#include <stdint.h>

typedef enum {
    PORT_TEXT_CODEC_RETAIL = 0,
    PORT_TEXT_CODEC_ANGEL_SP4,
} PortTextCodec;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t Port_TextGlyphPayloadSize(PortTextCodec codec, uint32_t prefix, uint32_t firstByte);
uint32_t Port_TextDecodeGlyph(PortTextCodec codec, uint32_t prefix, uint32_t firstByte, uint32_t secondByte);

#ifdef __cplusplus
}
#endif
