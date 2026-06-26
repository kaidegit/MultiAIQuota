file(GLOB_RECURSE GUI_SRCS ${CMAKE_SOURCE_DIR}/../gui/src/*.cpp)

idf_component_register(
    SRCS ${GUI_SRCS}
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/../gui/include
        ${CMAKE_SOURCE_DIR}/../core/include
    REQUIRES lvgl core
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
