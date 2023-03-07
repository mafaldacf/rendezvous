
file(GLOB EXAMPLE_FILES "*.cpp" "*.h")
add_executable(client ${EXAMPLE_FILES})
target_link_libraries(client rendezvous_lib)