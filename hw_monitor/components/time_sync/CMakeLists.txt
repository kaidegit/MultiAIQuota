idf_component_register(
    SRCS "time_sync.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
