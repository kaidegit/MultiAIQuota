file(GLOB_RECURSE CORE_SRCS ${CMAKE_SOURCE_DIR}/../core/src/*.cpp)

idf_component_register(
    SRCS ${CORE_SRCS}
    INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/../core/include
    REQUIRES ArduinoJson
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
