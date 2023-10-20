#!/usr/bin/env python3

from __future__ import print_function
import argparse
import time
import threading
import grpc
import datetime
from datetime import datetime
import paramiko
from scp import SCPClient
from tqdm import tqdm
import time
from proto import rendezvous_pb2 as pb
from proto import rendezvous_pb2_grpc as rv
from threading import Lock
import yaml
import os
import pandas as pd
import glob
import shutil
import sys

EVAL_DIR = 'server-eval'
GATHER_DELAY_CURRENT_THREAD = 5
GATHER_DELAY_REMOTE_CLIENTS = 10

# start off by loading the configuration file
with open('configs/settings.yml', 'r') as file:
    config = yaml.safe_load(file)
    RESULTS_DIR = str(config['results_dir'])
    SSH_KEY_PATH = str(config['ssh_key_path'])
    CLIENTS_IP = list(config['clients_ip'])
    SERVER_IP = str(config['server_ip'])
    SERVER_ADDRESS = str(SERVER_IP) + ':' + str(config['server_port'])                          

class EvalClient():
    def __init__(self, duration=None, threads=None, metadata=None, words23=None, clients=None, timestamp=None, variation=None, client_id=None):
        self.channel = grpc.insecure_channel(SERVER_ADDRESS)
        self.stub = rv.ClientServiceStub(self.channel)
        self.do_send_requests = True
        self.results = {}
        self.mutex = Lock()

        # eval parameters
        self.duration = duration
        self.threads = threads
        self.metadata = metadata
        self.words23 = words23
        self.clients = clients
        self.timestamp = timestamp if timestamp else datetime.now().strftime('%Y%m%d%H%M')
        self.variation = variation
        self.client_id = client_id

    def _gather_remote_client(self, client_id, client_address):
        results = {}
        client = _ssh_connect(client_address)
        print(f"[GATHER] Gathering results of client {client_id} @ {client_address}")
        
        _, stdout, _ = client.exec_command(f"cat {EVAL_DIR}/{self.timestamp}.info")
        stdout = stdout.read().decode()
        lines = stdout.strip().split('\n')
        for line in lines:
            for key, value_type in {'requests': int, 'responses': int, 'throughput': float, 'avg_latency': float}.items():
                if line.lower().startswith(key + ':'):
                    results[key] = value_type(line.split(':')[1].strip())

        os.makedirs('remote_csv_results', exist_ok=True)
        with SCPClient(client.get_transport()) as scp:
            scp.get(f'/home/ubuntu/{EVAL_DIR}/{self.timestamp}.csv', f'remote_csv_results/{self.timestamp}_{client_id}.csv')

        client.close()
        return results

    def _gather_remote_clients(self):

        # display progress bar with a small delay of 5
        waiting_time = self.duration + GATHER_DELAY_REMOTE_CLIENTS
        print(f"[GATHER] Waiting for all results file in remote for {waiting_time} seconds")
        progress_bar = tqdm(total=waiting_time, desc='Progress', bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt}')
        for _ in range(waiting_time):
            time.sleep(1)
            progress_bar.update(1)
        progress_bar.close()

        # prepare directories
        if self.variation: 
            base_dir = f'{RESULTS_DIR}/{self.variation}'
        else:
            base_dir = f'{RESULTS_DIR}'
        if self.words23:
            type_prefix = f"datastores"
        else:
            type_prefix = f"regions"
        os.makedirs(base_dir, exist_ok=True)
        results_name = f"{type_prefix}_{self.metadata}_clients_{self.clients}_duration_{self.duration}_threads_{self.threads}__{self.timestamp}"
        
        # gather all remote clients results
        clients_results = []
        for client_id in range(self.clients):
            client_address = CLIENTS_IP[client_id]
            clients_results.append(self._gather_remote_client(client_id, client_address))

        # gather csv results
        for csv_result_file in glob.glob(os.path.join('remote_csv_results', f'{self.timestamp}*')):
            with open(csv_result_file, 'r') as client_file:
                content = client_file.read()
            with open(f'{base_dir}/{results_name}.csv', 'a+') as local_file:
                local_file.write(content)
        shutil.rmtree('remote_csv_results')
    
        
        # write results to output
        with open(f'{base_dir}/{results_name}.info', 'w') as file:
            final_results = {'requests': 0, 'responses': 0, 'throughput (req/s)': 0.0, 'avg_latency (ms)': 0}
            for client_id, result in enumerate(clients_results):
                final_results['requests'] += result['requests']
                final_results['responses'] += result['responses']
                final_results['throughput (req/s)'] += result['throughput']
                final_results['avg_latency (ms)'] += result['avg_latency']
                file.write(f"Client {client_id} @ {CLIENTS_IP[client_id]}: {result}\n")
            
            final_results['avg_latency (ms)'] = final_results['avg_latency (ms)']/len(clients_results)
            file.write("----------------------------\n")
            file.write(f"Duration: {self.duration}\n")
            file.write(f"Clients: {self.clients}\n")
            file.write(f"Threads per Client: {self.threads}\n")
            file.write(f"Requests: {final_results['requests']}\n")
            file.write(f"Responses: {final_results['responses']} ({(final_results['responses']/final_results['requests'])*100}%)\n")
            file.write(f"Throughput (req/s): {round(final_results['throughput (req/s)'], 2)}\n")
            file.write(f"Latency (ms): {round(final_results['avg_latency (ms)'], 2)}\n")

            print(f"[INFO] Final Results: {final_results}")
        
        if final_results['responses'] != final_results['requests']:
            print(f"[WARNING] Missing {final_results['requests']-final_results['responses']} responses!")

    def _gather_local_thread(self, task_id, requests, responses, latencies):
        with self.mutex:
            # round to float with 2 decimal values
            #for i in range(len(latencies)):
            #    latencies[i] = round(latencies[i], 2)
            self.results[task_id] = (requests, responses, latencies)
    
    def _gather_local_threads(self):
        num_requests = 0
        num_responses = 0
        sum_avg_latencies = 0
        latency_values = []

        for thread_id, (requests, responses, latencies) in self.results.items():
            num_requests += requests
            num_responses += responses
            thread_sum_latencies = 0

            # compute avg latency for current thread
            for latency in latencies:
                thread_sum_latencies += latency

                # save all latency values
                latency_values.append({
                    'client_id': self.client_id,
                    'thread_id': thread_id,
                    'latency': latency
                })

            sum_avg_latencies += thread_sum_latencies/responses
            thread_id += 1
        
        throughput = num_requests/self.duration
        avg_latency = sum_avg_latencies/len(self.results)
        avg_latency_round = round(avg_latency, 2)

        # write global avg results
        with open(f"{self.timestamp}.info", 'w') as file:
            file.write(f"requests: {num_requests}\n")
            file.write(f"responses: {num_responses}\n")
            file.write(f"throughput: {throughput}\n")
            file.write(f"avg_latency: {avg_latency_round}\n")

        # write csv file with all latencies
        df = pd.DataFrame(latency_values)
        df.to_csv(f'{self.timestamp}.csv', sep=';', mode='w')


    def _send_requests(self, task_id):
        requests = 0
        responses = 0
        latencies = []

        # register request just for sanity check
        self.stub.RegisterRequest(pb.RegisterRequestMessage(rid=str(task_id)))
        
        if self.words23:
            rpc = self.stub.RegisterBranchesDatastores
            datastores = []
            for i in range(self.metadata):
                datastores.append(f"fabulous-datastore-{i}")
            request = pb.RegisterBranchesDatastoresMessage(rid=str(task_id), datastores=datastores, regions=["EU", "US"])
        else:
            rpc = self.stub.RegisterBranch
            regions = []
            for i in range(self.metadata):
                regions.append(f"fabulous-region-{i}")
            request = pb.RegisterBranchMessage(rid=str(task_id), service="service", regions=regions)
        
        while self.do_send_requests:
            try:
                requests += 1
                start_ts = datetime.utcnow().timestamp()
                rpc(request, timeout=30)
                end_ts = datetime.utcnow().timestamp()
                latency_ms = (end_ts - start_ts) * 1000
                latencies.append(latency_ms)
                responses += 1

            except grpc.RpcError as e:
                print("[ERROR]", e.details())
            
        time.sleep(GATHER_DELAY_CURRENT_THREAD)
    
        self._gather_local_thread(task_id, requests, responses, latencies)

    def run_local(self):
        thread_pool = []

        for i in range(self.threads):
            self.results[i] = None
            thread_pool.append(threading.Thread(target=self._send_requests, args=(i, )))

        for t in thread_pool:
            t.start()

        time.sleep(self.duration)
        self.do_send_requests = False

        for t in thread_pool:
            t.join()

        self._gather_local_threads()

    def _run_remote_client(self, client_id, client_address):
        client = _ssh_connect(client_address)
        print(f"[EVAL] Running eval for client {client_id} @ {client_address}")
        
        cmd = f'./eval.py run-local -c {client_id} -d {self.duration} -t {self.threads} -m {self.metadata} -ts {self.timestamp}'
        if self.words23:
            cmd += ' -w'
        client.exec_command(f"cd {EVAL_DIR} && {cmd}")
        client.close()
            
    def run_clients(self):
        print(f"[EVAL] START TIME: {self.timestamp}")
        thread_pool = []

        for client_id in range(self.clients):
            client_addr = CLIENTS_IP[client_id]
            thread_pool.append(threading.Thread(target=self._run_remote_client, args=(client_id, client_addr)))
        
        for t in thread_pool:
            t.start()

        for t in thread_pool:
            t.join()

        self._gather_remote_clients()

def _ssh_connect(ip):
    try:
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        private_key = paramiko.RSAKey.from_private_key_file(SSH_KEY_PATH)
        client.connect(ip, username='ubuntu', pkey=private_key)
        return client
    except Exception as e:
        print(f"[ERROR] An error occurred for {SERVER_IP}: {e}")
        exit(-1)

def deploy_clients():
    for client_id, client_hostname in enumerate(CLIENTS_IP):
        client = _ssh_connect(client_hostname)
        print(f"[DEPLOY] Deploying client {client_id} @ {client_hostname}")
        # kill existing python programs from previous interrupted executions
        client.exec_command("pkill -9 python")
        # clean previous results and code
        client.exec_command(f"rm -rf {EVAL_DIR} && mkdir -p {EVAL_DIR}")
        with SCPClient(client.get_transport()) as scp:
            base_dir = f'/home/ubuntu/{EVAL_DIR}'
            scp.put('eval.py', base_dir)
            scp.put('requirements.txt', base_dir)
            scp.put('configs', base_dir, recursive=True)
            scp.put('proto', base_dir, recursive=True)

        _, _, stderr = client.exec_command(f"cd {EVAL_DIR} && sudo pip install -r requirements.txt")
        stderr = stderr.read().decode()
        client.close()
        if len(stderr) > 0 and 'WARNING: Running pip as the' not in stderr:
            print(f"[ERROR] {stderr}")
            exit(-1)
    print("done!")

def _restart_server_native(client):
    # if we want to manually debug the server output
    # connect to the tmux session with:
    # > tmux attach-session -t rendezvous-server
    tmux_session_name = 'rendezvous-server'
    tmux_command = './rendezvous.sh local run server eu single.json'

    client.exec_command(f"tmux kill-session -t {tmux_session_name}")
    # start server but do not wait for command to be executed
    _, _, stderr = client.exec_command(f"cd rendezvous && tmux new-session -s {tmux_session_name} -d '{tmux_command}'")
    # add delay
    time.sleep(2)

    # make sure server is running
    _, _, stderr = client.exec_command(f"fuser 8001/tcp")
    stderr = stderr.read().decode()
    # for some reason the output goes to the stderr
    if len(stderr) > 0 and '8001/tcp' in stderr:
        print("done!")
    else:
        print(f"[ERROR] Could not start server {stderr}")
        exit(-1)

def _restart_server_docker(client):
    container_name = "ubuntu_metadata-server-eu_1"
    # kill previous server (faster than compose down)
    client.exec_command(f"docker kill {container_name}")
    # start new server
    client.exec_command(f"docker-compose up -d metadata-server-eu", get_pty=True)
    time.sleep(1)

    # make sure the server is running
    _, stdout, _ = client.exec_command(f"docker ps --format \"{{.Names}}\" | grep {container_name}")
    stdout = stdout.read().decode()
    if stdout and stdout[0] != container_name:
        print(f"[ERROR] Could not restart server: {stdout}")
        exit(-1)
    # make sure the server is the only container running
    _, stdout, _ = client.exec_command(f"docker ps --format \"{{.Names}}\" | wc -l")
    stdout = stdout.read().decode()
    if stdout and stdout[0] != '1':
        print(f"[ERROR] Could not restart server: {stdout}")
        exit(-1)
    print("done!")

def restart_server(docker):
    client = _ssh_connect(SERVER_IP)
    if docker:
        print(f"[INFO] Restarting metadata server container @ {SERVER_IP}...")
        _restart_server_docker(client)
    else:
        print(f"[INFO] Restarting metadata server @ {SERVER_IP}...")
        _restart_server_native(client)
    client.close()

def deploy_server(docker):
    client = _ssh_connect(SERVER_IP)
    if docker:
        print(f"[DEPLOY] Deploying metadata server container @ {SERVER_IP}...")
        print(f"[INFO] Pulling docker image! Might take some seconds...")
        _, _, stderr = client.exec_command(f"docker pull mafaldacf/rendezvous")
        stderr = stderr.read().decode()
        if stderr:
            print(f"[ERROR] Could not pull docker image: {stderr}")
        with SCPClient(client.get_transport()) as scp:
            base_dir = f'/home/ubuntu'
            scp.put('../docker-compose.yml', base_dir)
    client.close()
    print("done!")

if __name__ == '__main__':
    main_parser = argparse.ArgumentParser()
    command_parser = main_parser.add_subparsers(help='commands', dest='command')

    run_local_parser = command_parser.add_parser('run-local', help="Run client application locally")
    run_local_parser.add_argument('-d', '--duration', type=int, default=2, help="Duration in s")
    run_local_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    run_local_parser.add_argument('-m', '--metadata', type=int, default=1, help="Metadata size (regions or datastores)")
    run_local_parser.add_argument('-ts', '--timestamp', type=str, default=datetime.now().strftime('%Y%m%d%H%M'), help="Evaluation timestamp")
    run_local_parser.add_argument('-w', '--words23', action='store_true', help="Evaluation for words23 workshop")
    run_local_parser.add_argument('-c', '--client-id', default=0, type=str, help="(Remote) client id")

    run_clients = command_parser.add_parser('run-clients', help="Start application in remote clients")
    run_clients.add_argument('-d', '--duration', type=int, default=1, help="Duration in s")
    run_clients.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    run_clients.add_argument('-m', '--metadata', type=int, default=1, help="Number of datastores")
    run_clients.add_argument('-c', '--clients', type=int, default=len(CLIENTS_IP), help="Number of clients")
    run_clients.add_argument('-v', '--variation', type=str, help="Variation type")
    run_clients.add_argument('-w', '--words23', action='store_true', help="Evaluation for words23 workshop")

    restart_server_parser = command_parser.add_parser('restart-server', help="Restart server")
    restart_server_parser.add_argument('-d', '--docker', action='store_true', help="Run server on docker container")

    deploy_server_parser = command_parser.add_parser('deploy-server', help="Deploy remote server")
    deploy_server_parser.add_argument('-d', '--docker', action='store_true', help="Run server on docker container")

    deploy_clients_parser = command_parser.add_parser('deploy-clients', help="Deploy remote clients")

    args = vars(main_parser.parse_args())
    command = args.pop('command')
    
    if not command:
        print("Invalid arguments!")
        print("Usage: ./eval.py {run-local, run-clients, restart-server, deploy-server, deploy-clients} {...}")
        exit(-1)

    if command == 'run-clients' and args['clients'] > len(CLIENTS_IP):
        if args['clients'] > len(CLIENTS_IP):
            print(f"[ERROR] Invalid number of clients! Max value = {len(CLIENTS_IP)}")
            exit(-1)        

    function_name = command.replace('-', '_')
    if command in ['restart-server', 'deploy-clients', 'deploy-server']:
        evalClient = EvalClient()
        getattr(sys.modules[__name__], function_name)(**args)
    else:
        evalClient = EvalClient(**args)
        getattr(evalClient, function_name)()