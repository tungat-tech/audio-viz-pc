#include <lvgl.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

LV_FONT_DECLARE(lv_font_montserrat_vietnamese_12);
LV_FONT_DECLARE(lv_font_montserrat_vietnamese_14);

namespace {
void requireGlyph(const lv_font_t* font, std::uint32_t codepoint) {
    lv_font_glyph_dsc_t descriptor{};
    if (!lv_font_get_glyph_dsc(font, &descriptor, codepoint, 0)) {
        throw std::runtime_error("Vietnamese font is missing codepoint " +
                                 std::to_string(codepoint));
    }
}
}

int main() {
    // Representative Vietnamese glyphs: đ, ơ, ư, ế, ệ, ợ, ữ and combining
    // acute/dot-below marks used by media metadata that is not precomposed.
    constexpr std::array<std::uint32_t, 9> glyphs{
        0x0111, 0x01A1, 0x01B0, 0x1EBF, 0x1EC7,
        0x1EE3, 0x1EEF, 0x0301, 0x0323};
    for (const auto codepoint : glyphs) {
        requireGlyph(&lv_font_montserrat_vietnamese_12, codepoint);
        requireGlyph(&lv_font_montserrat_vietnamese_14, codepoint);
    }
    std::cout << "Vietnamese font glyph coverage passed\n";
}
