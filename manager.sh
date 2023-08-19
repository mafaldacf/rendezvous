#!/bin/bash

usage() {
    echo "Usage:"
    echo "> ./manager.sh local clean, build [{config, tests}], build-py-proto, run {server <replica id> <config>, tests, client, monitor}"
    echo "> ./manager.sh remote {deploy {eu, us}, update {eu, us}, start {dynamo, s3, cache, mysql}, stop}"
    echo "> ./manager.sh docker {deploy {eu, us}, start {dynamo, s3, cache, mysql}, stop}"
    echo "[INFO] Available server configs: remote.json, docker.json, local.json, single.json"
    exit 1
}

exit_usage() {
    echo "Invalid arguments!"
    usage
    exit 1
}

HOSTNAME_EU="18.184.132.153"
HOSTNAME_US="52.87.244.150"

SSH_KEY_EU="~/.ssh/rendezvous-eu.pem"
SSH_KEY_US="~/.ssh/rendezvous-us.pem"

SERVER_PORT_EU=8001
SERVER_PORT_US=8002

DOCKER_REPOSITORY_USERNAME="mafaldacf"

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
  cmake -DCONFIG_ONLY=ON ../..
  echo done!
}

local_build_tests() {
  cd metadata-server
  mkdir -p cmake/build
  cd cmake/build
  cmake -DTESTS=ON ../..
  echo done!
}

local_build_py_proto() {
  # need to specify package name in -I <package_name>=... for proto files to find absolute file during imports
  # https://github.com/protocolbuffers/protobuf/issues/1491
  # consequently, we have to remove part of the path from the output flags (the path will be complemented with the package name)

  # UNCOMMENT to build from global proto file in /protos/
  #cd metadata-server
  #python3 -m grpc_tools.protoc -I rendezvous/protos=protos --python_out=examples/python --pyi_out=examples/python --grpc_python_out=examples/python protos/rendezvous.proto

  # UNCOMMENT to build from proto file in /examples/python/rendezvous/protos
  cp metadata-server/protos/client.proto metadata-server/examples/python/rendezvous/protos/rendezvous.proto
  cp metadata-server/protos/client.proto datastore-monitor/proto/rendezvous.proto
  cd metadata-server
  python3 -m grpc_tools.protoc -I rendezvous/protos=examples/python/rendezvous/protos --python_out=examples/python --pyi_out=examples/python --grpc_python_out=examples/python examples/python/rendezvous/protos/rendezvous.proto
  cd ..
  python3 -m grpc_tools.protoc -I=datastore-monitor --python_out=datastore-monitor --pyi_out=datastore-monitor --grpc_python_out=datastore-monitor datastore-monitor/proto/rendezvous.proto
  rm metadata-server/examples/python/rendezvous/protos/rendezvous.proto
  rm datastore-monitor/proto/rendezvous.proto
  echo done!
}

local_run_server() {
  cd metadata-server/cmake/build/src
  ./rendezvous $1 $2
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

# -------
# REMOTE
# -------

remote_deploy() {
    hostname=$1
    ssh_key=$2

    ./manager.sh local clean
    echo "(1/6) Cleaned local cmake files"

    cmd="rm -rf rendezvous && mkdir rendezvous"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "(2/6) Cleaned EC2 instance workspace"

    scp -i "$ssh_key" -r metadata-server/ datastore-monitor/ server-eval/ manager.sh deps.sh  "ubuntu@$hostname:rendezvous"
    echo "(3/6) Copied project"

    cmd="sudo chmod 700 *.sh"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "(4/6) Granted access to rendezvous scripts"

    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname"
    sudo ./deps.sh
    echo "(5/6) Installed dependencies"

    ssh -i "$ssh_key" "ubuntu@$hostname"
    sudo ./manager.sh local build
    exit
    echo "(6/6) Built metadata server"

    echo ""
    echo done!
}

remote_update() {
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

remote_start() {
    hostname=$1
    ssh_key=$2
    region=$3
    datastore=$4

    cmd="cd rendezvous/metadata-server && ./manager.sh local build"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "Built project"

    cmd="cd rendezvous/metadata-server && ./manager.sh local build && ./manager.sh local run server $region remote.json"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd >/dev/null 2>&1 &
    echo "Started rendezvous server in '$region' instance"

    # datastore monitor only runs in the secondary region
    if [ $region != "eu" ]; then
        cmd="cd rendezvous/datastore-monitor && python3 main.py -r $region -d $datastore"
        ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd >/dev/null 2>&1 &
        echo "Started client process in '$region' instance"
    fi
}

remote_stop() {
    hostname=$1
    ssh_key=$2
    region=$3
    port=$4
    
    cmd="fuser -k $port/tcp"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "Killed rendezvous server process listening on port $port in '$region' instance"

    # datastore monitor only runs in the secondary region
    if [ $region != "eu" ]; then
        cmd="pkill -9 python"
        ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
        echo "Killed python client process in '$region' instance"
    fi
}

# ------
# DOCKER
# ------

docker_deploy() {
  docker build -t {DOCKER_REPOSITORY_USERNAME}/rendezvous .
  docker login -u ${DOCKER_REPOSITORY_USERNAME}
  docker push {DOCKER_REPOSITORY_USERNAME}/rendezvous .

  scp -i $ssh_key docker-compose.yml ubuntu@${hostname}:
  echo "(1/4) Copied docker compose file @ ${1}"

  cmd="sudo docker login -u ${DOCKER_REPOSITORY_USERNAME}"
  ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd
  echo "(2/4) Logged into docker @ ${1}"

  cmd="sudo docker pull mafaldacf/rendezvous-deps; sudo docker pull ${DOCKER_REPOSITORY_USERNAME}/rendezvous"
  ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd
  echo "(3/4) Pulled docker images @ ${1}"

  cmd="sudo docker tag ${DOCKER_REPOSITORY_USERNAME}/rendezvous:latest rendezvous:latest"
  ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd
  echo "(4/4) Tagged rendezvous docker image @ ${1}"

  echo ""
  echo done!
}

docker_start() {
  if [[ "$1" == "eu" ]]; then
    ssh_key=${SSH_KEY_EU}
    hostname=${HOSTNAME_EU}
    region="eu"
  elif [[ "$1" == "us" ]]; then
    ssh_key=${SSH_KEY_US}
    hostname=${HOSTNAME_US}
    region="us"
  else
    exit_usage
  fi

  if [ $2 = "dynamo" ] || [ $2 = "s3" ] || [ $2 = "cache" ] || [ $2 = "mysql" ]; then
    db=${2}
  else
    exit_usage
  fi

  cmd="sudo docker-compose up --force-recreate metadata-server-${region} datastore-monitor-${db}-${region}"
  ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd
  echo "(1/1) Started rendezvous @ ${1}"

  echo ""
  echo done!
}

docker_stop() {
  if [[ "$1" == "eu" ]]; then
    ssh_key=${SSH_KEY_EU}
    hostname=${HOSTNAME_EU}
    region="eu"
  elif [[ "$1" == "us" ]]; then
    ssh_key=${SSH_KEY_US}
    hostname=${HOSTNAME_US}
    region="us"
  else
    exit_usage
  fi

  cmd="sudo docker-compose down"
  ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd
  echo "(1/1) Stopped rendezvous @ ${1}"

  echo ""
  echo done!
}

# -------------------------------------------------------------------------------------------
# -------------------------------------------------------------------------------------------
if [ "$#" -eq 1 ] && [ $1 = "help" ]; then
  usage
# ------
# LOCAL
# ------
elif [ "$#" -eq 2 ] && [ $1 = "local" ] && [ $2 = "clean" ]; then
  local_clean
elif [ "$#" -eq 2 ] && [ $1 = "local" ] && [ $2 = "build" ]; then
  local_build
elif [ "$#" -eq 3 ] && [ $1 = "local" ] && [ $2 = "build" ] && [ $3 = "config" ]; then
  local_build_cfg
elif [ "$#" -eq 3 ] && [ $1 = "local" ] && [ $2 = "build" ] && [ $3 = "tests" ]; then
  local_build_tests
elif [ "$#" -eq 2 ] && [ $1 = "local" ] && [ $2 = "build-py-proto" ]; then
  local_build_py_proto
elif [ "$#" -eq 5 ] && [ $1 = "local" ] && [ $2 = "run" ] && [ $3 = "server" ]; then
  local_run_server $4 $5
elif [ "$#" -eq 3 ] && [ $1 = "local" ] && [ $2 = "run" ] && [ $3 = "client" ]; then
  local_run_client
elif [ "$#" -eq 3 ] && [ $1 = "local" ] && [ $2 = "run" ] && [ $3 = "monitor" ]; then
  local_run_monitor
elif [ "$#" -eq 3 ] && [ $1 = "local" ] && [ $2 = "run" ] && [ $3 = "tests" ]; then
  local_run_tests
# -------
# REMOTE
# -------
elif [ "$#" -eq 3 ] && [ $1 = "remote" ] && [ $2 = "deploy" ] && [ $3 = "eu" ]; then
  remote_deploy $HOSTNAME_EU $SSH_KEY_EU
elif [ "$#" -eq 3 ] && [ $1 = "remote" ]  && [ $2 = "deploy" ] && [ $3 = "us" ]; then
  remote_deploy $HOSTNAME_US $SSH_KEY_US
elif [ "$#" -eq 3 ] && [ $1 = "remote" ]  && [ $2 = "update" ] && [ $3 = "eu" ]; then
  remote_update $HOSTNAME_EU $SSH_KEY_EU eu
elif [ "$#" -eq 3 ] && [ $1 = "remote" ]  && [ $2 = "update" ] && [ $3 = "us" ]; then
  remote_update $HOSTNAME_US $SSH_KEY_US us
elif [ "$#" -eq 3 ] && [ $1 = "remote" ]  && [ "$2" = "start" ]; then
  remote_start $HOSTNAME_EU $SSH_KEY_EU eu
  case "$3" in 
    "dynamo" | "s3" | "cache" | "mysql")
      remote_start $HOSTNAME_US $SSH_KEY_US us $3
      ;;
    *)
      exit_usage
      ;;
  esac
elif [ "$#" -eq 2 ] && [ $1 = "remote" ]  && [ $2 = "stop" ]; then
  remote_stop $HOSTNAME_EU $SSH_KEY_EU eu $SERVER_PORT_EU
  remote_stop $HOSTNAME_EU $SSH_KEY_EU us $SERVER_PORT_US
# ------
# DOCKER
# ------
elif [ "$#" -eq 2 ] && [ $1 = "docker" ] && [ $2 = "deploy" ]; then
  docker_deploy
  docker_deploy
elif [ "$#" -eq 3 ] && [ $1 = "docker" ] && [ $2 = "start" ]; then
  docker_start eu $3
  docker_start us $3
elif [ "$#" -eq 2 ] && [ $1 = "docker" ] && [ $2 = "stop" ]; then
  docker_stop eu
  docker_stop us
else
  exit_usage
fi