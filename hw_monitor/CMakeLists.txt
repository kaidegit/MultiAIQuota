cmake_minimum_required(VERSION 3.20)

# Use EXTRA_COMPONENT_DIRS to add third_party and local components.
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(MultiAIQuotaMonitor)
