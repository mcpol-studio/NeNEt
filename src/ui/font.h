#pragma once

#include <cstdint>

namespace nenet {

constexpr int kFontWidth = 5;
constexpr int kFontHeight = 7;
constexpr int kFontAdvance = 6;

[[nodiscard]] const uint8_t* getCharBitmap(char c) noexcept;

}
