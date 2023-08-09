cmake_minimum_required(VERSION 3.8)

project(Rendezvous)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CONFIG_ONLY "Compile only config.json file" OFF)

if(NOT TARGET spdlog)
    # Stand-alone build
    find_package(spdlog REQUIRED)
endif()


if (CONFIG_ONLY)
    message("[INFO] Copying config files...")
    configure_file(configs/config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
    configure_file(configs/docker.json ${CMAKE_CURRENT_BINARY_DIR}/docker.json COPYONLY)
    configure_file(configs/local.json ${CMAKE_CURRENT_BINARY_DIR}/local.json COPYONLY)
    configure_file(configs/single.json ${CMAKE_CURRENT_BINARY_DIR}/single.json COPYONLY)
else()
    message("[INFO] Building rendezvous...")
    configure_file(configs/config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
    configure_file(configs/docker.json ${CMAKE_CURRENT_BINARY_DIR}/docker.json COPYONLY)
    configure_file(configs/local.json ${CMAKE_CURRENT_BINARY_DIR}/local.json COPYONLY)
    configure_file(configs/single.json ${CMAKE_CURRENT_BINARY_DIR}/single.json COPYONLY)
    include(cmake/FindgRPC.cmake)
    include(cmake/CompileProtos.cmake)
    #include(cmake/FindTBB.cmake)
    compile_protos(client)
    compile_protos(server)

    add_subdirectory(src)
    #add_subdirectory(test)
endif()