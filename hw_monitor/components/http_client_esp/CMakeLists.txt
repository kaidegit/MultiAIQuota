idf_component_register(
    SRCS "src/http_client_esp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core esp_http_client mbedtls
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
