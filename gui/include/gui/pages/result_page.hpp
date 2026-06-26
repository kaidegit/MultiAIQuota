#pragma once

#include "page.hpp"

namespace gui::pages {

class ResultPage : public Page {
public:
    using Page::Page;
    void create() override;
    void update() override;

private:
    lv_obj_t* container_ = nullptr;
};

} // namespace gui::pages
