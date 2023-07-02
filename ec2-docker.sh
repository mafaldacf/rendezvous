#! /bin/bash

# sudo docker-compose up rendezvous-eu client-dynamo-eu
# sudo docker-compose up rendezvous-us client-dynamo-us

HOSTNAME_EU="3.71.255.200"
HOSTNAME_US="18.233.5.140"

SSH_KEY_EU="~/.ssh/rendezvous-eu.pem"
SSH_KEY_US="~/.ssh/rendezvous-us.pem"

usage() {
    echo "Invalid arguments!"
    echo "Usage: ./ec2-docker.sh {eu, us} {dynamo, s3, cache, mysql}"
    exit 1
}


if [ "$#" -ne 2 ]; then
  usage
fi

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
fi

scp -i $ssh_key docker-compose.yml ubuntu@${hostname}:

cmd="sudo docker login -u mafaldacf"
ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd

cmd="sudo docker pull mafaldacf/rendezvous-deps; sudo docker pull mafaldacf/rendezvous"
ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd

cmd="sudo docker-compose up --force-recreate rendezvous-${region} subscriber-${db}-${region}"
ssh -o StrictHostKeyChecking=no -i $ssh_key ubuntu@${hostname} $cmd