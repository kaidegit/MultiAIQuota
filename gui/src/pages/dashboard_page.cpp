#include "gui/pages/dashboard_page.hpp"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>

#ifdef ESP_PLATFORM
#include <esp_log.h>
#else
#define ESP_LOGI(tag, fmt, ...) do { printf("I (%s): " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#endif

namespace gui::pages {

namespace {

const lv_color_t COLOR_BG = lv_color_hex(0x000000);
const lv_color_t COLOR_CARD_BG = lv_color_hex(0x000000);
const lv_color_t COLOR_CARD_BORDER = lv_color_hex(0x4b5563);
const lv_color_t COLOR_BAR_BG = lv_color_hex(0x000000);
const lv_color_t COLOR_BAR_PINK = lv_color_hex(0xf472b6);
const lv_color_t COLOR_BAR_GREEN = lv_color_hex(0x4ade80);
const lv_color_t COLOR_TEXT_DIM = lv_color_hex(0x9ca3af);
const lv_color_t COLOR_WIFI_ON = lv_color_hex(0x4ade80);
const lv_color_t COLOR_WIFI_OFF = lv_color_hex(0xef4444);

std::string format_reset_in(time_t reset_at) {
    time_t now = std::time(nullptr);
    double diff = std::difftime(reset_at, now);
    if (diff <= 0) return "resets now";

    // If the system clock is not set (e.g., fresh ESP32 boot without SNTP),
    // diff can be decades. Show absolute local time instead of a bogus countdown.
    if (diff > 365 * 86400) {
        char buf[32];
        struct tm tm_info;
        localtime_r(&reset_at, &tm_info);
        std::strftime(buf, sizeof(buf), "Reset at %m-%d %H:%M", &tm_info);
        return buf;
    }

    int64_t seconds = static_cast<int64_t>(diff);
    int64_t days = seconds / 86400;
    seconds %= 86400;
    int64_t hours = seconds / 3600;
    seconds %= 3600;
    int64_t minutes = seconds / 60;

    char buf[64];
    if (days > 0) {
        std::snprintf(buf, sizeof(buf), "Resets in %lldd %lldh", days, hours);
    } else if (hours > 0) {
        std::snprintf(buf, sizeof(buf), "Resets in %lldh %lldm", hours, minutes);
    } else {
        std::snprintf(buf, sizeof(buf), "Resets in %lldm", minutes);
    }
    return buf;
}

std::string format_refresh_time(std::time_t t) {
    char buf[32];
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    std::strftime(buf, sizeof(buf), "%H:%M", &tm_info);
    return std::string(buf);
}

} // namespace

void DashboardPage::create() {
    ESP_LOGI("dashboard", "create: state_ addr=%p", static_cast<const void*>(&state_));

    lv_obj_set_flex_flow(parent_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent_, 2, 0);
    lv_obj_set_style_pad_row(parent_, 1, 0);
    lv_obj_set_style_bg_color(parent_, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(parent_, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(parent_, LV_SCROLLBAR_MODE_OFF);

    create_header();
    create_cards_area();
    create_footer();
    update();
}

void DashboardPage::create_header() {
    lv_obj_t* header = lv_obj_create(parent_);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_pad_column(header, 2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);

    lv_obj_t* logo = lv_label_create(header);
    lv_label_set_text(logo, "*");
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(logo, COLOR_BAR_PINK, 0);

    header_name_ = lv_label_create(header);
    lv_obj_set_flex_grow(header_name_, 1);
    lv_obj_set_style_text_align(header_name_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(header_name_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(header_name_, lv_color_white(), 0);
    lv_label_set_long_mode(header_name_, LV_LABEL_LONG_DOT);

    header_wifi_ = lv_label_create(header);
    lv_label_set_text(header_wifi_, "o");
    lv_obj_set_style_text_font(header_wifi_, &lv_font_montserrat_10, 0);
}

void DashboardPage::create_cards_area() {
    cards_container_ = lv_obj_create(parent_);
    lv_obj_set_width(cards_container_, LV_PCT(100));
    lv_obj_set_height(cards_container_, 0);
    lv_obj_set_flex_grow(cards_container_, 1);
    lv_obj_set_flex_flow(cards_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cards_container_, 0, 0);
    lv_obj_set_style_pad_row(cards_container_, 3, 0);
    lv_obj_set_style_border_width(cards_container_, 0, 0);
    lv_obj_set_style_bg_opa(cards_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(cards_container_, LV_SCROLLBAR_MODE_OFF);
}

void DashboardPage::create_footer() {
    lv_obj_t* footer = lv_obj_create(parent_);
    lv_obj_set_size(footer, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_set_style_pad_column(footer, 2, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);

    footer_dots_ = lv_label_create(footer);
    lv_obj_set_flex_grow(footer_dots_, 1);
    lv_obj_set_style_text_align(footer_dots_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer_dots_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(footer_dots_, lv_color_white(), 0);

    footer_time_ = lv_label_create(footer);
    lv_obj_set_style_text_font(footer_time_, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(footer_time_, COLOR_TEXT_DIM, 0);
}

void DashboardPage::update() {
    ESP_LOGI("dashboard", "update: statuses=%zu querying=%d",
             state_.statuses.size(), static_cast<int>(state_.querying));
    update_header();

    if (!cards_container_) return;
    lv_obj_clean(cards_container_);

    if (state_.querying) {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, "Divining...");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        update_footer();
        return;
    }
    if (state_.statuses.empty()) {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, "No accounts.");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        update_footer();
        return;
    }

    const auto& s = state_.statuses[state_.selected_account_index];
    if (!s.is_valid) {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, s.invalid_message ? s.invalid_message->c_str() : "error");
        lv_obj_set_style_text_color(lbl, COLOR_WIFI_OFF, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        update_footer();
        return;
    }

    if (s.mode == maiq::QueryMode::Balance) {
        render_balance(s);
    } else {
        render_subscription(s);
    }

    update_footer();
}

void DashboardPage::update_header() {
    if (!header_name_ || !header_wifi_) return;

    if (state_.statuses.empty()) {
        lv_label_set_text(header_name_, "MultiAIQuota");
    } else {
        const auto& s = state_.statuses[state_.selected_account_index];
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s (%s)",
                      s.account_name.c_str(), maiq::to_string(s.vendor).c_str());
        lv_label_set_text(header_name_, buf);
    }

    lv_obj_set_style_text_color(header_wifi_, state_.wifi_connected() ? COLOR_WIFI_ON : COLOR_WIFI_OFF, 0);
}

void DashboardPage::update_footer() {
    if (!footer_dots_ || !footer_time_) return;

    std::string dots;
    for (size_t i = 0; i < state_.statuses.size(); ++i) {
        dots += (i == state_.selected_account_index) ? "* " : "o ";
    }
    lv_label_set_text(footer_dots_, dots.c_str());

    if (state_.last_refresh_at) {
        std::string txt = "Updated " + format_refresh_time(*state_.last_refresh_at);
        lv_label_set_text(footer_time_, txt.c_str());
    } else {
        lv_label_set_text(footer_time_, "--:--");
    }
}

void DashboardPage::render_subscription(const maiq::AccountStatus& s) {
    auto* short_entry = pick_short_quota(s);
    auto* week_entry = pick_week_quota(s);

    if (short_entry) {
        make_card("Current", *short_entry, true);
    } else {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, "No short quota");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    }

    if (week_entry) {
        make_card("Weekly", *week_entry, true);
    } else {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, "No weekly quota");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    }
}

void DashboardPage::render_balance(const maiq::AccountStatus& s) {
    auto* daily = pick_daily_used(s);
    auto* remaining = pick_remaining(s);

    if (daily) {
        make_card("Today", *daily, false);
    } else {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, "No daily usage");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    }

    if (remaining) {
        make_card("Balance", *remaining, false);
    } else {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, "No balance");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    }
}

lv_obj_t* DashboardPage::make_card(const char* label, const maiq::QuotaEntry& entry,
                                   bool is_subscription) {
    lv_obj_t* card = lv_obj_create(cards_container_);
    lv_obj_set_size(card, LV_PCT(100), 43);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 1, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 3, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    lv_obj_t* top = lv_obj_create(card);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_style_pad_column(top, 4, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);

    lv_obj_t* value = lv_label_create(top);
    lv_obj_set_flex_grow(value, 1);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(value, lv_color_white(), 0);

    double pct = 0.0;
    if (is_subscription) {
        if (entry.total && *entry.total > 0 && entry.remaining) {
            pct = (*entry.remaining / *entry.total) * 100.0;
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(pct));
        lv_label_set_text(value, buf);
    } else {
        char buf[64];
        if (entry.used) {
            std::snprintf(buf, sizeof(buf), "%.1f", *entry.used);
            pct = (entry.total && *entry.total > 0) ? (*entry.used / *entry.total * 100.0) : 0.0;
        } else if (entry.remaining) {
            std::snprintf(buf, sizeof(buf), "%.1f", *entry.remaining);
            pct = (entry.total && *entry.total > 0) ? (*entry.remaining / *entry.total * 100.0) : 0.0;
        } else {
            std::snprintf(buf, sizeof(buf), "-");
        }
        lv_label_set_text(value, buf);
    }

    lv_obj_t* pill = lv_obj_create(top);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(pill, 1, 0);
    lv_obj_set_style_pad_hor(pill, 5, 0);
    lv_obj_set_style_radius(pill, 3, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_bg_color(pill, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);

    lv_obj_t* pill_lbl = lv_label_create(pill);
    lv_label_set_text(pill_lbl, label);
    lv_obj_set_style_text_font(pill_lbl, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(pill_lbl, lv_color_white(), 0);

    lv_obj_t* bar = lv_bar_create(card);
    lv_obj_set_size(bar, LV_PCT(100), 3);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_bg_color(bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, COLOR_CARD_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, is_subscription ? COLOR_BAR_PINK : COLOR_BAR_GREEN, LV_PART_INDICATOR);
    lv_bar_set_value(bar, static_cast<int32_t>(pct), LV_ANIM_OFF);

    lv_obj_t* sub = lv_label_create(card);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(sub, COLOR_TEXT_DIM, 0);

    if (entry.reset_at) {
        lv_label_set_text(sub, format_reset_in(*entry.reset_at).c_str());
    } else if (is_subscription) {
        lv_label_set_text(sub, "No reset time");
    } else {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %s", entry.name.c_str(), entry.unit.c_str());
        lv_label_set_text(sub, buf);
    }

    return card;
}

const maiq::QuotaEntry* DashboardPage::pick_short_quota(const maiq::AccountStatus& s) const {
    const maiq::QuotaEntry* best = nullptr;
    for (const auto& e : s.entries) {
        std::string name = e.name;
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name.find("hour") != std::string::npos || name.find("h") != std::string::npos) {
            if (!best) {
                best = &e;
            } else {
                // prefer the smallest window if reset_at is nearer
                if (e.reset_at && best->reset_at && *e.reset_at < *best->reset_at) {
                    best = &e;
                }
            }
        }
    }
    if (best) return best;
    for (const auto& e : s.entries) {
        std::string name = e.name;
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name.find("week") == std::string::npos) return &e;
    }
    return s.entries.empty() ? nullptr : &s.entries.front();
}

const maiq::QuotaEntry* DashboardPage::pick_week_quota(const maiq::AccountStatus& s) const {
    for (const auto& e : s.entries) {
        std::string name = e.name;
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name.find("week") != std::string::npos) return &e;
    }
    return s.entries.empty() ? nullptr : &s.entries.back();
}

const maiq::QuotaEntry* DashboardPage::pick_daily_used(const maiq::AccountStatus& s) const {
    for (const auto& e : s.entries) {
        std::string name = e.name;
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if ((name.find("day") != std::string::npos || name.find("daily") != std::string::npos) && e.used) {
            return &e;
        }
    }
    for (const auto& e : s.entries) {
        if (e.used) return &e;
    }
    return s.entries.empty() ? nullptr : &s.entries.front();
}

const maiq::QuotaEntry* DashboardPage::pick_remaining(const maiq::AccountStatus& s) const {
    const maiq::QuotaEntry* best = nullptr;
    for (const auto& e : s.entries) {
        if (e.remaining) {
            if (!best || !best->remaining || *e.remaining > *best->remaining) {
                best = &e;
            }
        }
    }
    return best;
}

} // namespace gui::pages
