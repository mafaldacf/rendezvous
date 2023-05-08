#!/bin/bash

# if necessary, export these variables in command line before running the script
export MY_INSTALL_DIR=$HOME/.local
export PATH="$MY_INSTALL_DIR/bin:$PATH"

# Tools
echo '(1) Installing tools...'
sudo apt-get update
sudo apt-get install -y autoconf git pkg-config cmake automake libtool curl zip unzip tar make wget g++ nano libtbb-dev libspdlog-dev
sudo apt-get clean

# Create ./local directory for CMake, gRPC and Protobuf
sudo mkdir -p $MY_INSTALL_DIR

# Update CMake
echo '(2) Updating CMake...'
wget -q -O cmake-linux.sh https://github.com/Kitware/CMake/releases/download/v3.19.6/cmake-3.19.6-Linux-x86_64.sh
sh cmake-linux.sh -- --skip-license --prefix=$MY_INSTALL_DIR
rm cmake-linux.sh

# Install gRPC and Protobuf dependencies
echo '(3) Installing gRPC and Protobuf dependencies...'
git clone --recurse-submodules -b v1.52.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build
cd cmake/build
sudo cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR ../..
sudo make -j 2
sudo make install
cd ../..
sudo rm -r grpc

# Install GTest
echo '(4) Installing GTest...'
git clone https://github.com/google/googletest.git -b v1.13.0
cd googletest
mkdir build
cd build
sudo cmake .. -DBUILD_GMOCK=OFF
sudo make
sudo make install
cd ../..
sudo rm -r googletest

# Install JSON for C++
echo '(5) Installing JSON for C++...'
git clone -b v3.11.2 --depth 1 https://github.com/nlohmann/json.git
cd json
mkdir build
cd build
sudo cmake ..
sudo cmake --install .
cd ../..
sudo rm -r json

# Install spdlog
echo '(6) Installing spdlog...'
git clone -b v${SPDLOG_VERSION} https://github.com/gabime/spdlog.git
cd spdlog
mkdir build
cd build
cmake ..
make -j
cd ../..
rm -rf spdlog

echo 'Done!'