cmake_minimum_required(VERSION 3.20)
project(MultiAIQuota LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MAIQ_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set(arduinojson_SOURCE_DIR ${MAIQ_THIRD_PARTY_DIR}/ArduinoJson)
set(cpp-httplib_SOURCE_DIR ${MAIQ_THIRD_PARTY_DIR}/cpp-httplib)
set(lvgl_SOURCE_DIR ${MAIQ_THIRD_PARTY_DIR}/lvgl)

if(NOT EXISTS ${arduinojson_SOURCE_DIR}/src/ArduinoJson.h)
    message(FATAL_ERROR "Missing ArduinoJson submodule. Run: git submodule update --init --recursive")
endif()

if(NOT EXISTS ${cpp-httplib_SOURCE_DIR}/httplib.h)
    message(FATAL_ERROR "Missing cpp-httplib submodule at third_party/cpp-httplib. Add/init the submodule before configuring.")
endif()

add_subdirectory(core)
add_subdirectory(pc)
add_subdirectory(cli)
add_subdirectory(tests)

option(BUILD_HOST_SIM "Build LVGL host simulator" OFF)
if(BUILD_HOST_SIM)
    if(NOT EXISTS ${lvgl_SOURCE_DIR}/CMakeLists.txt)
        message(FATAL_ERROR "Missing lvgl submodule. Run: git submodule update --init --recursive")
    endif()

    set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/host_sim/include/lv_conf.h CACHE STRING "" FORCE)
    set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
    set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
    set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
    add_subdirectory(${lvgl_SOURCE_DIR} ${CMAKE_BINARY_DIR}/third_party/lvgl)

    find_package(SDL2 REQUIRED)
    target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(lvgl PUBLIC ${SDL2_LIBRARIES})

    add_subdirectory(gui)
    add_subdirectory(host_sim)
endif()
