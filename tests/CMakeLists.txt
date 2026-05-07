find_package(Qt6 REQUIRED COMPONENTS Test)

set(FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set(PARSER_TEST_TARGETS)

function(add_parser_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_include_directories(${NAME} PRIVATE ../src/ ${simpleini_SOURCE_DIR})
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
    set_target_properties(${NAME} PROPERTIES FOLDER "Tests")
    add_test(NAME ${NAME} COMMAND ${NAME})
    set(PARSER_TEST_TARGETS ${PARSER_TEST_TARGETS} ${NAME} PARENT_SCOPE)
endfunction()

add_parser_test(test_jsonc_parser test_jsonc_parser.cpp)
add_parser_test(test_toml_parser  test_toml_parser.cpp)
add_parser_test(test_ini_parser   test_ini_parser.cpp)
add_parser_test(test_yaml_parser  test_yaml_parser.cpp)
add_parser_test(test_env_parser   test_env_parser.cpp)
add_parser_test(test_jsonl_parser test_jsonl_parser.cpp)
add_parser_test(test_cbor_parser  test_cbor_parser.cpp)
add_parser_test(test_xml_parser   test_xml_parser.cpp)
add_parser_test(test_plist_parser test_plist_parser.cpp)

# UI Layer / Helper tests
add_executable(test_node_path test_node_path.cpp)
target_include_directories(test_node_path PRIVATE ../src/ ${simpleini_SOURCE_DIR})
target_link_libraries(test_node_path PRIVATE
    datatreeviewer Qt6::Gui Qt6::Widgets Qt6::Test
)
set_target_properties(test_node_path PROPERTIES FOLDER "Tests")
add_test(NAME test_node_path COMMAND test_node_path)
list(APPEND PARSER_TEST_TARGETS test_node_path)

# Meta target to run all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            $<$<BOOL:$<CONFIG>>:-C>
            $<$<BOOL:$<CONFIG>>:$<CONFIG>>
    DEPENDS ${PARSER_TEST_TARGETS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and running all parser tests"
    USES_TERMINAL
)
