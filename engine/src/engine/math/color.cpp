#include "color.h"

namespace my {

uint32_t Color::ToRgb() const {
    uint32_t c = (uint8_t)std::round(r * 255.0f);
    c <<= 8;
    c |= (uint8_t)std::round(g * 255.0f);
    c <<= 8;
    c |= (uint8_t)std::round(b * 255.0f);
    return c;
}

uint32_t Color::ToRgba() const {
    uint32_t c = (uint8_t)std::round(r * 255.0f);
    c <<= 8;
    c |= (uint8_t)std::round(g * 255.0f);
    c <<= 8;
    c |= (uint8_t)std::round(b * 255.0f);
    c <<= 8;
    c |= (uint8_t)std::round(a * 255.0f);
    return c;
}

// Color Color::Hex(uint32_t hex) {
//     float b = (hex & 0xFF) / 255.0f;
//     hex >>= 8;
//     float g = (hex & 0xFF) / 255.0f;
//     hex >>= 8;
//     float r = (hex & 0xFF) / 255.0f;
//     return Color(r, g, b, 1.0f);
// }

Color Color::HexRgba(uint32_t hex) {
    float a = (hex & 0xFF) / 255.0f;
    hex >>= 8;
    float b = (hex & 0xFF) / 255.0f;
    hex >>= 8;
    float g = (hex & 0xFF) / 255.0f;
    hex >>= 8;
    float r = (hex & 0xFF) / 255.0f;
    return Color(r, g, b, a);
}

}  // namespace my
