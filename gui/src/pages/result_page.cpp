#include "gui/pages/result_page.hpp"

#include "maiq/types.hpp"

#include <cstdio>
#include <string>

namespace gui::pages {

void ResultPage::create() {
    lv_obj_set_flex_flow(parent_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent_, 4, 0);

    lv_obj_t* title = lv_label_create(parent_);
    lv_label_set_text(title, "Results");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    container_ = lv_obj_create(parent_);
    lv_obj_set_size(container_, LV_PCT(100), LV_PCT(80));
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 2, 0);
    lv_obj_set_style_pad_row(container_, 2, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_AUTO);

    update();
}

void ResultPage::update() {
    if (!container_) return;
    lv_obj_clean(container_);

    if (state_.querying) {
        lv_obj_t* lbl = lv_label_create(container_);
        lv_label_set_text(lbl, "Querying...");
        return;
    }
    if (state_.statuses.empty()) {
        lv_obj_t* lbl = lv_label_create(container_);
        lv_label_set_text(lbl, "No accounts.");
        return;
    }

    for (const auto& s : state_.statuses) {
        lv_obj_t* card = lv_obj_create(container_);
        lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(card, 2, 0);
        lv_obj_set_style_pad_row(card, 1, 0);

        lv_obj_t* header = lv_label_create(card);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s (%s)", s.account_name.c_str(), to_string(s.vendor).c_str());
        lv_label_set_text(header, buf);
        lv_obj_set_style_text_font(header, &lv_font_montserrat_12, 0);

        if (!s.is_valid) {
            lv_obj_t* err = lv_label_create(card);
            lv_label_set_text(err, s.invalid_message ? s.invalid_message->c_str() : "error");
            lv_obj_set_style_text_font(err, &lv_font_montserrat_12, 0);
            continue;
        }

        for (const auto& e : s.entries) {
            lv_obj_t* line = lv_label_create(card);
            char line_buf[128];
            if (e.remaining) {
                std::snprintf(line_buf, sizeof(line_buf), "%s: %.1f %s",
                              e.name.c_str(), *e.remaining, e.unit.c_str());
            } else {
                std::snprintf(line_buf, sizeof(line_buf), "%s: -", e.name.c_str());
            }
            lv_label_set_text(line, line_buf);
            lv_obj_set_style_text_font(line, &lv_font_montserrat_12, 0);
        }
    }
}

} // namespace gui::pages
