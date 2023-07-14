#!/bin/bash

HOSTNAME_EU="3.123.128.138"
HOSTNAME_US="52.87.244.150"

SSH_KEY_EU="~/.ssh/rendezvous-eu-2.pem"
SSH_KEY_US="~/.ssh/rendezvous-us-2.pem"

setup() {
    hostname=$1
    ssh_key=$2
    region=$3

    cd rendezvous-server && ./rendezvous.sh clean && cd ..
    echo "Cleaned local cmake files"

    cmd="rm -rf rendezvous && mkdir rendezvous"
    ssh -o StrfictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "Cleaned EC2 instance workspace"

    scp -i "$ssh_key" -r rendezvous-server/ subscriber-process/  "ubuntu@$hostname:rendezvous"
    echo "Copied project"

    #cmd="cd rendezvous/rendezvous-server && sudo chmod 777 *.sh"
    #ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    #echo "Granted access to rendezvous scripts"

    #cmd="cd rendezvous/rendezvous-server && ./rendezvous.sh build"
    #ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    #echo "Installed dependencies"
}

update() {
    hostname=$1
    ssh_key=$2
    region=$3

    scp -i "$ssh_key" rendezvous-server/config.json "ubuntu@$hostname:rendezvous/rendezvous-server"
    echo "Copied config files to '$region' instance"

    if [ $region != "eu" ]; then
        scp -i "$ssh_key" subscriber-process/config/* "ubuntu@$hostname:rendezvous/subscriber-process/config"
        echo "Copied connections-$region.yaml file to '$region' instance"

        scp -i "$ssh_key" -r subscriber-process/*.py "ubuntu@$hostname:rendezvous/subscriber-process"
        echo "Copied python code to '$region' instance"
    fi
}

start() {
    hostname=$1
    ssh_key=$2
    region=$3
    datastore=$4

    cmd="cd rendezvous/rendezvous-server && ./rendezvous.sh build"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd
    echo "Built project"

    cmd="cd rendezvous/rendezvous-server && ./rendezvous.sh build && ./rendezvous.sh run server $region"
    ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd >/dev/null 2>&1 &
    echo "Started rendezvous server in '$region' instance"

    if [ $region != "eu" ]; then
        cmd="cd rendezvous/subscriber-process && python3 main.py -cp aws -r $region -d $datastore"
        ssh -o StrictHostKeyChecking=no -i "$ssh_key" "ubuntu@$hostname" $cmd >/dev/null 2>&1 &
        echo "Started client process in '$region' instance"
    fi
}

stop() {
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

usage() {
    echo "Invalid arguments!"
    echo "Usage:"
    echo "(0) ./start-ec2.sh setup {eu, us}"
    echo "(1) ./start-ec2.sh update {eu, us}"
    echo "(2) ./start-ec2.sh start {eu, us} {dynamo, s3, cache, mysql}"
    echo "(3) ./start-ec2.sh stop {eu, us}"
    exit 1
}

if [ "$#" -eq 2 ] && [ $1 = "setup" ] && [ $2 = "eu" ]; then
    setup $HOSTNAME_EU $SSH_KEY_EU eu

elif [ "$#" -eq 2 ] && [ $1 = "setup" ] && [ $2 = "us" ]; then
    setup $HOSTNAME_US $SSH_KEY_US us

elif [ "$#" -eq 2 ] && [ $1 = "update" ] && [ $2 = "eu" ]; then
    update $HOSTNAME_EU $SSH_KEY_EU eu

elif [ "$#" -eq 2 ] && [ $1 = "update" ] && [ $2 = "us" ]; then
    update $HOSTNAME_US $SSH_KEY_US us

elif [ "$#" -eq 3 ] && [ "$1" = "start" ] && [ "$2" = "eu" ]; then
    case "$3" in 
        "dynamo" | "s3" | "cache" | "mysql")
            start $HOSTNAME_EU $SSH_KEY_EU eu $3
            ;;
        *)
            usage
            ;;
    esac

elif [ "$#" -eq 3 ] && [ "$1" = "start" ] && [ "$2" = "us" ]; then
    case "$3" in 
        "dynamo" | "s3" | "cache" | "mysql")
            start $HOSTNAME_US $SSH_KEY_US us $3
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