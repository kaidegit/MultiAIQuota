#pragma once

#include "page.hpp"

#include "maiq/types.hpp"

namespace gui::pages {

class DashboardPage : public Page {
public:
    using Page::Page;
    void create() override;
    void update() override;

private:
    void create_header();
    void create_cards_area();
    void create_footer();

    void update_header();
    void update_footer();

    void render_subscription(const maiq::AccountStatus& s);
    void render_balance(const maiq::AccountStatus& s);

    lv_obj_t* make_card(const char* title, const maiq::QuotaEntry& entry,
                        bool is_subscription, bool weekly);

    const maiq::QuotaEntry* pick_short_quota(const maiq::AccountStatus& s) const;
    const maiq::QuotaEntry* pick_week_quota(const maiq::AccountStatus& s) const;
    const maiq::QuotaEntry* pick_daily_used(const maiq::AccountStatus& s) const;
    const maiq::QuotaEntry* pick_remaining(const maiq::AccountStatus& s) const;

    lv_obj_t* header_name_ = nullptr;
    lv_obj_t* header_wifi_ = nullptr;
    lv_obj_t* header_status_dot_ = nullptr;
    lv_obj_t* cards_container_ = nullptr;
    lv_obj_t* footer_dots_ = nullptr;
    lv_obj_t* footer_page_ = nullptr;
    lv_obj_t* footer_time_ = nullptr;
};

} // namespace gui::pages
