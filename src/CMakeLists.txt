
file(GLOB SRC_FILES "*.cpp" "*.h")
add_executable(rendezvous ${SRC_FILES})
target_link_libraries(rendezvous rendezvous_lib)

# UNCOMMENT to enable AddressSanitizer for detecting memory errors/leaks
#target_compile_options(rendezvous PRIVATE -fsanitize=address -g)
#target_link_options(rendezvous PRIVATE -fsanitize=address -g)
