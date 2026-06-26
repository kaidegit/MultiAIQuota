idf_component_register(
    SRCS "input.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lvgl driver board
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
