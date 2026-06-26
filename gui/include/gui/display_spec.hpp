#pragma once

#include <cstdint>

namespace gui {

struct DisplaySpec {
    uint16_t width;
    uint16_t height;
    bool landscape;
    uint16_t safe_inset;
    uint16_t header_h;
    uint16_t footer_h;
};

constexpr DisplaySpec XUEERSI_SPEC{160, 128, true, 8, 18, 24};

} // namespace gui
