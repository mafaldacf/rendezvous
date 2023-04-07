#!/bin/bash

if [ "$#" -eq 1 ] && [ $1 = "clean" ]
  then
    # c++
    rm -r -f cmake/build

    # python
    rm -r -f examples/python/__pycache__
    rm -r -f examples/python/rendezvous/protos/__pycache__

elif [ "$#" -eq 1 ] && [ $1 = "build" ]
  then
    mkdir -p cmake/build
    cd cmake/build
    cmake ../..
    make clean
    make

elif [ "$#" -eq 1 ] && [ $1 = "build-config" ]
  then
    mkdir -p cmake/build
    cd cmake/build
    cmake DCONFIG_ONLY=ON ../..

elif [ "$#" -eq 1 ] && [ $1 = "build-py" ]
  then
    # need to specify package name in -I <package_name>=... for proto files to find absolute file during imports
    # https://github.com/protocolbuffers/protobuf/issues/1491
    # consequently, we have to remove part of the path from the output flags (the path will be complemented with the package name)

    # UNCOMMENT to build from global proto file in /protos/
    python3 -m grpc_tools.protoc -I rendezvous/protos=protos --python_out=examples/python --pyi_out=examples/python --grpc_python_out=examples/python protos/rendezvous.proto

    # UNCOMMENT to build from proto file in /examples/python/rendezvous/protos
    # python3 -m grpc_tools.protoc -I rendezvous/protos=examples/python/rendezvous/protos --python_out=examples/python --pyi_out=examples/python --grpc_python_out=examples/python examples/python/rendezvous/protos/rendezvous.proto

elif [ "$#" -ge 2 ] && [ "$#" -le 3 ] && [ $1 = "run" ] && [ $2 = "server" ]
  then
    cd cmake/build/src
    # Check if there are additional arguments starting from $3
    if [ "$#" -eq 3 ]
    then
      # Pass all remaining arguments starting from $3 using "$@"
      ./rendezvous $3
    else
      # No additional arguments, run ./rendezvous without any extra arguments
      ./rendezvous
    fi

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "client" ]
  then
    cd cmake/build/examples/cpp
    ./client

elif [ "$#" -eq 3 ] && [ $1 = "run" ] && [ $2 = "client" ] && [ $3 = "--stress_test" ]
  then
    cd cmake/build/examples/cpp
    ./client $3

elif [ "$#" -eq 4 ] && [ $1 = "run" ] && [ $2 = "client" ] && [ $3 = "--script" ]
  then
    cd cmake/build/examples/cpp
    ./client $3 ../../../../scripts/$4

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "client-py" ]
  then
    cd examples/python
    python3 client.py

elif [ "$#" -eq 3 ] && [ $1 = "run" ] && [ $2 = "client-py" ] && [ $3 = "--stress_test" ]
  then
    cd examples/python
    python3 client.py $3

elif [ "$#" -eq 4 ] && [ $1 = "run" ] && [ $2 = "client-py" ] && [ $3 = "--script" ]
  then
    cd examples/python
    python3 client.py $3 ../../../scripts/$4

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "tests" ]
  then
    cd cmake/build/test
    ./tests

else
    echo "Invalid arguments!"
    echo "Usage:"
    echo "(1) ./run.sh clean"
    echo "(2) ./run.sh build"
    echo "(3) ./run.sh build-config"
    echo "(4) ./run.sh build-py"
    echo "(5) ./run.sh run server [<replica id>]"
    echo "(6) ./run.sh run client [--script <script name>] | [--stress_test]"
    echo "(7) ./run.sh run client-py [--script <script name>] | [--stress_test]"
    echo "(8) ./run.sh run tests"
    exit 1
fi