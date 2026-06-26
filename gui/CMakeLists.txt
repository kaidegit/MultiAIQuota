file(GLOB_RECURSE GUI_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(maiq_gui STATIC ${GUI_SOURCES})

target_include_directories(maiq_gui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${lvgl_SOURCE_DIR}/src
        ${arduinojson_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(maiq_gui
    PUBLIC
        lvgl
        maiq_core
)

target_compile_features(maiq_gui PUBLIC cxx_std_20)
