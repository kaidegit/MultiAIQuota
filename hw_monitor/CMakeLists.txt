cmake_minimum_required(VERSION 3.20)

# Use EXTRA_COMPONENT_DIRS to add third_party and local components.
# Only add the libraries that are actually built into the firmware. cpp-httplib
# is kept in third_party for the PC target, but its upstream CMakeLists.txt is
# not compatible with the ESP-IDF component scanner, so do not scan it here.
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ArduinoJson
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(MultiAIQuotaMonitor)
