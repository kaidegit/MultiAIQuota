#include "app.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" void app_main() {
    app::run();
}
