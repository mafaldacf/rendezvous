Instituto Superior Técnico

Master's Degree in Computer Science and Engineering

Thesis 2022/2023

# Rendezvous: Request Workflow Monitor for Microservice-based Web Applications

## Rendezvous API Overview

**Metadata Server API**

- `Register Request` - register a new request 
- `Register Branch` - register a new branch
- `Close Branch` - close a branch
- `Wait Request` - wait until a request is completed for a given context
- `Check Request` - check request status for a given status
- `Check Request by Regions` - check request status per region for a given status

**Client Shim API**

- `write(k, v, m)` - intermediates communication between a subsystem and a datastore and injects rendezvous metadata

**Datastore Monitor Shim API**

- `find(m)` - lookup for injected metadata in a datastore and return acknowledgment to datastore monitor

## Requirements

- **C++** (version used: 11.4.0)
- **Python** (version used: 3.10.12)
- **Docker** (version used: 20.10.21)
- **Docker Compose** (version used: 1.29.2)

Local deployment:

- [gRPC](https://grpc.io/): *Modern open source high performance Remote Procedure Call (RPC) framework*
- [Protobuf](https://protobuf.dev/): *Language-neutral, platform-neutral extensible mechanisms for serializing structured data*
- [CMake](https://cmake.org/): *Open-source, cross-platform family of tools designed to build, test and package software*
- [GoogleTest](http://google.github.io/googletest/): *Google's C++ testing and mocking framework*
- [nlohmann JSON](https://github.com/nlohmann/json): *JSON for Modern C++*
- [spdlog](https://github.com/gabime/spdlog): *Very fast, header-only/compiled, C++ logging library*

## Getting Started

Install Python dependencies for evaluation:

    pip install -r server-eval/requirements.txt

Install Python dependencies for local deployment (optional):

    pip install -r datastore-monitor/requirements.txt

Install C++ dependencies for local deployment (optional):

    ./deps.sh

## Testing Rendezvous Metadata Server Locally

Go to `metadata-server/configs` and configure your own json file. 
You can use `single.json` to run a single server locally.

Make sure you have installed all the necessary local dependencies for Python and C++.

### Metadata Server Deployment (1/2): Native OS

Build and run project

    ./rendezvous.sh local build
    ./rendezvous.sh local run server <region> <config>

Clean generated files

    ./rendezvous.sh local clean

Run GoogleTest tests
  
    ./rendezvous.sh local run tests

### Metadata Server Deployment (2/2): Docker

Build project:

    docker build -t rendezvous .

Run with docker:

    docker run -it -p 8000:8000 rendezvous ./rendezvous.sh run server eu single.json

Or run with docker-compose:

    docker-compose run metadata-server-eu

### Testing Metadata Server with a Simple Client

Generate Python Protobuf files

    ./rendezvous.sh local build-py-proto

Run your client

    ./rendezvous.sh local run client

You can also test the monitor example that simply subscribes the server

    ./rendezvous.sh local run monitor

## Evaluation

This section presents a quick guide on how to deploy Rendezvous for all three evaluations.

### Getting Started

Both Post-Notification and Rendezvous Metadata Server evaluations will be using AWS EC2 instances. For that reason, we create the following AMI:

1. Go to AWS EC2 in `eu-central-1` and create a new key pair `rendezvous-eu`, add permissions `sudo chmod 500 rendezvous-eu.pem`, and copy to `~/.ssh/rendezvous-eu`.
2. Go to Instances and launch a new instance with the following settings:
    - AMI: `Ubuntu 22.04 LTS`
    - Instance type: `t2.medium` (cheaper instances with fewer vCPUs will not be able to build the project with CMake)
    - Select the previously created keypair `rendezvous-eu`
    - For now you can use the default VPC and Security Group settings
3. In the local project folder, edit the `rendezvous.sh` script with the public IP and keypair path of the new instance
4. Upload the project with `./rendezvous.sh remote deploy`. Note that this might take a long time. If you encounter any problems (especially running the dependencies script or building the C++ project) you should do it mannually
5. In AWS EC2 Instances, select your instance and go to 'Actions' -> 'Image and Templates' -> 'Create Image' to create a new `rendezvous` AMI
6. When the AMI is ready, select it, go to 'Actions' -> 'Copy AMI', choose the US East (N. Virginia) (`us-east-1`) and copy the AMI

Now you have two `rendezvous` AWS EC2 AMIs in `eu-central-1` and `us-east-1` =)

At the end, perform the 1st step but now from `us-east-1` region and `rendezvous-us` keypair.

### Post-Notification

For this microbenchmark, Rendezvous is deployed AWS, in two EC2 `t2.xlarge` (4 vCPU and 16 GiB RAM) instances for both primary (`eu-central-1`) and secondary (`us-east-1`) regions. Before preciding, make sure you already setup the Post-Notification VPC according to the README of the `antipode-post-notification` repository.

For both `eu-central-1` and `us-east-1` do the following:
1. Create a new Security Group `rendezvous` for the Post-Notification VPC `antipode-mq`
2. Add the following inbound rules
   - SSH from any IPv4 source (0.0.0.0/0)
   - Custom TCP from any IPv4 source (0.0.0.0/0). Port is `8001` if in EU and `8002` if in US.
3. Launch a new EC2 instance:
   - AMI: previous created `rendezvous` image from 'My AMIs'
   - Instance type: `t2.xlarge`
   - Keypair: `rendezvous-eu` if in EU and `rendezvous-us` if in US
   - Select the `antipode-mq` VPC and the previously created Security Group
   - Make sure a public IP is assigned (for deployment with `redis`, the private IP is the one used for the connections file of the Post Notification)

Now that both instances are running, go to your local project folder  
    - Edit the `rendezvous.sh` parameters for public IPs and keypair paths
    - Edit the `metadata-server/configs/remote.json` with the public IPs


Prior to each Post-Notification deployment of the post-storage (dynamo, s3, cache, redis) and notification-storage (sns), run:
    
    ./rendezvous.sh remote start {dynamo, s3, cache, mysql}

After each deployment, stop Rendezvous:

    ./rendezvous.sh remote stop 

At the end, don't forget to terminate both instances.

#### (Easy and) Alternative Rendezvous Deployment

Although Rendezvous was deployed in native OS for the **official** evaluation with Post-Notification, it can also be deployed using Docker, which is way easier. 

After creating the EC2 instance for both regions (`eu-central-1` and `us-east-1`) with the correct Security Groups and SSH Key Pairs:
1. Make sure you already have `aws cli` installed and configured with your credentials running `aws configure`.
2. Configure the `rendezvous.sh` script with the necessary parameters
3. Configure the `metadata-server/configs/remote.json` file with the EC2 instances public ips
4. Make sure the `datastore-monitor/config/connections.yaml` file for rendezvous server corresponds to the hostname used in the docker compose file (`rendezvous-eu` and `rendezvous-us`)

Build and deploy rendezvous:

    ./rendezvous.sh docker build
    ./rendezvous.sh docker deploy

For each deployment:
    ./rendezvous.sh docker start {dynamo, s3, cache, mysql}
    ./rendezvous.sh docker stop

### DeathStarBench

For this benchmark, Rendezvous is deployed using Docker along with the remaining microservices.

You just simply need to build Rendezvous and you are ready to go with the README instructions of the `antipode-deathstarbench` repository:

    docker build -t rendezvous .

In the `antipode-deathstarbench` repository, the plots and results are obtained by configuring `plots/configs/rendezvous.yml`:

    ./plot plots/configs/rendezvous.yml --plots throughput_latency_with_consistency_window
    ./plot plots/configs/rendezvous.yml --plots storage_overhead
    ./plot plots/configs/rendezvous.yml --plots rendezvous_info

### Rendezvous Metadata Server

For this evaluation, we deployed 1 metadata server and 1-5 clients in AWS using EC2 instances in `eu-central-1` with the previously created AMI:

1. Go to AWS EC2 and launch 1 instance for the `metadata server` and 5 instances for the `clients` (you can select the number of instances to launch instead of doing it manually). For each instance, use the following settings:
   - AMI: previous created `rendezvous` image from 'My AMIs'
   - Instance type: `t2.xlarge`
   - Keypair: `rendezvous-eu`
   - Select the `antipode-mq` VPC and the previously created Security Group
   - Make sure a public IP is assigned
2. Configure your instance connections in `server-eval/configs/settings.yaml`
3. Configure your deployment types (by default, these correspond to the combinations used in the **official** evaluation)

You can now relax and run the evaluation from the `server-eval` folder:

    ./eval_master.py