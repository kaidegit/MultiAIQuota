#pragma once

#include "page.hpp"

namespace gui::pages {

class HomePage : public Page {
public:
    using Page::Page;
    void create() override;
    void update() override;

private:
    lv_obj_t* status_label_ = nullptr;
};

} // namespace gui::pages
