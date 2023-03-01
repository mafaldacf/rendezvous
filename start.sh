#!/bin/bash

if [ "$#" -eq 1 ] && [ $1 = "clean" ]
  then
    rm -r -f cmake/build

elif [ "$#" -eq 1 ] && [ $1 = "make" ]
  then
    mkdir -p cmake/build
    cd cmake/build
    cmake ../..
    make clean
    make

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "server" ]
  then
    cd cmake/build/src
    ./rendezvous

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "client" ]
  then
    cd cmake/build/examples
    ./client

# run client on stress testing mode
elif [ "$#" -eq 3 ] && [ $1 = "run" ] && [ $2 = "client" ] && [ $3 = "--stress_test" ]
  then
    cd cmake/build/examples
    ./client $3

# run client and provide script path with 4rd argument
elif [ "$#" -eq 4 ] && [ $1 = "run" ] && [ $2 = "client" ] && [ $3 = "--script" ]
  then
    cd cmake/build/examples
    ./client $3 ../../../scripts/$4

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "tests" ]
  then
    cd cmake/build/test
    ./tests

else
    echo "Invalid arguments!"
    echo "Usage:"
    echo "(1) ./run.sh clean|make"
    echo "(2) ./run.sh run server"
    echo "(3) ./run.sh run client [--script <script name>] | [--stress_test]"
    echo "(4) ./run.sh run tests"
    exit 1
fi