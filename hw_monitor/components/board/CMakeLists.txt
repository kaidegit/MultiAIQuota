idf_component_register(
    SRCS "board.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
