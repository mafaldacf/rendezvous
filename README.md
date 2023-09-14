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

Local deployment without Docker:

- [gRPC](https://grpc.io/)
- [Protobuf](https://protobuf.dev/)
- [CMake](https://cmake.org/)
- [GoogleTest](http://google.github.io/googletest/)
- [nlohmann JSON](https://github.com/nlohmann/json)
- [spdlog](https://github.com/gabime/spdlog)

## Getting Started

Install Python dependencies for **official evaluation**:

    pip install -r server-eval/requirements.txt

Install Python dependencies for local deployment **without Docker** (optional):

    pip install -r datastore-monitor/requirements.txt

Install C++ dependencies for local deployment **without Docker** (optional):

    ./deps.sh

## Running Rendezvous Metadata Server Locally

Go to `metadata-server/config` and configure your `params` json file as well as the connections in the `connections` folder. 
You can use `metadata-server/config/connections/single.json` to run a single server locally.

Make sure you have installed all the necessary local dependencies for Python and C++.

### Metadata Server Deployment (1/2)

Build and run project for the following available parameters:
- region: `eu`, `us`
- connections_filename: `docker.json`, `local.json`, `remote.json`, `single.json`

    ./rendezvous.sh local build
    ./rendezvous.sh local run server <region> <connections_filename>

Clean generated files

    ./rendezvous.sh local clean

Test metadata server with GoogleTests framework
  
    ./rendezvous.sh local run tests

### Metadata Server Deployment (2/2) using Docker

Build project:

    docker build -t rendezvous .

Run with docker:

    docker run -it -p 8000:8000 rendezvous ./rendezvous.sh run server eu single.json

Or run with docker-compose:

    docker-compose run metadata-server-eu

### Testing Metadata Server with Client Samples

Generate Python Protobuf files

    ./rendezvous.sh local build-py-proto

Run the metadata server:

    ./rendezvous.sh local run server eu single.json

Option 1/2: Run **client** in Python

    ./rendezvous.sh local run client

Option 2/2: Run **datastore monitor** in Python that simply subscribes the server

    ./rendezvous.sh local run monitor

## Evaluation

This section presents a quick guide on how to deploy Rendezvous for all three evaluations.

### Getting Started

**SKIP** this section if you are running rendezvous using Docker (e.g. for post-notification micro-benchmark)!!!

#### Setting up EC2 AMIs and Key Pairs

1. Go to AWS EC2 Key Pairs in `eu-central-1` and create a new key pair `rendezvous-eu`, add permissions `sudo chmod 500 rendezvous-eu.pem`, and copy to `~/.ssh/rendezvous-eu.pem`.
2. Do the same but now for `us-east-1` and name the key pair as `rendezvous-us`.
3. Go to AWS EC2 Instances in `eu-central-1` and launch a new instance with the following settings:
    - AMI: `Ubuntu 22.04 LTS`
    - Instance type: `t2.medium` (cheaper instances with fewer vCPUs will not be able to build the project with CMake)
    - Select the previously created keypair `rendezvous-eu`
    - For now you can use the default VPC and Security Group settings
4. In the local project folder, edit the `rendezvous.sh` script with the public IP and keypair path of the new instance
5. Deploy the project with `./rendezvous.sh remote deploy`. Note that this might take a long time. If you encounter any problems (especially running the dependencies script `./deps.sh` or building the C++ project `./rendezvous.sh local build`, both remotely) you should do it mannually :(
6. In AWS EC2 Instances, select your instance and go to ACTIONS -> IMAGES AND TEMPLATES -> CREATE IMAGE to create a new `rendezvous` AMI
7. When the AMI is ready, select it, go to ACTIONS -> COPY AMI, choose the US East (N. Virginia) (`us-east-1`) and copy the AMI

Now you have two `rendezvous` AWS EC2 AMIs in `eu-central-1` and `us-east-1` =)

### Post-Notification

#### VPC Configuration

For this microbenchmark, Rendezvous is deployed in two EC2 `t2.xlarge` (4 vCPU and 16 GiB RAM) instances for both primary (`eu-central-1`) and secondary (`us-east-1`) regions. 

REQUIREMENTS: make sure you already setup the Post-Notification VPC according to the README of the `antipode-post-notification` repository.

For both `eu-central-1` and `us-east-1` do the following:
1. Create a new Security Group `rendezvous` for the Post-Notification VPC `antipode-mq`
   - Name: `antipode-mq-rendezvous`
2. Add the following inbound rules
   - SSH from any IPv4 source (0.0.0.0/0)
   - Custom TCP from any IPv4 source (0.0.0.0/0). Port is `8001` if in EU and `8002` if in US.

#### Alternative 1/2 (OFFICIAL EVAL): Running Rendezvous in Native OS

For each region (`eu-central-1` and `us-east-1`), start two EC2 instances:
   - AMI: previous created `rendezvous` image from 'My AMIs'
   - Instance type: `t2.xlarge`
   - Keypair: `rendezvous-eu` in EU or `rendezvous-us` in US
   - Select the `antipode-mq` VPC and the previously created Security Group `antipode-mq-rendezvous`
   - Make sure to **ENABLE** assignment of public IP

Now that both instances are running, go to your local project folder:
   1. Edit the main `rendezvous.sh` script parameters for public IPs and keypair paths
   2. Edit the metadata server `metadata-server/config/connections/remote.json` config with the public IPs
   3. Make sure that both servers' connections identified by `rendezvous` in `datastore-monitor/config/connections.yaml` match the following:
       - `eu-central-1`: `localhost:8001` 
       - `us-east-1`: `localhost:8002`

Update the config file with the new ips to the remote instances:

    ./rendezvous.sh remote update

For each deployment:

    ./rendezvous.sh remote start {dynamo, s3, cache, mysql}
    ./rendezvous.sh remote stop

**REMINDER**: At the end, don't forget to terminate both instances.

#### Alternative 2/2 (CONVENIENCE TESTING): running Rendezvous using Docker

Although Rendezvous was deployed in native OS for the **official** evaluation with Post-Notification, it can also be deployed using Docker, which is way easier.

Install `aws cli` if not yet done and configure your credentials with `aws configure`.

For each region (`eu-central-1` and `us-east-1`), start two EC2 instances:
   - AMI: `Ubuntu 22.04 LTS`
   - Instance type: user preference (can be `t2.micro`)
   - Keypair: `rendezvous-eu` in EU or `rendezvous-us` in US
   - Select the `antipode-mq` VPC and the previously created Security Group `antipode-mq-rendezvous`
   - Make sure to **ENABLE** assignment of public IP

Now that both instances are running, go to your local project folder:
   1. Edit the main `rendezvous.sh` script parameters for public IPs and keypair paths
   2. Edit the metadata server `metadata-server/config/remote.json` config with the public IPs
   3. Make sure that both servers' connections identified by `rendezvous` in `datastore-monitor/config/connections.yaml` match the following:
       - `eu-central-1`: `rendezvous-eu:8001` 
       - `us-east-1`: `rendezvous-us:8002`

Build and deploy rendezvous:

    ./rendezvous.sh docker build
    ./rendezvous.sh docker deploy

For each deployment:

    ./rendezvous.sh docker start {dynamo, s3, cache, mysql}
    ./rendezvous.sh docker stop

**REMINDER**: At the end, don't forget to terminate both instances.

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
   - Instance type: `t2.xlarge` for the `metadata server` and `t2.large` for the `clients`
   - Keypair: `rendezvous-eu`
   - Select the `antipode-mq` VPC and the previously created Security Group `antipode-mq-rendezvous`
   - Make sure to **ENABLE** assignment of public IP
2. Configure your instance connections in `server-eval/configs/settings.yaml`
3. If desired, configure your deployment types in `server-eval/configs/master.yaml` (by default, these correspond to the combinations used in the **official** evaluation):
    - If `words23` is enabled, the metadata variations corresponds to number of datastores (ideally ranging 1, 5, 10, 15 and 20)
    - Otherwise, the metadata variations corresponds to number of regions (ideally ranging 1, 10 and 100) used in the thesis evaluation.

You can now relax and run the evaluation from the `server-eval` folder

    ./master.py

Plot for `thesis` or `words23`. If doing it for `words23`, you can add the flag `--annotate` to hardcode annotate the datapoints (these can be changed in the python script)

    ./plot.py {thesis, words23}