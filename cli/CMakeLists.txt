file(GLOB_RECURSE CLI_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(maiq ${CLI_SOURCES})

target_include_directories(maiq PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(maiq
    PRIVATE
        maiq_core
        maiq_platform_pc
)

target_compile_features(maiq PRIVATE cxx_std_20)
