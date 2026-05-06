cmake_minimum_required(VERSION 3.16)
project(datatreeviewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(MSVC AND PROJECT_IS_TOP_LEVEL)
    add_compile_options("$<$<CONFIG:Release>:/Zi>")
    add_link_options(
        "$<$<CONFIG:Release>:/DEBUG>"
        "$<$<CONFIG:Release>:/OPT:REF>"
        "$<$<CONFIG:Release>:/OPT:ICF>")
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
    src/core/config_node.h
    src/core/iformat_parser.h
    src/core/parser_registry.h
    src/core/parser_registry.cpp
    src/core/parser_helpers.cpp
)

set(PARSER_SOURCES
    src/parsers/nlohmann_convert.h
    src/parsers/jsonc_parser.h
    src/parsers/jsonc_parser.cpp
    src/parsers/toml_parser.h
    src/parsers/toml_parser.cpp
    src/parsers/ini_parser.h
    src/parsers/ini_parser.cpp
    src/parsers/yaml_parser.h
    src/parsers/yaml_parser.cpp
    src/parsers/env_parser.h
    src/parsers/env_parser.cpp
    src/parsers/jsonl_parser.h
    src/parsers/jsonl_parser.cpp
    src/parsers/cbor_parser.h
    src/parsers/cbor_parser.cpp
    src/parsers/xml_parser.h
    src/parsers/xml_parser.cpp
    src/parsers/plist_parser.h
    src/parsers/plist_parser.cpp
)

set(WORKER_SOURCES
    src/workers/background_thread.h
    src/workers/parse_worker.h
    src/workers/parse_worker.cpp
)

set(UI_SOURCES
    src/ui/breadcrumb_bar.h
    src/ui/breadcrumb_bar.cpp
    src/ui/search_bar.h
    src/ui/search_bar.cpp
    src/ui/status_bar.h
    src/ui/status_bar.cpp
    src/ui/style_assets.h
    src/ui/tree_delegate.h
    src/ui/tree_delegate.cpp
    src/ui/tree_filter_proxy.h
    src/ui/tree_filter_proxy.cpp
    src/ui/tree_model.h
    src/ui/tree_model.cpp
    src/ui/tree_renderer.h
    src/ui/tree_renderer.cpp
    src/ui/data_tree_viewer.h
    src/ui/data_tree_viewer.cpp
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
    find_package(Qt6 REQUIRED COMPONENTS Test)

    set(FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    set(PARSER_TEST_TARGETS)

    function(add_parser_test NAME SOURCE)
        add_executable(${NAME} ${SOURCE})
        target_include_directories(${NAME} PRIVATE src/ ${simpleini_SOURCE_DIR})
        target_link_libraries(${NAME} PRIVATE
            nlohmann_json::nlohmann_json
            tomlplusplus::tomlplusplus
            ryml
            pugixml::pugixml
            Qt6::Test
        )
        target_sources(${NAME} PRIVATE
            ${CORE_SOURCES}
            ${PARSER_SOURCES}
        )
        target_compile_definitions(${NAME} PRIVATE
            FIXTURES_DIR="${FIXTURES_DIR}")
        add_test(NAME ${NAME} COMMAND ${NAME})
        set(PARSER_TEST_TARGETS ${PARSER_TEST_TARGETS} ${NAME} PARENT_SCOPE)
    endfunction()

    add_parser_test(test_jsonc_parser tests/test_jsonc_parser.cpp)
    add_parser_test(test_toml_parser  tests/test_toml_parser.cpp)
    add_parser_test(test_ini_parser   tests/test_ini_parser.cpp)
    add_parser_test(test_yaml_parser  tests/test_yaml_parser.cpp)
    add_parser_test(test_env_parser   tests/test_env_parser.cpp)
    add_parser_test(test_jsonl_parser tests/test_jsonl_parser.cpp)
    add_parser_test(test_cbor_parser  tests/test_cbor_parser.cpp)
    add_parser_test(test_xml_parser   tests/test_xml_parser.cpp)
    add_parser_test(test_plist_parser tests/test_plist_parser.cpp)

    add_custom_target(run_all_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                $<$<BOOL:$<CONFIG>>:-C>
                $<$<BOOL:$<CONFIG>>:$<CONFIG>>
        DEPENDS ${PARSER_TEST_TARGETS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building and running all parser tests"
        USES_TERMINAL
    )
endif()
