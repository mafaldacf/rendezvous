
find_package(GTest CONFIG REQUIRED)

#file(GLOB TEST_FILES "*.cpp")
file(GLOB TEST_FILES "server_test.cpp" "server_concurrency_test.cpp")

file(GLOB SRC_FILES "../src/*.cpp" "../src/*.h" "../src/metadata/*.cpp" "../src/metadata/*.h" "../src/replicas/*.cpp" "../src/replicas/*.h")
list(FILTER SRC_FILES EXCLUDE REGEX "../src/main.cpp$")

file(GLOB UTILS_FILES "../examples/cpp/utils.h")

add_executable(tests server_test.cpp ${SRC_FILES} ${UTILS_FILES})
target_link_libraries(tests PRIVATE rendezvous_lib rendezvous_server_lib GTest::gtest_main GTest::gtest)