file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(maiq_core STATIC ${CORE_SOURCES})

target_include_directories(maiq_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${arduinojson_SOURCE_DIR}/src
)

target_compile_features(maiq_core PUBLIC cxx_std_20)

target_compile_options(maiq_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)
