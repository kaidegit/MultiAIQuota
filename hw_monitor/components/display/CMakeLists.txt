idf_component_register(
    SRCS "display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd lvgl driver board
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
