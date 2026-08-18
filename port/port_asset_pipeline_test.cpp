#include "port_asset_pipeline.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures;

void Check(bool condition, const char* expression) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", expression);
        ++failures;
    }
}

} // namespace

int main() {
    /* SP4 glyphs use one payload byte for the low banks and two payload bytes
     * once the 0x0F prefix selects an injected bank. The symbolic form must
     * preserve both forms so editing and rebuilding cannot shift the stream. */
    const std::vector<uint8_t> sp4Bytes = {
        0x41,                         // A
        0x0B, 0x34,                   // one-byte glyph
        0x0F, 0x3F,                  // one-byte glyph at the threshold boundary
        0x0F, 0x40, 0xA5,             // two-byte glyph
        0x00,
    };
    std::string symbolic;
    std::string error;
    size_t consumed = 0;
    Check(PortAssetPipeline::DecodeTmcText(sp4Bytes.data(), sp4Bytes.size(), symbolic, &consumed, &error,
                                           PORT_TEXT_CODEC_ANGEL_SP4),
          "SP4 text decodes");
    Check(consumed == sp4Bytes.size(), "SP4 decoder consumes the terminator");
    Check(symbolic == "A{0B:34}{0F:3F}{0F:40:A5}", "SP4 symbolic form preserves variable glyphs");

    std::vector<uint8_t> roundTrip;
    Check(PortAssetPipeline::EncodeTmcText(symbolic, roundTrip, &error), "SP4 symbolic form encodes");
    Check(roundTrip == sp4Bytes, "SP4 decode/encode round trip is byte exact");

    roundTrip.clear();
    Check(PortAssetPipeline::EncodeTmcText("{Color:Green}{0F:40:A5}", roundTrip, &error,
                                           PORT_TEXT_CODEC_ANGEL_SP4),
          "SP4 editable text accepts explicit glyph tokens");
    Check(roundTrip == std::vector<uint8_t>({0x02, 0x02, 0x0F, 0x40, 0xA5, 0x00}),
          "SP4 editable text keeps control/glyph byte order");
    roundTrip.clear();
    Check(!PortAssetPipeline::EncodeTmcText("{Key:A}", roundTrip, &error, PORT_TEXT_CODEC_ANGEL_SP4),
          "SP4 rejects the retail Key command alias");

    /* Retail keeps 0x0D as the historical carriage-return mapping and 0x0F
     * as the named Symbol command. This guards the codec split from changing
     * existing USA/EU/clean-JP editable assets. */
    const std::vector<uint8_t> retailBytes = {0x0D, 0x0F, 0x34, 0x00};
    symbolic.clear();
    Check(PortAssetPipeline::DecodeTmcText(retailBytes.data(), retailBytes.size(), symbolic, nullptr, &error,
                                           PORT_TEXT_CODEC_RETAIL),
          "retail text decodes");
    Check(symbolic == "\r{Symbol:34}", "retail control semantics remain unchanged");

    const std::vector<uint8_t> delimiterBytes = {'\\', '{', '}', 0x00};
    symbolic.clear();
    Check(PortAssetPipeline::DecodeTmcText(delimiterBytes.data(), delimiterBytes.size(), symbolic, nullptr, &error,
                                           PORT_TEXT_CODEC_RETAIL),
          "literal editable-text delimiters decode");
    Check(symbolic == "\\\\\\{\\}", "literal editable-text delimiters are escaped");
    roundTrip.clear();
    Check(PortAssetPipeline::EncodeTmcText(symbolic, roundTrip, &error), "escaped delimiters encode");
    Check(roundTrip == delimiterBytes, "escaped delimiter round trip is byte exact");

    if (failures != 0) {
        std::fprintf(stderr, "port_asset_pipeline_test: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("port_asset_pipeline_test: ALL PASS");
    return 0;
}
