Instituto Superior Técnico

Master's Degree in Computer Science and Engineering

Thesis 2022/2023

# Rendezvous

This work introduces **Rendezvous**, a system that offers correctness guarantees and prevents consistency violations in microservice applications, including *cross-service inconsistencies* defined by [Antipode](https://dl.acm.org/doi/10.1145/3600006.3613176) authors. The work stands out for capturing all service remote calls and database operations while leaving datastores unmodified, contrary to solutions like [FlightTracker](https://www.usenix.org/conference/osdi20/presentation/shi).

The system is composed of a `metadata server` that stores the state of the request and services' actions, including writes to objects that are not immediately visible, especially in a cluster of multiple databases operating in multiple regions. It exposes an API for services to register new information at any point, which can be later queried to retrieve the set of dependencies and status that define the progress of the request or to enforce a consistent view over former actions.

Rendezvous was capable of correcting all inconsistencies with an average latency overhead of 7%. The evaluation was performed in two benchmarks ([PostNotification](https://github.com/Antipode-SOSP23/antipode-post-notification/tree/rendezvous) and [DeathStarBench](https://github.com/Antipode-SOSP23/antipode-deathstarbench/tree/rendezvous)) that presented an average of 83% of inconsistencies.

📄 [Request Workflow Monitor for Microservice-based Web Applications](https://fenix.tecnico.ulisboa.pt/cursos/meic-a/dissertacao/2535628432474134) (Instituto Superior Técnico, Masters Thesis)

## Rendezvous in Function-as-a-Service (FaaS)

The thesis also laid the foundation for a new iteration of **Rendezvous** tailored to cloud platforms (code available in the [`words23`](https://github.com/mafaldacf/rendezvous/tree/words23) branch), proposing a method for enhancing Function-as-a-Service (FaaS) platforms by transparently incorporating the framework into applications.

📄 [Rendezvous: Where Serverless Functions Find Consistency](https://dl.acm.org/doi/10.1145/3605181.3626290) (WORDS '23)

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

- `find(m)` - lookup for injected metadata in a datastore and return an acknowledgment to datastore monitor

## Requirements

- [C++](https://cplusplus.com/) (version used: 11.4.0)
- [Python](https://www.python.org/) (version used: 3.10.12)
- [Docker](https://docs.docker.com/manuals/) (version used: 20.10.21)
- [Docker Compose](https://docs.docker.com/compose/) (version used: 1.29.2)
- [gRPC](https://grpc.io/)
- [Protobuf](https://protobuf.dev/)
- [CMake](https://cmake.org/)
- [GoogleTest](http://google.github.io/googletest/)
- [nlohmann JSON](https://github.com/nlohmann/json)
- [spdlog](https://github.com/gabime/spdlog)

## Getting Started

Install Python dependencies for **official evaluation**:
```zsh
pip install -r server-eval/requirements.txt
```

*[OPTIONAL]* If running **deployment on host machine without Docker**, then install C++ dependencies:
```zsh
pip install -r datastore-monitor/requirements.txt
./deps.sh
```

## Local Deployment of Metadata Server

Go to `metadata-server/config` and configure your `params` json file as well as the connections in the `connections` folder. 
You can use `metadata-server/config/connections/single.json` to run a single server locally.

Make sure you have installed all the necessary local dependencies for Python and C++.

### Option 1: Deployment of Metadata Server on Docker Container

Build project:
```zsh
docker build -t rendezvous .
```

Run metadata server either with docker-compose or docker:
```zsh
docker-compose run metadata-server-eu
docker run -it -p 8000:8000 rendezvous ./rendezvous.sh run server eu single.json
```

### Option 2: Deployment of Metadata Server on Host Machine

Ensure you have a cleaned environment
```zsh
./rendezvous.sh local clean
```

Build and run project specifying the region and the connections filename (located in `metadata-server/config/connections/`) parameters
```zsh
./rendezvous.sh local build
./rendezvous.sh local run server {eu, us} {docker.json, local.json, remote.json, single.json}
```

Test metadata server with GoogleTests framework
```zsh
./rendezvous.sh local run tests
```

## Local Testing with Client Samples

Prior to the following steps, make sure that your metadata server is running locally.

Generate Protobuf files in Python
```zsh
./rendezvous.sh local build-py-proto
```

Run a **client** in Python that you can interact with to send requests
```zsh
./rendezvous.sh local run client
```

You can also run a dummy **datastore monitor** in Python that you can interact with to test the connection and subscriptions
```zsh
./rendezvous.sh local run monitor
```

## GCP Configuration

Setup EC2 Key Pairs
1. Go to AWS EC2 Key Pairs in `eu-central-1` and create a new key pair `rendezvous-eu`, add permissions `sudo chmod 500 rendezvous-eu.pem`, and copy to `~/.ssh/rendezvous-eu.pem`.
2. Do the same but now for `us-east-1` and name the key pair as `rendezvous-us`.

## GCP Deployment of Rendezvous (Metadata Server and Datastore Monitor)

Although Rendezvous was deployed in the host OS for the **official** evaluation, you can test `Post-Notification` microbenchmark using Docker.

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

Build and deploy metadata server:
```zsh
./rendezvous.sh docker build
./rendezvous.sh docker deploy
```

Deploy any desired datastore monitor and stop at the end if you want to switch to another:
```zsh
./rendezvous.sh docker start {dynamo, s3, cache, mysql}
./rendezvous.sh docker stop
```

*[REMINDER]* At the end, don't forget to terminate both instances in `eu-central-1` and `us-east-1`!

## Official Evaluation

This section presents a quick guide on how to deploy Rendezvous for all three evaluations.

### Post-Notification

*[WARNING]* Make sure you already setup VPC and Datastores according to the README of the `antipode-post-notification` repository!

#### GCP Configuration of EC2 AMIs

1. Go to AWS EC2 Instances in `eu-central-1` and launch a new instance with the following settings:
    - AMI: `Ubuntu 22.04 LTS`
    - Instance type: `t2.medium` (cheaper instances with fewer vCPUs will not be able to build the project with CMake)
    - Select the previously created keypair `rendezvous-eu`
    - For now you can use the default VPC and Security Group settings
2. In the local project folder, edit the `rendezvous.sh` script with the public IP and keypair path of the new instance
3. Deploy the project with `./rendezvous.sh remote deploy`. Note that this might take a long time. If you encounter any problems (especially running the dependencies script `./deps.sh` or building the C++ project `./rendezvous.sh local build`, both remotely) you should do it mannually
4. In AWS EC2 Instances, select your instance and go to ACTIONS -> IMAGES AND TEMPLATES -> CREATE IMAGE to create a new `rendezvous` AMI
5. When the AMI is ready, select it, go to ACTIONS -> COPY AMI, choose the US East (N. Virginia) (`us-east-1`) and copy the AMI

Now you have two `rendezvous` AWS EC2 AMIs in `eu-central-1` and `us-east-1` =)

#### GCP Configuration of VPC

For both `eu-central-1` and `us-east-1` do the following:
1. Create a new Security Group `rendezvous` for the Post-Notification VPC `antipode-mq`
   - Name: `antipode-mq-rendezvous`
2. Add the following inbound rules
   - SSH from any IPv4 source (0.0.0.0/0)
   - Custom TCP from any IPv4 source (0.0.0.0/0). Port is `8001` if in EU and `8002` if in US.

#### GCP Deployment of Rendezvous (Metadata Server and Datastore Monitor)

For each region (`eu-central-1` and `us-east-1`), start two EC2 instances:
   - AMI: previous created `rendezvous` image from 'My AMIs'
   - Instance type: `t2.xlarge` (4 vCPU and 16 GiB RAM)
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
```zsh
./rendezvous.sh remote update
```

Deploy any desired datastore monitor and stop at the end if you want to switch to another:
```zsh
./rendezvous.sh docker start {dynamo, s3, cache, mysql}
./rendezvous.sh docker stop
```

*[REMINDER]* At the end, don't forget to terminate both instances!

### DeathStarBench

For this benchmark, Rendezvous is deployed automatically using Docker along with the remaining microservices of DeathStarBench.

You just simply need to build Rendezvous and you are ready to go with the README instructions of the `antipode-deathstarbench` repository:
```zsh
docker build -t rendezvous .
```

In the `antipode-deathstarbench` repository, the plots and results are obtained by configuring `antipode-deathstarbench/plots/configs/rendezvous.yml`:
```zsh
./plot plots/configs/rendezvous.yml --plots throughput_latency_with_consistency_window
./plot plots/configs/rendezvous.yml --plots storage_overhead
./plot plots/configs/rendezvous.yml --plots rendezvous_info
```

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
```zsh
./master.py
```

Plot the graphs for the thesis evaluation
```zsh
./plot.py thesis
```
