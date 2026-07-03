idf_component_register(
    SRCS
        "main.cpp"
        "app.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        core
        gui
        lvgl
        ArduinoJson
        http_client_esp
        display
        input
        wifi
        time_sync
        web_server
        storage
        board
        nvs_flash
)

# Pack the built Svelte frontend into the LittleFS partition image.
# Run `npm run build` in hw_monitor/frontend first to populate ../www.
littlefs_create_partition_image(littlefs ${CMAKE_CURRENT_SOURCE_DIR}/../www FLASH_IN_PROJECT)
