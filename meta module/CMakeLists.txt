cmake_minimum_required(VERSION 3.22)
include(../metamodule-plugin-sdk/plugin.cmake)

project(MADZINE 
    VERSION 2.1.3 
    DESCRIPTION "MADZINE Modules for MetaModule" 
    LANGUAGES C CXX ASM
)

add_library(MADZINE STATIC)
target_include_directories(MADZINE PRIVATE src)

target_sources(MADZINE PRIVATE
    src/plugin.cpp
    src/SwingLFO.cpp
    src/EuclideanRhythm.cpp
    src/ADGenerator.cpp
    src/Pinpple.cpp
    src/MADDY.cpp
    src/PPaTTTerning.cpp
    src/TWNC.cpp
    src/QQ.cpp
    src/Observer.cpp
)

create_plugin(
    SOURCE_LIB      MADZINE
    PLUGIN_NAME     MADZINE
    PLUGIN_JSON     ${CMAKE_CURRENT_LIST_DIR}/plugin.json
    SOURCE_ASSETS   ${CMAKE_CURRENT_LIST_DIR}/assets
    DESTINATION     ${CMAKE_CURRENT_LIST_DIR}/../metamodule-plugins/
)