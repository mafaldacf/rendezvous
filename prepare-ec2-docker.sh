#! /bin/bash

# sudo docker-compose up rendezvous-eu client-dynamo-eu
# sudo docker-compose up rendezvous-us client-dynamo-us

KEYPAIR_PATH_EU=rendezvous-eu.pem
PUBLIC_IP_EU=3.70.201.28

KEYPAIR_PATH_US=rendezvous-us.pem
PUBLIC_IP_US=3.82.112.26


# TO BE SELECTED BY USER
if [[ "$1" == "eu" ]]; then
  KEYPAIR_PATH="~/.ssh/${KEYPAIR_PATH_EU}"
  PUBLIC_IP=${PUBLIC_IP_EU}
  REGION="eu"
elif [[ "$1" == "us" ]]; then
  KEYPAIR_PATH="~/.ssh/${KEYPAIR_PATH_US}"
  PUBLIC_IP=${PUBLIC_IP_US}
  REGION="us"
else
  echo "Invalid region argument. Available regions: 'eu', 'us'"
  exit 1
fi

scp -i $KEYPAIR_PATH docker-compose.yml ubuntu@${PUBLIC_IP}:

cmd="sudo docker login -u mafaldacf"
ssh -o StrictHostKeyChecking=no -i $KEYPAIR_PATH ubuntu@${PUBLIC_IP} $cmd

cmd="sudo docker pull mafaldacf/rendezvous-deps; sudo docker pull mafaldacf/rendezvous"
ssh -o StrictHostKeyChecking=no -i $KEYPAIR_PATH ubuntu@${PUBLIC_IP} $cmd

#cmd="sudo docker-compose up rendezvous-${REGION} client-dynamo-${REGION}"
#ssh -o StrictHostKeyChecking=no -i $KEYPAIR_PATH ubuntu@${PUBLIC_IP} $cmd