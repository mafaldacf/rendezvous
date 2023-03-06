# Rendezvous

Request workflow monitor for microservice-web based applications.

Available commands:
- `Register Request` - register a new request in the system
- `Register Branch` - register a new branch concerning a previously registered request
- `Close Branch` - close a branch
- `Wait Request` - wait until a request is completed, i.e., all branches are closed given a context
- `Check Request` - check request status according to its branches
- `Check Request by Regions` - check request status per region according to its branches
- `Get Inconsistencies` - check number of inconsistencies prevented so far

## Technology Used

- C++ programming language
- gRPC: framework of remote procedure calls that supports client and server communication
- Protobuf: cross-platform data used to serialize structured data
- CMake: open-source build system generator that manages the build process
- GoogleTest: unit testing library for C++

## Getting Started

Install gRPC and its dependencies for C++: [gRPC Quick Start](https://grpc.io/docs/languages/cpp/quickstart/#install-grpc)
Install gRPC and its dependencies for Python: [gRPC Quick Start](https://grpc.io/docs/languages/python/quickstart/)

Install GoogleTest: [Generic Build Instructions: Standalone CMake Project](https://github.com/google/googletest/blob/main/googletest/README.md#standalone-cmake-project)

## How to Run

### Alternative 1: Docker

    docker build -t rendezvous .
    docker run -it -p 8000:8000 rendezvous

### Alternative 2: Host machine

Build and run project

    ./start.sh make
    ./start.sh run server
    ./start.sh run client [<script>]

Clean generated files

    ./start.sh clean

Run tests
    ./start.sh run tests