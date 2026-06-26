idf_component_register(
    SRCS "wifi.cpp"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_wifi esp_netif esp_event
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
