
find_package(GTest CONFIG REQUIRED)

file(GLOB TEST_FILES "*.cpp")

file(GLOB SRC_FILES "../src/*.cpp" "../src/*.h")
list(FILTER SRC_FILES EXCLUDE REGEX "../src/main.cpp$")

file(GLOB UTILS_FILES "../examples/cpp/utils.h")

add_executable(tests ${TEST_FILES} ${SRC_FILES} ${UTILS_FILES})
target_link_libraries(tests monitor_lib GTest::gtest_main GTest::gtest)