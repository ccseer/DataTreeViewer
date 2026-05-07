cmake_minimum_required(VERSION 3.16)
project(datatreeviewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)

# ---- SeerSdk (plugin host interface) ----
include(FetchContent)
message(STATUS "Fetching third-party dependencies... seer-sdk")
FetchContent_Declare(SeerSdk
    GIT_REPOSITORY https://github.com/ccseer/Seer-sdk.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SeerSdk)

# ---- Third-Party via FetchContent ----
include(FetchContent)

# nlohmann/json (header-only)
message(STATUS "Fetching third-party dependencies... nlohmann_json")
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# toml++ (header-only; noex API to avoid exceptions)
message(STATUS "Fetching third-party dependencies... toml")
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
set(TOML_EXCEPTIONS 0 CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tomlplusplus)

# SimpleIni (single header)
message(STATUS "Fetching third-party dependencies... simpleini")
FetchContent_Declare(simpleini
    GIT_REPOSITORY https://github.com/brofield/simpleini.git
    GIT_TAG        v4.22
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(simpleini)

# rapidyaml — needs exceptions for parse error handling
set(RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS ON CACHE BOOL "" FORCE)
FetchContent_Declare(rapidyaml
    GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
    GIT_TAG        v0.7.2
    GIT_SUBMODULES ext/c4core
)
FetchContent_MakeAvailable(rapidyaml)

message(STATUS "Fetching third-party dependencies... pugixml")
FetchContent_Declare(pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pugixml)

# ---- Sources ----
set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/config_node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/iformat_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/parser_registry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/parser_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/parser_helpers.cpp
)

set(PARSER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/nlohmann_convert.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/jsonc_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/jsonc_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/toml_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/toml_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/ini_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/ini_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/yaml_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/yaml_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/env_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/env_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/jsonl_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/jsonl_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/cbor_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/cbor_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/xml_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/xml_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/plist_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsers/plist_parser.cpp
)

set(WORKER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/background_thread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/parse_worker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/parse_worker.cpp
)

set(UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/breadcrumb_bar.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/breadcrumb_bar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/search_bar.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/search_bar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/status_bar.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/status_bar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/style_assets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/node_path.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/node_path.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/tree_delegate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/tree_delegate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/tree_filter_proxy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/tree_filter_proxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/tree_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/tree_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/tree_renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/tree_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/data_tree_viewer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/data_tree_viewer.cpp
)

# ---- DataTreeViewer DLL ----
add_library(datatreeviewer SHARED
    ${CORE_SOURCES}
    ${PARSER_SOURCES}
    ${WORKER_SOURCES}
    ${UI_SOURCES}
    ${seersdk_SOURCE_DIR}/seer/viewerbase.h
)

target_include_directories(datatreeviewer PRIVATE
    src/
    ${simpleini_SOURCE_DIR}
)

target_link_libraries(datatreeviewer PRIVATE
    SeerSdk::SeerSdk
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    ryml
    pugixml::pugixml
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg
)

set_target_properties(datatreeviewer PROPERTIES
    OUTPUT_NAME "datatreeviewer" SUFFIX ".dll")

# Copy plugin.json to output dir (Seer plugin discovery needs it alongside the DLL)
add_custom_command(TARGET datatreeviewer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/bin/plugin.json"
        "$<TARGET_FILE_DIR:datatreeviewer>/plugin.json")

# ---- Seer Integration Test ----
add_executable(datatreeviewer_test WIN32 src/test.cpp)
target_include_directories(datatreeviewer_test PRIVATE src/ ${simpleini_SOURCE_DIR})
target_link_libraries(datatreeviewer_test PRIVATE
    datatreeviewer SeerSdk::SeerSdk Qt6::Core Qt6::Gui Qt6::Widgets)
set_target_properties(datatreeviewer_test PROPERTIES
    VS_STARTUP_PROJECT ON)

# ---- Tests ----
option(BUILD_TESTS "Build parser unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
