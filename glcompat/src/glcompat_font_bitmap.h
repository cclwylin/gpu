// Sprint 42 — embedded 8×13 ASCII bitmap font.
//
// Each glyph is 13 rows of 8 bits (left = MSB). Rows are top-to-bottom
// when stored, but glBitmap expects them bottom-up; the font_8x13_*
// functions handle flip at render time.
//
// Coverage: ASCII 0x20 (space) … 0x7E (~). Outside this range emits a
// blank cell (advance only).

#pragma once
#include <cstdint>

namespace glcompat::font {

// 95 glyphs × 13 rows of 1 byte each.
extern const uint8_t bitmap_8x13[95][13];

constexpr int kBitmap8x13_W       = 8;
constexpr int kBitmap8x13_H       = 13;
constexpr int kBitmap8x13_Advance = 8;

}  // namespace glcompat::font
