#include "gui/pages/home_page.hpp"

namespace gui::pages {

void HomePage::create() {
    lv_obj_set_flex_flow(parent_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title = lv_label_create(parent_);
    lv_label_set_text(title, "MultiAIQuota");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    status_label_ = lv_label_create(parent_);
    lv_label_set_text(status_label_, state_.wifi_status.c_str());
}

void HomePage::update() {
    if (status_label_) {
        lv_label_set_text(status_label_, state_.wifi_status.c_str());
    }
}

} // namespace gui::pages
