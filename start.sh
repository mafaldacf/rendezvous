#!/bin/bash

if [ "$#" -eq 1 ] && [ $1 = "clean" ]
  then
    cd cmake/build
    make clean

elif [ "$#" -eq 1 ] && [ $1 = "fullclean" ]
  then
    rm -r cmake
    
elif [ "$#" -eq 1 ] && [ $1 = "make" ]
  then
    mkdir -p cmake/build
    cd cmake/build
    cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../..
    make clean
    make

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "server" ]
  then
    cd cmake/build
    ./server

elif [ "$#" -eq 2 ] && [ $1 = "run" ] && [ $2 = "client" ]
  then
    cd cmake/build
    ./client

# run client and provide script path with 3rd argument
elif [ "$#" -eq 3 ] && [ $1 = "run" ] && [ $2 = "client" ]
  then
    cd cmake/build
    ./client ../../$3

else
    echo "Invalid arguments!"
    echo "Usage: ./run.sh clean|make|run server|client"
    exit 1
fi