find_package(SDL2 REQUIRED)

option(MAIQ_GUI_HOST_SAVE_SNAPSHOT "Save a PNG screenshot and exit" OFF)

add_executable(maiq_gui_host src/host_main.cpp)

target_include_directories(maiq_gui_host
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${lvgl_SOURCE_DIR}/src
        ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(maiq_gui_host
    PRIVATE
        maiq_gui
        maiq_core
        maiq_platform_pc
        lvgl
        ${SDL2_LIBRARIES}
)

target_compile_features(maiq_gui_host PRIVATE cxx_std_20)

if(MAIQ_GUI_HOST_SAVE_SNAPSHOT)
    target_compile_definitions(maiq_gui_host PRIVATE MAIQ_GUI_HOST_SAVE_SNAPSHOT=1)
endif()
