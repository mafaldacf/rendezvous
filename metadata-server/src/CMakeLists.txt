find_package(nlohmann_json)

file(GLOB SRC_FILES "*.cpp" "*.h" "metadata/*.cpp" "metadata/*.h" "services/*.cpp" "services/*.h" "replicas/*.cpp" "replicas/*.h")
add_executable(rendezvous ${SRC_FILES})
target_link_libraries(
    rendezvous PRIVATE 
    rendezvous_client_lib 
    rendezvous_server_lib 
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only)

# UNCOMMENT to enable AddressSanitizer for detecting memory errors/leaks
# target_compile_options(rendezvous PRIVATE -fsanitize=address -g)
# target_link_options(rendezvous PRIVATE -fsanitize=address -g)
