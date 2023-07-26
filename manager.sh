#!/bin/bash

usage() {
    echo "Invalid arguments! Usage:"
    echo "> ./manager.sh local {clean, build, build-cfg, build-py, run {server <replica id>, tests, client, monitor}}"
    echo "> ./manager.sh aws {setup {eu, us}, update {eu, us}, start {eu, us {dynamo, s3, cache, mysql}}, stop {eu, us}}"
    echo "> ./manager.sh aws-docker {eu, us} {dynamo, s3, cache, mysql}"
    exit 1
}

HOSTNAME_EU="18.184.132.153"
HOSTNAME_US="52.87.244.150"

SSH_KEY_EU="~/.ssh/rendezvous-eu.pem"
SSH_KEY_US="~/.ssh/rendezvous-us.pem"

# ------
# LOCAL 
# ------

local_clean() {
  cd metadata-server
  # c++
  rm -r -f cmake/build
  # python
  rm -r -f examples/python/__pycache__
  rm -r -f examples/python/rendezvous/protos/__pycache__
  echo done!
}

local_build() {
  cd metadata-server
  mkdir -p cmake/build
  cd cmake/build
  cmake ../..
  make
  echo done!
}

local_build_cfg() {
  cd metadata-server
  mkdir -p cmake/build
  cd cmake/build
  cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../..
  echo done!
}

local_build_py() {
  # need to specify package name in -I <package_name>=... for proto files to find absolute file during imports
  # https://github.com/protocolbuffers/protobuf/issues/1491
  # consequently, we have to remove part of the path from the output flags (the path will be complemented with the package name)

  # UNCOMMENT to build from global proto file in /protos/
  cd metadata-server
  python3 -m grpc_tools.protoc -I rendezvous/protos=protos --python_out=examples/python --pyi_out=examples/python --grpc_python_out=examples/python protos/rendezvous.proto

  # UNCOMMENT to build from proto file in /examples/python/rendezvous/protos
  # python3 -m grpc_tools.protoc -I rendezvous/protos=examples/python/rendezvous/protos --python_out=examples/python --pyi_out=examples/python --grpc_python_out=examples/python examples/python/rendezvous/protos/rendezvous.proto
  echo done!
}

local_run_server() {
  cd metadata-server/cmake/build/src
  ./rendezvous $1
}

local_run_client() {
  cd metadata-server/examples/python
  python3 client.py
}

local_run_monitor() {
  cd metadata-server/examples/python
  python3 monitor.py
}

local_run_tests() {
  cd metadata-server/cmake/build/test
  ./tests
}

# ------
# AWS 
# ------

aws_setup() {
    hostname=$1
    ssh_key=$2
    region=$3

    cd metadata-server && ./rendezvous.sh clean && cd ..
    echo "Cleaned local cmake files"

    cmd="rm -rf rendezvous && mkdir rendezvous"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "Cleaned EC2 instance workspace"

    scp -i "$ssh_key" -r metadata-server/ datastore-monitor/  "ubuntu@$hostname:rendezvous"
    echo "Copied project"

    #cmd="cd rendezvous/metadata-server && sudo chmod 777 *.sh"
    #ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    #echo "Granted access to rendezvous scripts"

    #cmd="cd rendezvous/metadata-server && ./rendezvous.sh build"
    #ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    #echo "Installed dependencies"
}

aws_update() {
    hostname=$1
    ssh_key=$2
    region=$3

    scp -i "$ssh_key" metadata-server/config.json "ubuntu@$hostname:rendezvous/metadata-server"
    echo "Copied config files to '$region' instance"

    if [ $region != "eu" ]; then
        scp -i "$ssh_key" datastore-monitor/config/* "ubuntu@$hostname:rendezvous/datastore-monitor/config"
        echo "Copied connections-$region.yaml file to '$region' instance"

        scp -i "$ssh_key" -r datastore-monitor/*.py "ubuntu@$hostname:rendezvous/datastore-monitor"
        echo "Copied python code to '$region' instance"
    fi
}

aws_start() {
    hostname=$1
    ssh_key=$2
    region=$3
    datastore=$4

    cmd="cd rendezvous/metadata-server && ./rendezvous.sh build"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "Built project"

    cmd="cd rendezvous/metadata-server && ./rendezvous.sh build && ./rendezvous.sh run server $region"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd >/dev/null 2>&1 &
    echo "Started rendezvous server in '$region' instance"

    if [ $region != "eu" ]; then
        cmd="cd rendezvous/datastore-monitor && python3 main.py -r $region -d $datastore"
        ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd >/dev/null 2>&1 &
        echo "Started client process in '$region' instance"
    fi
}

aws_stop() {
    hostname=$1
    ssh_key=$2
    region=$3
    port=$4
    
    cmd="fuser -k $port/tcp"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "Killed rendezvous server process listening on port $port in '$region' instance"

    if [ $region != "eu" ]; then
        cmd="pkill -9 python"
        ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
        echo "Killed python client process in '$region' instance"
    fi
}

# -------------------------------------------------------------------------------------------
# -------------------------------------------------------------------------------------------

# ------
# LOCAL 
# ------

if [ "$#" -eq 2 ] && [ $1 = "local" ] && [ $2 = "clean" ]; then
  local_clean

elif [ "$#" -eq 2 ] && [ $1 = "local" ] && [ $2 = "build" ]; then
  local_build

elif [ "$#" -eq 2 ] && [ $1 = "local" ] && [ $2 = "build-cfg" ]; then
  local_build_cfg

elif [ "$#" -eq 2 ] && [ $1 = "local" ] && [ $2 = "build-py" ]; then
  local_build_py
    
elif [ "$#" -eq 4 ] && [ $1 = "local" ] && [ $2 = "run" ] && [ $3 = "server" ]; then
  local_run_server $4

elif [ "$#" -eq 3 ] && [ $1 = "local" ] && [ $2 = "run" ] && [ $3 = "client" ]; then
  local_run_client

elif [ "$#" -eq 3 ] && [ $1 = "local" ] && [ $2 = "run" ] && [ $3 = "monitor" ]; then
  local_run_monitor

elif [ "$#" -eq 3 ] && [ $1 = "local" ] && [ $2 = "run" ] && [ $3 = "tests" ]; then
  local_run_tests

# ------
# AWS 
# ------

elif [ "$#" -eq 3 ] && [ $1 = "aws" ] && [ $2 = "setup" ] && [ $3 = "eu" ]; then
  aws_setup $HOSTNAME_EU $SSH_KEY_EU eu

elif [ "$#" -eq 3 ] && [ $1 = "aws" ]  && [ $2 = "setup" ] && [ $3 = "us" ]; then
  aws_setup $HOSTNAME_US $SSH_KEY_US us

elif [ "$#" -eq 3 ] && [ $1 = "aws" ]  && [ $2 = "update" ] && [ $3 = "eu" ]; then
  aws_update $HOSTNAME_EU $SSH_KEY_EU eu

elif [ "$#" -eq 3 ] && [ $1 = "aws" ]  && [ $2 = "update" ] && [ $3 = "us" ]; then
  aws_update $HOSTNAME_US $SSH_KEY_US us

elif [ "$#" -eq 3 ] && [ $1 = "aws" ]  && [ "$2" = "start" ] && [ "$3" = "eu" ]; then
  aws_start $HOSTNAME_EU $SSH_KEY_EU eu

elif [ "$#" -eq 4 ] && [ $1 = "aws" ]  && [ "$2" = "start" ] && [ "$3" = "us" ]; then
  case "$4" in 
    "dynamo" | "s3" | "cache" | "mysql")
      aws_start $HOSTNAME_US $SSH_KEY_US us $4
      ;;
    *)
      usage
      ;;
  esac

elif [ "$#" -eq 3 ] && [ $1 = "aws" ]  && [ $2 = "stop" ] && [ $3 = "eu" ]; then
  aws_stop $HOSTNAME_EU $SSH_KEY_EU eu 8001

elif [ "$#" -eq 3 ] && [ $1 = "aws" ]  && [ $2 = "stop" ] && [ $3 = "us" ]; then
  aws_stop $HOSTNAME_US $SSH_KEY_US us 8002

# ------
# DOCKER
# ------

elif [ "$#" -eq 3 ] && [ $1 = "aws-docker" ]; then

  if [[ "$1" == "eu" ]]; then
    ssh_key=${SSH_KEY_EU}
    hostname=${HOSTNAME_EU}
    region="eu"
  elif [[ "$1" == "us" ]]; then
    ssh_key=${SSH_KEY_US}
    hostname=${HOSTNAME_US}
    region="us"
  else
    usage
  fi

  if [ $2 = "dynamo" ] || [ $2 = "s3" ] || [ $2 = "cache" ] || [ $2 = "mysql" ]; then
    db=${2}
  else
    usage
  fi

  scp -i $ssh_key docker-compose.yml ubuntu@${hostname}:

  cmd="sudo docker login -u mafaldacf"
  ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd

  cmd="sudo docker pull mafaldacf/rendezvous-deps; sudo docker pull mafaldacf/rendezvous"
  ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd

  cmd="sudo docker-compose up --force-recreate metadata-server-${region} datastore-monitor-${db}-${region}"
  ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd

else
  usage
fi