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
const lv_color_t COLOR_PANEL_BG = lv_color_hex(0x04070c);
const lv_color_t COLOR_CARD_BG = lv_color_hex(0x03070c);
const lv_color_t COLOR_CARD_EDGE = lv_color_hex(0x1d2733);
const lv_color_t COLOR_CARD_BORDER = lv_color_hex(0x283241);
const lv_color_t COLOR_BAR_BG = lv_color_hex(0x222a36);
const lv_color_t COLOR_BAR_PINK = lv_color_hex(0xff5f9b);
const lv_color_t COLOR_BAR_MAGENTA = lv_color_hex(0xf23df1);
const lv_color_t COLOR_BAR_GREEN = lv_color_hex(0x4ade80);
const lv_color_t COLOR_TEXT_DIM = lv_color_hex(0x9aa3af);
const lv_color_t COLOR_WIFI_ON = lv_color_hex(0x4ade80);
const lv_color_t COLOR_WIFI_OFF = lv_color_hex(0xef4444);

std::string format_reset_short(time_t reset_at) {
    time_t now = std::time(nullptr);
    double diff = std::difftime(reset_at, now);
    if (diff <= 0) return "now";

    if (diff > 365 * 86400) {
        char buf[16];
        struct tm tm_info;
        localtime_r(&reset_at, &tm_info);
        std::strftime(buf, sizeof(buf), "%m-%d %H:%M", &tm_info);
        return buf;
    }

    int64_t seconds = static_cast<int64_t>(diff);
    int64_t days = seconds / 86400;
    seconds %= 86400;
    int64_t hours = seconds / 3600;
    seconds %= 3600;
    int64_t minutes = seconds / 60;

    char buf[24];
    if (days > 0) {
        std::snprintf(buf, sizeof(buf), "%lldd %lldh", days, hours);
    } else if (hours > 0) {
        std::snprintf(buf, sizeof(buf), "%lldh %lldm", hours, minutes);
    } else {
        std::snprintf(buf, sizeof(buf), "%lldm", minutes);
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

static void refresh_icon_spin_cb(void* var, int32_t v) {
    lv_obj_set_style_transform_rotation(static_cast<lv_obj_t*>(var), v, 0);
}

static void start_refresh_spin(lv_obj_t* icon) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, icon);
    lv_anim_set_exec_cb(&a, refresh_icon_spin_cb);
    lv_anim_set_values(&a, 0, 3600);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void stop_refresh_spin(lv_obj_t* icon) {
    lv_anim_del(icon, refresh_icon_spin_cb);
    lv_obj_set_style_transform_rotation(icon, 0, 0);
}

} // namespace

void DashboardPage::create() {
    ESP_LOGI("dashboard", "create: state_ addr=%p", static_cast<const void*>(&state_));

    lv_obj_set_flex_flow(parent_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent_, 0, 0);
    lv_obj_set_style_pad_row(parent_, 0, 0);
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
    lv_obj_set_size(header, LV_PCT(100), 18);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(header, 1, 0);
    lv_obj_set_style_pad_bottom(header, 1, 0);
    lv_obj_set_style_pad_left(header, 5, 0);
    lv_obj_set_style_pad_right(header, 3, 0);
    lv_obj_set_style_pad_column(header, 4, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, COLOR_CARD_EDGE, 0);
    lv_obj_set_style_bg_color(header, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(header, 0, 0);

    lv_obj_t* logo = lv_label_create(header);
    lv_label_set_text(logo, "*");
    lv_obj_set_width(logo, 10);
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(logo, COLOR_BAR_PINK, 0);

    header_name_ = lv_label_create(header);
    lv_obj_set_flex_grow(header_name_, 1);
    lv_obj_set_style_text_align(header_name_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(header_name_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(header_name_, lv_color_white(), 0);
    lv_label_set_long_mode(header_name_, LV_LABEL_LONG_DOT);

    header_wifi_ = lv_label_create(header);
    lv_label_set_text(header_wifi_, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(header_wifi_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(header_wifi_, lv_color_white(), 0);

    header_status_dot_ = lv_obj_create(header);
    lv_obj_remove_style_all(header_status_dot_);
    lv_obj_set_size(header_status_dot_, 5, 5);
    lv_obj_set_style_radius(header_status_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(header_status_dot_, LV_OPA_COVER, 0);
}

void DashboardPage::create_cards_area() {
    cards_container_ = lv_obj_create(parent_);
    lv_obj_set_width(cards_container_, LV_PCT(100));
    lv_obj_set_height(cards_container_, 0);
    lv_obj_set_flex_grow(cards_container_, 1);
    lv_obj_set_flex_flow(cards_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_top(cards_container_, 4, 0);
    lv_obj_set_style_pad_bottom(cards_container_, 3, 0);
    lv_obj_set_style_pad_hor(cards_container_, 3, 0);
    lv_obj_set_style_pad_row(cards_container_, 4, 0);
    lv_obj_set_style_border_width(cards_container_, 0, 0);
    lv_obj_set_style_bg_opa(cards_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(cards_container_, LV_SCROLLBAR_MODE_OFF);
}

void DashboardPage::create_footer() {
    lv_obj_t* footer = lv_obj_create(parent_);
    lv_obj_set_size(footer, LV_PCT(100), 16);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_top(footer, 1, 0);
    lv_obj_set_style_pad_bottom(footer, 1, 0);
    lv_obj_set_style_pad_hor(footer, 5, 0);
    lv_obj_set_style_pad_column(footer, 4, 0);
    lv_obj_set_style_border_width(footer, 1, 0);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(footer, COLOR_CARD_EDGE, 0);
    lv_obj_set_style_bg_color(footer, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(footer, 0, 0);

    footer_dots_ = lv_obj_create(footer);
    lv_obj_remove_style_all(footer_dots_);
    lv_obj_set_size(footer_dots_, 24, 10);
    lv_obj_set_flex_flow(footer_dots_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer_dots_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(footer_dots_, 0, 0);
    lv_obj_set_style_pad_column(footer_dots_, 5, 0);

    footer_page_ = lv_label_create(footer);
    lv_obj_set_flex_grow(footer_page_, 1);
    lv_obj_set_style_text_align(footer_page_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(footer_page_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(footer_page_, lv_color_white(), 0);

    footer_refresh_icon_ = lv_label_create(footer);
    lv_label_set_text(footer_refresh_icon_, LV_SYMBOL_REFRESH);
    lv_obj_set_size(footer_refresh_icon_, 10, 10);
    lv_obj_set_style_text_align(footer_refresh_icon_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer_refresh_icon_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(footer_refresh_icon_, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_transform_pivot_x(footer_refresh_icon_, 5, 0);
    lv_obj_set_style_transform_pivot_y(footer_refresh_icon_, 5, 0);

    footer_time_ = lv_label_create(footer);
    lv_obj_set_style_text_font(footer_time_, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(footer_time_, COLOR_TEXT_DIM, 0);
}

void DashboardPage::update() {
    const char* account_name = "<none>";
    std::string vendor_name = "<none>";
    if (!state_.statuses.empty() && state_.selected_account_index < state_.statuses.size()) {
        const auto& selected = state_.statuses[state_.selected_account_index];
        account_name = selected.account_name.c_str();
        vendor_name = maiq::to_string(selected.vendor);
    }
    ESP_LOGI("dashboard", "update: selected=%zu statuses=%zu querying=%d account=%s vendor=%s",
             state_.selected_account_index, state_.statuses.size(),
             static_cast<int>(state_.querying), account_name, vendor_name.c_str());
    update_header();

    if (!cards_container_) return;
    lv_obj_clean(cards_container_);

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
    if (!header_name_ || !header_wifi_ || !header_status_dot_) return;

    if (state_.statuses.empty()) {
        lv_label_set_text(header_name_, "MultiAIQuota");
    } else {
        const auto& s = state_.statuses[state_.selected_account_index];
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s (%s)",
                      s.account_name.c_str(), maiq::to_string(s.vendor).c_str());
        lv_label_set_text(header_name_, buf);
    }

    lv_obj_set_style_text_color(header_wifi_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(header_status_dot_,
                              state_.wifi_connected() ? COLOR_WIFI_ON : COLOR_WIFI_OFF,
                              0);
}

void DashboardPage::update_footer() {
    if (!footer_dots_ || !footer_time_ || !footer_refresh_icon_) return;

    lv_obj_clean(footer_dots_);
    size_t dot_count = state_.statuses.size();
    if (dot_count > 4) dot_count = 4;
    for (size_t i = 0; i < dot_count; ++i) {
        lv_obj_t* dot = lv_obj_create(footer_dots_);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, i == state_.selected_account_index ? 5 : 4,
                        i == state_.selected_account_index ? 5 : 4);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot,
                                  i == state_.selected_account_index ? COLOR_BAR_PINK : COLOR_BAR_BG,
                                  0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }
    if (footer_page_) {
        char page_buf[16];
        std::snprintf(page_buf, sizeof(page_buf), "%zu/%zu",
                      state_.statuses.empty() ? 0 : state_.selected_account_index + 1,
                      state_.statuses.empty() ? 0 : state_.statuses.size());
        lv_label_set_text(footer_page_, page_buf);
    }

    if (state_.querying) {
        if (!refresh_spinning_) {
            start_refresh_spin(footer_refresh_icon_);
            refresh_spinning_ = true;
        }
    } else {
        if (refresh_spinning_) {
            stop_refresh_spin(footer_refresh_icon_);
            refresh_spinning_ = false;
        }
    }

    if (state_.last_refresh_at) {
        lv_label_set_text(footer_time_, format_refresh_time(*state_.last_refresh_at).c_str());
    } else {
        lv_label_set_text(footer_time_, "--:--");
    }
}

void DashboardPage::render_subscription(const maiq::AccountStatus& s) {
    auto* short_entry = pick_short_quota(s);
    auto* week_entry = pick_week_quota(s);

    if (short_entry) {
        std::string title = short_entry->name;
        bool has_quota = title.find("Quota") != std::string::npos ||
                         title.find("quota") != std::string::npos;
        if (!has_quota) title += " Quota";
        make_card(title.c_str(), *short_entry, true, false);
    } else {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, "No short quota");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    }

    if (week_entry) {
        make_card("Weekly Quota", *week_entry, true, true);
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
        make_card("Today", *daily, false, false);
    } else {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, "No daily usage");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    }

    if (remaining) {
        make_card("Balance", *remaining, false, true);
    } else {
        lv_obj_t* lbl = lv_label_create(cards_container_);
        lv_label_set_text(lbl, "No balance");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    }
}

lv_obj_t* DashboardPage::make_card(const char* title, const maiq::QuotaEntry& entry,
                                   bool is_subscription, bool weekly) {
    lv_obj_t* card = lv_obj_create(cards_container_);
    lv_obj_set_size(card, LV_PCT(100), 38);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 5, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(card, COLOR_PANEL_BG, 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_HOR, 0);

    lv_obj_t* icon = lv_label_create(card);
    lv_label_set_text(icon, weekly ? LV_SYMBOL_LIST : LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(icon, COLOR_BAR_PINK, 0);
    lv_obj_set_pos(icon, 6, 4);

    lv_obj_t* title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_size(title_lbl, 76, 12);
    lv_obj_set_pos(title_lbl, 17, 3);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_white(), 0);

    lv_obj_t* reset = lv_label_create(card);
    lv_obj_set_size(reset, 58, 10);
    lv_obj_set_pos(reset, 94, 4);
    lv_label_set_long_mode(reset, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(reset, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(reset, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_align(reset, LV_TEXT_ALIGN_RIGHT, 0);
    if (entry.reset_at) {
        std::string txt = std::string(LV_SYMBOL_REFRESH " ") + format_reset_short(*entry.reset_at);
        lv_label_set_text(reset, txt.c_str());
    } else {
        lv_label_set_text(reset, "--");
    }

    double pct = 0.0;
    char value_buf[64];
    if (is_subscription) {
        if (entry.total && *entry.total > 0 && entry.remaining) {
            pct = (*entry.remaining / *entry.total) * 100.0;
        }
        std::snprintf(value_buf, sizeof(value_buf), "%d%%", static_cast<int>(pct));
    } else {
        if (entry.used) {
            std::snprintf(value_buf, sizeof(value_buf), "%.1f", *entry.used);
            pct = (entry.total && *entry.total > 0) ? (*entry.used / *entry.total * 100.0) : 0.0;
        } else if (entry.remaining) {
            std::snprintf(value_buf, sizeof(value_buf), "%.1f", *entry.remaining);
            pct = (entry.total && *entry.total > 0) ? (*entry.remaining / *entry.total * 100.0) : 0.0;
        } else {
            std::snprintf(value_buf, sizeof(value_buf), "-");
        }
    }
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;

    lv_obj_t* value = lv_label_create(card);
    lv_label_set_text(value, value_buf);
    lv_obj_set_pos(value, 5, 15);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(value, lv_color_white(), 0);

    lv_obj_t* bar = lv_bar_create(card);
    lv_obj_set_size(bar, 92, 7);
    lv_obj_set_pos(bar, 60, 27);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, is_subscription ? COLOR_BAR_PINK : COLOR_BAR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(bar, is_subscription ? COLOR_BAR_MAGENTA : COLOR_BAR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_main_stop(bar, 28, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_stop(bar, 220, LV_PART_INDICATOR);
    lv_bar_set_value(bar, static_cast<int32_t>(pct), LV_ANIM_OFF);

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
