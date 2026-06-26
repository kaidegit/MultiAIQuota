cmake_minimum_required(VERSION 3.20)
project(MultiAIQuota LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG        v7.3.0
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.20.0
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(ArduinoJson cpp-httplib)

add_subdirectory(core)
add_subdirectory(pc)
add_subdirectory(cli)
add_subdirectory(tests)

option(BUILD_HOST_SIM "Build LVGL host simulator" OFF)
if(BUILD_HOST_SIM)
    FetchContent_Declare(
        lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG        v9.2.2
        GIT_SHALLOW    TRUE
    )
    set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/host_sim/include/lv_conf.h CACHE STRING "" FORCE)
    set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
    set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
    set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(lvgl)

    find_package(SDL2 REQUIRED)
    target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(lvgl PUBLIC ${SDL2_LIBRARIES})

    add_subdirectory(gui)
    add_subdirectory(host_sim)
endif()
