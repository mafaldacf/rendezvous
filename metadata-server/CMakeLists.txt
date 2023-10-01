cmake_minimum_required(VERSION 3.8)

project(Rendezvous)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CONFIG_ONLY "Compile only config.json file" OFF)
option(TESTS "Compile rendezvous with GTests" OFF)

if(NOT TARGET spdlog)
    # Stand-alone build
    find_package(spdlog REQUIRED)
endif()


message("[INFO] Copying config files...")
# Copy all JSON config files from configs/ directory
file(GLOB config_files ${CMAKE_SOURCE_DIR}/config/*.json)
foreach(config ${config_files})
    get_filename_component(config_filename ${config} NAME)
    configure_file(${config} ${CMAKE_CURRENT_BINARY_DIR}/config/${config_filename} COPYONLY)
endforeach()

file(GLOB config_files ${CMAKE_SOURCE_DIR}/config/connections/*.json)
foreach(config ${config_files})
    get_filename_component(config_filename ${config} NAME)
    configure_file(${config} ${CMAKE_CURRENT_BINARY_DIR}/config/connections/${config_filename} COPYONLY)
endforeach()

if(NOT CONFIG_ONLY)
    message("[INFO] Building rendezvous project")
    include(cmake/FindgRPC.cmake)
    include(cmake/CompileProtos.cmake)
    include(cmake/FindTBB.cmake)
    compile_protos(client)
    compile_protos(server)
    add_subdirectory(src)

    message("[INFO] Building rendezvous tests")
    add_subdirectory(test)
endif()