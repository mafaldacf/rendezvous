# Rendezvous

Request workflow monitor for microservice-web based applications.

Available commands:
- `Register Request` - register a new request in the system
- `Register Branch` - register a new branch concerning a previously registered request
- `Close Branch` - close a branch
- `Wait Request` - wait until a request is completed, i.e., all branches are closed given a context
- `Check Request` - check request status according to its branches
- `Check Request by Regions` - check request status per region according to its branches

## Technology Used

- C++ programming language
- gRPC: framework of remote procedure calls that supports client and server communication
- Protobuf: cross-platform data used to serialize structured data
- CMake: open-source build system generator that manages the build process

## Requirements

Install gRPC and its dependencies: [gRPC Quick Start](https://grpc.io/docs/languages/cpp/quickstart/#install-grpc) 

## How to Run

Build project

    ./start.sh make

Run server

    ./start.sh run server

Run client

    ./start.sh run client

Clean project (optional) with two variations

    ./start.sh clean
    ./start.sh fullclean

