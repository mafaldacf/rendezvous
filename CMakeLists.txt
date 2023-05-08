cmake_minimum_required(VERSION 3.8)

project(Rendezvous)
set(CMAKE_CXX_STANDARD 17)

option(CONFIG_ONLY "Compile only config.json file" OFF)

if(NOT TARGET spdlog)
    # Stand-alone build
    find_package(spdlog REQUIRED)
endif()


if (CONFIG_ONLY)
    message("[INFO] Copying config file...")
    configure_file(config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
else()
    message("[INFO] Building rendezvous...")
    configure_file(config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
    include(cmake/FindgRPC.cmake)
    include(cmake/CompileProtos.cmake)
    #include(cmake/FindTBB.cmake)
    compile_protos(client)
    compile_protos(server)

    add_subdirectory(src)
    add_subdirectory(test)
    #add_subdirectory(examples/cpp)
endif()