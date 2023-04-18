# Rendezvous

Request workflow monitor for microservice-web based applications.

Available commands:

- `Register Request` - register a new request in the system
- `Register Branch` - register a new branch concerning a previously registered request
- `Close Branch` - close a branch
- `Wait Request` - wait until a request is completed, i.e., all branches are closed given a context
- `Check Request` - check request status according to its branches
- `Check Request by Regions` - check request status per region according to its branches
- `Get Prevented Inconsistencies` - check number of inconsistencies prevented so far

## Requirements

- C++
- Python
- gRPC: framework of remote procedure calls that supports client and server communication
- Protobuf: cross-platform data used to serialize structured data
- CMake: open-source build system generator that manages the build process
- GoogleTest: unit testing library for C++

## Getting Started

Install gRPC and its dependencies for C++: [gRPC Quick Start](https://grpc.io/docs/languages/cpp/quickstart/#install-grpc)
Install gRPC and its dependencies for Python: [gRPC Quick Start](https://grpc.io/docs/languages/python/quickstart/)

Install GoogleTest: [Generic Build Instructions: Standalone CMake Project](https://github.com/google/googletest/blob/main/googletest/README.md#standalone-cmake-project)

## Deploying server on AWS EC2 using Docker

1. Go to AWS and create a repository in Elastic Container Registry (ECR) in `eu-central-1` region
    - Repository name: `rendezvous`
    - Obtain the repository URI

2. Push docker image to ECR
    - Install and configure aws cli to setup keys (access key & secret access key) and default region `eu-central-1`
        > sudo apt install awscli
        > aws configure
    - Build and upload docker image
    [NOTE]: when trying to login to aws ecr use the following 2nd command instead of the one provided in the aws instructions, otherwise it won't work ([ref](https://stackoverflow.com/questions/60583847/aws-ecr-saying-cannot-perform-an-interactive-login-from-a-non-tty-device-after))
    [NOTE]: make sure you tag your new docker image as stated in the 3rd command (`<ecr_uri>:latest`)
        > docker build -t rendezvous .
        > aws ecr get-login-password --region eu-central-1 | docker login --username AWS --password-stdin 851889773113.dkr.ecr.eu-central-1.amazonaws.com
        > docker tag rendezvous:latest 851889773113.dkr.ecr.eu-central-1.amazonaws.com/rendezvous:latest
        > docker push 851889773113.dkr.ecr.eu-central-1.amazonaws.com/rendezvous:latest

3. Go to AWS and create an EC2 instance:
    - Instance name: `rendezvous`
    - Generate a new key pair `<rendezvous-eu>`
    - Save the private key in a secure directory (e.g. `~/.ssh/`), otherwise you will not be able to establish a SSH connection
        > cp rendezvous-eu.pem ~/.ssh/
        > sudo chmod 400 ~/.ssh/rendezvous-eu.pem
    - Obtain the public ip of your EC2 instance
    - Go to to AWS VPC and configure the security group selected for your new instance:
      - Add the following inbound rules:
        - SSH connections on port 22 from your public IP (just to be cautious)
        - TCP connections on port 8000 from any IPv4 address (0.0.0.0/0)
        - TCP connections on port 8000 from any IPv6 address (::/0)
      - Add the following outbound rules:
        - All traffic to any IPv4 address (0.0.0.0/0)
        - All traffic to any IPv6 address (::/0)

4. Connect to your EC2 instance via SSH and retrieve docker image from repository
    - Connect via SSH
        > ssh -i "~/.ssh/rendezvous-eu.pem" ubuntu@ec2-54-93-76-92.eu-central-1.compute.amazonaws.com
        > ssh -i "~/.ssh/rendezvous-us.pem" ubuntu@ec2-44-201-115-5.compute-1.amazonaws.com
    - Install the necessary tools and configure aws cli to setup keys (access key & secret access key) and default region `eu-central-1`
        > sudo apt-get update
        > sudo apt install docker.io
        > sudo apt install awscli
        > aws configure
    - Pull the docker image
        > sudo docker login -u AWS -p $(aws ecr get-login-password --region eu-central-1) 851889773113.dkr.ecr.eu-central-1.amazonaws.com
        > sudo docker pull 851889773113.dkr.ecr.eu-central-1.amazonaws.com/rendezvous:latest

5. Run rendezvous server inside your EC2 instance

    [NOTE]: make sure you bind your server to *0.0.0.0* to listen to all interfaces, otherwise it won't work and you'll get connection refused ([ref](https://pythonspeed.com/articles/docker-connection-refused/))

    > sudo docker run -it -p 8001:8001 851889773113.dkr.ecr.eu-central-1.amazonaws.com/rendezvous
    > sudo docker run -it -p 8002:8002 851889773113.dkr.ecr.eu-central-1.amazonaws.com/rendezvous

## Running server on local Docker container

> docker build -t rendezvous .
> docker run -it -p 8000:8000 rendezvous

## Running project locally

- Build and run project

> ./start.sh make
> ./start.sh run server
> ./start.sh run client

- Clean generated files

> ./start.sh clean

- Run tests
  
> ./start.sh run tests