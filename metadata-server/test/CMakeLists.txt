
find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json)

#file(GLOB TEST_FILES "core_test.cpp" "concurrency_test.cpp" "dependencies_test.cpp" "service_tags_test.cpp")
file(GLOB TEST_FILES "core_test" "service_tags_test.cpp" "concurrency_test.cpp" "wait_logs_test.cpp")

file(GLOB SRC_FILES "../src/*.cpp" "../src/*.h" "../src/metadata/*.cpp" "../src/metadata/*.h" "../src/replicas/*.cpp" "../src/replicas/*.h")
list(FILTER SRC_FILES EXCLUDE REGEX "../src/main.cpp$")

file(GLOB UTILS_FILES "utils.h")

add_executable(tests ${TEST_FILES} ${SRC_FILES} ${UTILS_FILES})

target_link_libraries(
    tests PRIVATE 
    rendezvous_client_lib 
    rendezvous_server_lib 
    GTest::gtest_main
    GTest::gtest
    spdlog::spdlog_header_only
    TBB::tbb)