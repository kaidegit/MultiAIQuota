find_package(OpenSSL REQUIRED)

add_library(maiq_platform_pc STATIC src/http_client_pc.cpp)

target_include_directories(maiq_platform_pc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${cpp-httplib_SOURCE_DIR}
        ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(maiq_platform_pc
    PUBLIC
        maiq_core
        OpenSSL::SSL
        OpenSSL::Crypto
)

target_compile_features(maiq_platform_pc PUBLIC cxx_std_20)
