FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
ENV MY_INSTALL_DIR=$HOME/.local
ENV PATH="$MY_INSTALL_DIR/bin:$PATH"

# Tools
RUN apt-get update \
    && apt-get install -y build-essential \
    autoconf \
    git \
    pkg-config \
    cmake \
    automake \
    libtool \
    curl \
    zip \
    unzip \
    tar \
    make \
    wget \
    g++ \
    nano \
    && apt-get clean

# Create ./local directory for CMake, gRPC and Protobuf
RUN mkdir -p $MY_INSTALL_DIR

# Update CMake
RUN wget -q -O cmake-linux.sh https://github.com/Kitware/CMake/releases/download/v3.19.6/cmake-3.19.6-Linux-x86_64.sh \
    && sh cmake-linux.sh -- --skip-license --prefix=$MY_INSTALL_DIR \
    && rm cmake-linux.sh

# Install gRPC and Protobuf dependencies
RUN git clone --recurse-submodules -b v1.52.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc \
    && cd grpc \
    && mkdir -p cmake/build \
    && cd cmake/build \
    && cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR ../.. \
    && make -j 4 \
    && make install \
    && cd ../..

# Install GTest
RUN git clone https://github.com/google/googletest.git -b v1.13.0 \
    && cd googletest \
    && mkdir build \
    && cd build \
    && cmake .. -DBUILD_GMOCK=OFF \
    && make \
    && make install

# Install JSON for C++
RUN git clone -b v3.11.2 --depth 1 https://github.com/nlohmann/json.git \
    && cd json \
    && mkdir build \
    && cd build \
    && cmake .. \
    && cmake --install .

WORKDIR /app
COPY . .

#EXPOSE 8000

# Build project
RUN ./start.sh build
    
#CMD ["./start.sh", "run", "server", "${REPLICA_ID}"]