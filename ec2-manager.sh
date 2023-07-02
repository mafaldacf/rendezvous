#!/bin/bash

HOSTNAME_EU="3.71.255.200"
HOSTNAME_US="18.233.5.140"

SSH_KEY_EU="~/.ssh/rendezvous-eu.pem"
SSH_KEY_US="~/.ssh/rendezvous-us.pem"

setup() {
    hostname=$1
    ssh_key=$2
    region=$3

    scp -i "$ssh_key" rendezvous-server/config.json "ubuntu@$hostname:rendezvous/rendezvous-server"
    echo "Copied config files to '$region' instance"

    scp -i "$ssh_key" subscriber-process/python/config/* "ubuntu@$hostname:rendezvous/subscriber-process/python/config"
    echo "Copied connections-$region.yaml file to '$region' instance"

    scp -i "$ssh_key" -r subscriber-process/python/*.py "ubuntu@$hostname:rendezvous/subscriber-process/python"
    echo "Copied python code to '$region' instance"
}

deploy() {
    hostname=$1
    ssh_key=$2
    region=$3
    datastore=$4

    cmd="cd rendezvous/rendezvous-server && ./start.sh build && ./start.sh run server $region"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd >/dev/null 2>&1 &
    echo "Started rendezvous server in '$region' instance"

    cmd="cd rendezvous/subscriber-process/python && python3 main.py -cp aws -r $region -d $datastore"
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
    setup $HOSTNAME_EU $SSH_KEY_EU eu
    setup $HOSTNAME_US $SSH_KEY_US us

elif [ "$#" -eq 3 ] && [ "$1" = "start" ] && [ "$2" = "eu" ]; then
    case "$3" in 
        "dynamo" | "s3" | "cache" | "mysql")
            deploy $HOSTNAME_EU $SSH_KEY_EU eu $3
            ;;
        *)
            usage
            ;;
    esac

elif [ "$#" -eq 3 ] && [ "$1" = "start" ] && [ "$2" = "us" ]; then
    case "$3" in 
        "dynamo" | "s3" | "cache" | "mysql")
            deploy $HOSTNAME_US $SSH_KEY_US us $3
            ;;
        *)
            usage
            ;;
    esac

elif [ "$#" -eq 2 ] && [ $1 = "stop" ] && [ $2 = "eu" ]; then
    stop $HOSTNAME_EU $SSH_KEY_EU eu 8001

elif [ "$#" -eq 2 ] && [ $1 = "stop" ] && [ $2 = "us" ]; then
    stop $HOSTNAME_US $SSH_KEY_US us 8002

else
    usage
fi

echo "done!"