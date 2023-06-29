#!/bin/bash

hostname_eu="18.197.10.189"
hostname_us="54.172.169.68"

ssh_key_eu="~/.ssh/rendezvous-eu-2.pem"
ssh_key_us="~/.ssh/rendezvous-us-2.pem"

setup() {
    hostname=$1
    ssh_key=$2
    region=$3

    scp -i "$ssh_key" rendezvous-server/config.json "ubuntu@$hostname:rendezvous/rendezvous-server"
    echo "Copied config.json file to '$region' instance"

    scp -i "$ssh_key" client-process/python/config/connections-$region.yaml "ubuntu@$hostname:rendezvous/client-process/python/config"
    echo "Copied connections-$region.yaml file to '$region' instance"
}

deploy() {
    hostname=$1
    ssh_key=$2
    region=$3
    datastore=$4

    cmd="cd rendezvous/rendezvous-server && ./start.sh build && ./start.sh run server $region"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd >/dev/null 2>&1 &
    echo "Started rendezvous server in '$region' instance"

    cmd="cd rendezvous/client-process/python && python3 main.py -cp aws -r $region -d $datastore"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd >/dev/null 2>&1 &
    echo "Started client process in '$region' instance"
}

stop() {
    hostname=$1
    ssh_key=$2
    region=$3
    port=$4
    
    cmd="fuser -k $port/tcp"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "Killed rendezvous server process listening on port $port in '$region' instance"

    cmd="pkill -9 python"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "Killed python client process in '$region' instance"
}

usage() {
    echo "Invalid arguments!"
    echo "Usage:"
    echo "(0) ./start-ec2.sh setup"
    echo "(1) ./start-ec2.sh start {eu, us} {dynamo, s3, cache, mysql}"
    echo "(2) ./start-ec2.sh stop {eu, us}"
    exit 1
}

if [ "$#" -eq 1 ] && [ $1 = "setup" ]; then
    setup $hostname_eu $ssh_key_eu eu
    setup $hostname_us $ssh_key_us us

elif [ "$#" -eq 3 ] && [ "$1" = "start" ] && [ "$2" = "eu" ]; then
    case "$3" in 
        "dynamo" | "s3" | "cache" | "mysql")
            deploy $hostname_eu $ssh_key_eu eu $3
            ;;
        *)
            usage
            ;;
    esac

elif [ "$#" -eq 3 ] && [ "$1" = "start" ] && [ "$2" = "us" ]; then
    case "$3" in 
        "dynamo" | "s3" | "cache" | "mysql")
            deploy $hostname_us $ssh_key_us us $3
            ;;
        *)
            usage
            ;;
    esac

elif [ "$#" -eq 2 ] && [ $1 = "stop" ] && [ $2 = "eu" ]; then
    stop $hostname_eu $ssh_key_eu eu 8001

elif [ "$#" -eq 2 ] && [ $1 = "stop" ] && [ $2 = "us" ]; then
    stop $hostname_us $ssh_key_us us 8002

else
    usage
fi

echo "done!"