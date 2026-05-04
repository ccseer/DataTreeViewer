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

# ---- Sources ----
set(CORE_SOURCES
    src/core/config_node.h
    src/core/iformat_parser.h
    src/core/parser_registry.h
    src/core/parser_registry.cpp
)

set(PARSER_SOURCES
    src/parsers/jsonc_parser.h
    src/parsers/jsonc_parser.cpp
    src/parsers/toml_parser.h
    src/parsers/toml_parser.cpp
    src/parsers/ini_parser.h
    src/parsers/ini_parser.cpp
    src/parsers/yaml_parser.h
    src/parsers/yaml_parser.cpp
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
    src/ui/tree_renderer.h
    src/ui/tree_renderer.cpp
    src/ui/data_tree_viewer.h
    src/ui/data_tree_viewer.cpp
)

# ---- DataTreeViewer DLL ----
add_library(datatreeviewer SHARED
    plugin_main.cpp
    ${CORE_SOURCES}
    ${PARSER_SOURCES}
    ${WORKER_SOURCES}
    ${UI_SOURCES}
)

target_include_directories(datatreeviewer PRIVATE
    src/
    ${simpleini_SOURCE_DIR}
)

target_link_libraries(datatreeviewer PRIVATE
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    ryml
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg
)

set_target_properties(datatreeviewer PROPERTIES
    OUTPUT_NAME "datatreeviewer" SUFFIX ".dll")

# ---- Demo executable ----
add_executable(datatreeviewer_demo WIN32 src/demo_host.cpp)
target_include_directories(datatreeviewer_demo PRIVATE src/ ${simpleini_SOURCE_DIR})
target_link_libraries(datatreeviewer_demo PRIVATE
    datatreeviewer Qt6::Core Qt6::Gui Qt6::Widgets)

# ---- Tests ----
option(BUILD_TESTS "Build parser unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    set(FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

    function(add_parser_test NAME SOURCE)
        add_executable(${NAME} ${SOURCE})
        target_include_directories(${NAME} PRIVATE src/ ${simpleini_SOURCE_DIR})
        target_link_libraries(${NAME} PRIVATE
            nlohmann_json::nlohmann_json
            tomlplusplus::tomlplusplus
            ryml
            Qt6::Test
        )
        target_sources(${NAME} PRIVATE
            ${CORE_SOURCES}
            ${PARSER_SOURCES}
        )
        target_compile_definitions(${NAME} PRIVATE
            FIXTURES_DIR="${FIXTURES_DIR}")
        add_test(NAME ${NAME} COMMAND ${NAME})
    endfunction()

    add_parser_test(test_jsonc_parser tests/test_jsonc_parser.cpp)
    add_parser_test(test_toml_parser  tests/test_toml_parser.cpp)
    add_parser_test(test_ini_parser   tests/test_ini_parser.cpp)
    add_parser_test(test_yaml_parser  tests/test_yaml_parser.cpp)
endif()
