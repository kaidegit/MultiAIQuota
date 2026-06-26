#pragma once

#include "gui/app_state.hpp"
#include "gui/display_spec.hpp"

#include <lvgl.h>

namespace gui::pages {

class Page {
public:
    Page(lv_obj_t* parent, const AppState& state, const DisplaySpec& spec)
        : parent_(parent), state_(state), spec_(spec) {}
    virtual ~Page() = default;

    virtual void create() = 0;
    virtual void update() {}

protected:
    lv_obj_t* parent_;
    const AppState& state_;
    const DisplaySpec& spec_;
};

} // namespace gui::pages
