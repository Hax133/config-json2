cmake_minimum_required(VERSION 3.16)
project(config-json2 LANGUAGES CXX)

include(CheckIncludeFileCXX)
check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()


find_package(nlohmann_json QUIET)
set(JSON_LIBS)
if(nlohmann_json_FOUND)
    list(APPEND JSON_LIBS nlohmann_json::nlohmann_json)
    message(STATUS "Found nlohmann_json package")
else()
    message(STATUS "nlohmann_json not found, try third_party header")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/json.hpp")
        include_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party")
        message(STATUS "Using bundled nlohmann_json header")
    else()
        message(FATAL_ERROR "nlohmann_json not found. Install nlohmann-json3-dev or place single header in contrib/json-config/third_party/nlohmann/json.hpp")
    endif()
endif()

set(CONFIG_JSON_SRC
    # === Helper Headers ===
    helper/config-json2-helper.cc

    # === Model Headers ===
    model/config-json2-handler-default.cc
    model/config-json2-handler-extra.cc
)

set(CONFIG_JSON_HDR
    # === Helper Headers ===
    helper/config-json2-helper.h

    # === Model Headers ===
    model/config-json2-handler-default.h
    model/config-json2-handler-extra.h
)


build_lib(
    LIBNAME           config-json2
    SOURCE_FILES      ${CONFIG_JSON_SRC}
    HEADER_FILES      ${CONFIG_JSON_HDR}
    LIBRARIES_TO_LINK ${libcore} ${JSON_LIBS} protobuf
)
find_package(Boost REQUIRED COMPONENTS filesystem)
target_link_libraries(${libconfig-json2} PUBLIC Boost::filesystem)