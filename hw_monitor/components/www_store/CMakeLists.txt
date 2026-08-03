idf_component_register(
    SRCS "src/www_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES heap esp_rom
    # Embed the packed Svelte frontend (www.tar.gz) into the firmware.
    # Run `npm run build` in hw_monitor/frontend first: it populates ../www
    # and packs it into ../www.tar.gz, which is decompressed into PSRAM at boot.
    EMBED_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../www.tar.gz"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
