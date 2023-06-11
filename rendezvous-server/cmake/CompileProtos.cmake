function(compile_protos proto_name)

    # Proto file
    get_filename_component(PROTO_FILE protos/${proto_name}.proto ABSOLUTE)
    get_filename_component(PROTO_PATH ${PROTO_FILE} PATH)

    # Generated sources
    set(PROTO_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.cc)
    set(PROTO_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.h)
    set(GRPC_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.cc)
    set(GRPC_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.h)

    add_custom_command(
    OUTPUT ${PROTO_SOURCES} ${PROTO_HEADERS} ${GRPC_SOURCES} ${GRPC_HEADERS}
    COMMAND $<TARGET_FILE:protobuf::protoc>
    ARGS --grpc_out ${CMAKE_CURRENT_BINARY_DIR}
        --cpp_out ${CMAKE_CURRENT_BINARY_DIR}
        -I ${PROTO_PATH}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${PROTO_FILE}
    DEPENDS ${PROTO_FILE})

    # Include generated *.pb.h files
    include_directories(${CMAKE_CURRENT_BINARY_DIR})

    # Libraries
    add_library(rendezvous_${proto_name}_lib
    ${GRPC_SOURCES}
    ${GRPC_HEADERS}
    ${PROTO_SOURCES}
    ${PROTO_HEADERS})
    
    target_link_libraries(rendezvous_${proto_name}_lib
    gRPC::grpc++
    protobuf::libprotobuf)

  endfunction()