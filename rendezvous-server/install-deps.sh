#!/bin/bash

keypairname=rendezvous-eu
ec2_ip=ec2-3-68-80-248.eu-central-1.compute.amazonaws.com
scp -i "~/.ssh/${keypairname}.pem" -r ~/.aws ubuntu@${ec2_ip}:/home/ubuntu/.aws

# if necessary, export these variables in command line before running the script
export MY_INSTALL_DIR=$HOME/.local
export PATH="$MY_INSTALL_DIR/bin:$PATH"

# Tools
echo '(1) Installing tools...'
sudo apt-get update
sudo apt-get install -y autoconf git pkg-config cmake automake libtool curl zip unzip tar make wget g++ nano libtbb-dev libspdlog-dev
sudo apt-get clean

# Prepare environemnt
sudo mkdir -p $MY_INSTALL_DIR
sudo mkdir -p ~/deps

# Update CMake
echo '(2) Updating CMake...'
cd ~/deps
wget -q -O cmake-linux.sh https://github.com/Kitware/CMake/releases/download/v3.19.6/cmake-3.19.6-Linux-x86_64.sh
sh cmake-linux.sh -- --skip-license --prefix=$MY_INSTALL_DIR
rm cmake-linux.sh

# Install gRPC and Protobuf dependencies
echo '(3) Installing gRPC and Protobuf dependencies...'
cd ~/deps
git clone --recurse-submodules -b v1.52.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build
cd cmake/build
sudo cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR ../..
sudo make -j 4
sudo make install
sudo rm -r ~/deps/grpc

# Install GTest
echo '(4) Installing GTest...'
cd ~/deps
git clone https://github.com/google/googletest.git -b v1.13.0
cd googletest
mkdir build
cd build
sudo cmake .. -DBUILD_GMOCK=OFF
sudo make
sudo make install
sudo rm -r ~/deps/googletest

# Install JSON for C++
echo '(5) Installing JSON for C++...'
cd ~/deps
git clone -b v3.11.2 --depth 1 https://github.com/nlohmann/json.git
cd json
mkdir build
cd build
sudo cmake ..
sudo cmake --install .
sudo rm -r ~/deps/json

# Install spdlog
echo '(6) Installing spdlog...'
cd ~/deps
git clone -b v1.11.0 https://github.com/gabime/spdlog.git
cd  
mkdir build
cd build
sudo cmake ..
sudo make -j
sudo rm -r ~/deps/spdlog
sudo apt-get install libspdlog-dev # idk why i need this but it only works like this

# Remaining stuff
sudo apt update
sudo apt upgrade -y 
sudo apt install build-essential -y

# Install python dependencies
cd ~/rendezvous/subscriber-process/python
sudo apt install python3-pip -y
sudo pip install -r requirements.txt

# aws cli
sudo apt install awscli -y

echo 'Done!'
