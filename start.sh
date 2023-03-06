#!/bin/bash

if [ "$#" -eq 1 ] && [ $1 = "clean" ]
  then
    # C++
    rm -r -f cmake/build

    # Python
    rm -r -f examples/python/__pycache__
    rm -f examples/python/monitor*

elif [ "$#" -eq 1 ] && [ $1 = "make" ]
  then
    mkdir -p cmake/build
    cd cmake/build
    cmake ../..
    make clean
    make

elif [ "$#" -eq 1 ] && [ $1 = "build" ]
  then
    python3 -m grpc_tools.protoc -I protos --python_out=examples/python --pyi_out=examples/python --grpc_python_out=examples/python protos/monitor.proto

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "server" ]
  then
    cd cmake/build/src
    ./rendezvous

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "client-cpp" ]
  then
    cd cmake/build/examples/cpp
    ./client

elif [ "$#" -eq 3 ] && [ $1 = "run" ] && [ $2 = "client-cpp" ] && [ $3 = "--stress_test" ]
  then
    cd cmake/build/examples/cpp
    ./client $3

elif [ "$#" -eq 4 ] && [ $1 = "run" ] && [ $2 = "client-cpp" ] && [ $3 = "--script" ]
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
    echo "(2) ./run.sh make"
    echo "(3) ./run.sh build"
    echo "(4) ./run.sh run server"
    echo "(5) ./run.sh run client-cpp [--script <script name>] | [--stress_test]"
    echo "(6) ./run.sh run client-py [--script <script name>] | [--stress_test]"
    echo "(7) ./run.sh run tests"
    exit 1
fi