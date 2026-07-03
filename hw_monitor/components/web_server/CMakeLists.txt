idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server storage core wifi http_client_esp display
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
