cmake_minimum_required(VERSION 3.5.1)

project(Rendezvous)
set(CMAKE_CXX_STANDARD 14)

option(CONFIG_ONLY "Compile only config.json file" OFF)

if (CONFIG_ONLY)
    message("[INFO] Copying config file...")
    configure_file(config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
else()
    message("[INFO] Building rendezvous...")
    configure_file(config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
    include(cmake/FindgRPC.cmake)
    include(cmake/CompileProtos.cmake)
    compile_protos(rendezvous)
    compile_protos(rendezvous_server)

    add_subdirectory(src)
    #add_subdirectory(test)
    #add_subdirectory(examples/cpp)
endif()