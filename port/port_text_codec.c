#include "port_text_codec.h"

uint32_t Port_TextGlyphPayloadSize(PortTextCodec codec, uint32_t prefix, uint32_t firstByte) {
    if (prefix < 0x0Bu || prefix > 0x0Fu) {
        return 0;
    }
    if (codec == PORT_TEXT_CODEC_ANGEL_SP4 && prefix == 0x0Fu && (firstByte & 0xFFu) >= 0x40u) {
        return 2;
    }
    return 1;
}

uint32_t Port_TextDecodeGlyph(PortTextCodec codec, uint32_t prefix, uint32_t firstByte, uint32_t secondByte) {
    firstByte &= 0xFFu;
    secondByte &= 0xFFu;
    if (codec == PORT_TEXT_CODEC_ANGEL_SP4) {
        if (prefix >= 0x0Bu && prefix <= 0x0Eu) {
            return (((prefix & 7u) + 1u) << 8) | firstByte;
        }
        if (prefix == 0x0Fu) {
            if (firstByte <= 0x2Fu) {
                return 0x200u | firstByte;
            }
            if (firstByte <= 0x3Fu) {
                return 0x300u | (firstByte & 0x0Fu);
            }
            /* The patch stores the bank in a four-bit field. Values after
             * bank 15 wrap to bank 0, matching the lookup path's nibble mask
             * and keeping GetCharacter's extended-bank checks consistent. */
            return ((((firstByte & 0x0Fu) + 9u) & 0x0Fu) << 8) | secondByte;
        }
        return 0;
    }

    switch (prefix) {
        case 0x0B:
            return 0x400u | firstByte;
        case 0x0C:
            return 0x700u | firstByte;
        case 0x0D:
            return 0x500u | firstByte;
        case 0x0E:
            return 0x600u | firstByte;
        case 0x0F:
            return 0x300u | firstByte;
        default:
            return 0;
    }
}
