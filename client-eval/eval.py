from __future__ import print_function
import argparse
import time
import threading
import grpc
from functools import partial
import pause
import datetime
from datetime import datetime, timedelta
import pandas as pd
from pprint import pprint as pp
from matplotlib import pyplot as plt
import glob
import paramiko
import seaborn as sns
from scp import SCPClient

from proto import rendezvous_pb2 as pb
from proto import rendezvous_pb2_grpc as rdv

from threading import Thread, Lock

RESULTS_DIR = "results/second_eval"
RENDEZVOUS_ADDRESS = "3.123.128.138:8001"
CLIENT_SSH_KEY = "/home/leafen/.ssh/rendezvous-eu-2.pem"
CLIENTS_IP = ["3.73.124.189", "3.120.245.63", "3.122.53.196", "3.71.196.185", "3.75.249.223"]
STARTUP_DELAY_S = 2
GATHER_DELAY_S = 1


class EvalClient():
    def __init__(self, rendezvous_address=None):
        if rendezvous_address:
            self.channel = grpc.insecure_channel(rendezvous_address)
            self.stub = rdv.ClientServiceStub(self.channel)
        self.do_send = True
        self.results = {}
        self.mutex = Lock()

    def get_datapoints(self, prefix):
        latencies = []
        throughputs = []

        for file_path in glob.glob(f"{RESULTS_DIR}/{prefix}_*.txt"):
            with open(file_path, "r") as file:
                for line in file:
                    if "Throughput" in line:
                        throughput = line.split(":")[1].strip()
                        throughputs.append(int(throughput))
                    elif "Latency" in line:
                        latency = line.split(":")[1].strip()
                        latencies.append(int(latency))
        throughputs.sort()
        latencies.sort()
        return sorted(zip(throughputs, latencies))
    
    def annotate_plot(self, plt, datapoints):
        # hard coded :(
        if len(x) == 5:
            xy_pos = [(-25, 5), (-30, 5), (-30, 0), (-30, 0)]
            for i, (throughput, latency) in enumerate(datapoints):
                if i == 0:
                    plt.annotate(f"1 client", (throughput, latency), xytext=(5, 15), textcoords='offset points', ha='center')
                else:
                    plt.annotate(f"{i+1} clients", (throughput, latency), xytext=xy_pos[i-1], textcoords='offset points', ha='center')

    def plot(self):
        # Apply the default theme
        sns.set_theme(style='ticks')
        plt.rcParams["figure.figsize"] = [6,3.5] 
        plt.rcParams["figure.dpi"] = 600
        plt.rcParams['axes.labelsize'] = 'small'

        plt.xlabel("Throughput (req/s)")
        plt.ylabel("Latency (ms)")

        results = {
            '1 region': self.get_datapoints('metadata_1'),
            '10 regions': self.get_datapoints('metadata_10'),
            '100 regions': self.get_datapoints('metadata_100')
        }

        data = [
            {
            'throughput': dp[0],
            'latency': dp[1],
            'metadata': metadata,
            } for metadata, dps in results.items() for dp in dps
        ]
        

        df = pd.DataFrame.from_records(data)
        pp(df)
        ax = sns.lineplot(data=df, x="throughput", y="latency", hue='metadata', marker='o')
        # reverse order of legend
        handles, labels = ax.get_legend_handles_labels()
        ax.legend(handles[::-1], labels[::-1],)

        ax.set_xlabel('Throughput (req/s)')
        ax.set_ylabel('Latency (ms)')
        ax.legend_.set_title(None)

        #self.annotate_plot(plt, results['1 region'])

        plot_name = f'plots/{time.time()}.png'
        plt.savefig(plot_name, bbox_inches = 'tight', pad_inches = 0.1)
        print(f"Successfuly saved plot figure in {plot_name}!")
    
    def gather_current_thread(self, task_id, requests, responses, latencies):
        total_latency = 0
        for i in range(responses):
            total_latency += latencies[i]
        avg_latency = total_latency/responses
        with self.mutex:
            self.results[task_id] = [requests, responses, avg_latency]
    
    def gather(self, duration):
        num_requests = 0
        num_responses = 0
        avg_latency = 0

        for v in self.results.values():
            num_requests += v[0]
            num_responses += v[1]
            avg_latency += v[2]
        
        throughput = num_requests/duration
        avg_latency = int(avg_latency/len(self.results))

        #with open(f"./results/duration_{duration}.txt", "w") as file:
        #    file.write(f"Responses: {num_responses}/{num_requests}\n")
        #    file.write(f"Throughput (req/s): {throughput}\n")
        #    file.close()

        #print("\n-------------------------------------------------------", flush=True)
        print(f"requests: {num_requests}", flush=True)
        print(f"responses: {num_responses}", flush=True)
        print(f"throughput: {throughput}", flush=True)
        print(f"avg_latency: {avg_latency}", flush=True)



    def send(self, sleep, task_id, metadata_size):
        requests = 0
        responses = 0
        latencies = []
        
        regions = []
        for i in range(metadata_size):
            regions.append(f"a-very-interesting-region-{i}")

        request = pb.RegisterBranchesMessage(rid=str(task_id), regions=regions, service='eval_service')
        while self.do_send:
            try:
                requests += 1
                start_ts = datetime.utcnow().timestamp()
                self.stub.RegisterBranches(request)
                end_ts = datetime.utcnow().timestamp()
                latency_ms = int((end_ts - start_ts) * 1000)
                latencies.append(latency_ms)
                responses += 1

            except grpc.RpcError as e:
                print("[ERROR]", e.details(), flush=True)
                
            time.sleep(sleep)
            
        time.sleep(GATHER_DELAY_S)
    
        self.gather_current_thread(task_id, requests, responses, latencies)

    def run(self, rendezvous_address, duration, threads, sleep, metadata_size):
        thread_pool = []
        for i in range(threads):
            self.results[i] = None
            thread_pool.append(threading.Thread(target=self.send, args=(sleep, i, metadata_size)))

        for t in thread_pool:
            t.start()

        time.sleep(duration)
        self.do_send = False

        for t in thread_pool:
            t.join()

        self.gather(duration)

    def remote_deploy(self):
        for client in CLIENTS_IP:
            self.copy_remote(client)

    def remote_run(self, rendezvous_address, duration, threads, sleep, clients, metadata_size):
        print("-----------------------------------", flush=True)
        print(f"Running eval for {clients} clients", flush=True)

        thread_results = [None] * clients
        thread_pool = []

        for client_id in range(clients):
            client_addr = CLIENTS_IP[client_id]
            thread_pool.append(threading.Thread(target=self.exec_remote, args=(thread_results, client_id, client_addr, rendezvous_address, duration, threads, sleep, metadata_size)))
        
        for t in thread_pool:
            t.start()

        print(f"Waiting for clients in {STARTUP_DELAY_S} seconds", flush=True)

        for t in thread_pool:
            t.join()

        results_filename = f"metadata_{metadata_size}_clients_{clients}_duration_{duration}_threads_{threads}__{datetime.now().strftime('%Y%m%d%H%M')}.txt"
        with open(f"{RESULTS_DIR}/{results_filename}", 'w') as file:
        
            final_results = {'requests': 0, 'responses': 0, 'throughput (req/s)': 0.0, 'avg_latency (ms)': 0}
            for client_id, result in enumerate(thread_results):
                final_results['requests'] += result['requests']
                final_results['responses'] += result['responses']
                final_results['throughput (req/s)'] += result['throughput']
                final_results['avg_latency (ms)'] += result['avg_latency']
                file.write(f"Client {CLIENTS_IP[client_id]}: {result}\n")
            
            final_results['avg_latency (ms)'] = final_results['avg_latency (ms)']/len(thread_results)
            file.write("----------------------------\n")
            file.write(f"Duration: {duration}\n")
            file.write(f"Clients: {clients}\n")
            file.write(f"Threads per Client: {threads}\n")
            file.write(f"Requests: {final_results['requests']}\n")
            file.write(f"Responses: {final_results['responses']} ({(final_results['requests']/final_results['responses'])*100}%)\n")
            file.write(f"Throughput (req/s): {int(final_results['throughput (req/s)'])}\n")
            file.write(f"Latency (ms): {int(final_results['avg_latency (ms)'])}\n")

            print(f"Final Results: {final_results}", flush=True)
        

    def copy_remote(self, hostname):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        try:
            private_key = paramiko.RSAKey.from_private_key_file(CLIENT_SSH_KEY)
            client.connect(hostname, username='ubuntu', pkey=private_key)
            print(f"[SCP] Connected to {hostname}", flush=True)

            client.exec_command(f"mkdir -p client-eval")
            with SCPClient(client.get_transport()) as scp:
                scp.put('eval.py', '/home/ubuntu/client-eval')
                scp.put('requirements.txt', '/home/ubuntu/client-eval')
                scp.put('proto', '/home/ubuntu/client-eval', recursive=True)

            _, stdout, stderr = client.exec_command(f"cd client-eval && sudo pip install -r requirements.txt")
            stderr = stderr.read().decode()
            if len(stderr) > 0:
                print(f"[ERROR] {stderr}", flush=True)

        except paramiko.AuthenticationException:
            print(f"Authentication failed for {hostname}.", flush=True)
        except paramiko.SSHException as ssh_ex:
            print(f"Error occurred while connecting to {hostname}: {ssh_ex}", flush=True)
        except Exception as ex:
            print(f"An error occurred for {hostname}: {ex}", flush=True)

    def exec_remote(self, thread_results, task_id, client_address, rendezvous_address, duration, threads, sleep, metadata_size):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        results = {}
        try:
            private_key = paramiko.RSAKey.from_private_key_file(CLIENT_SSH_KEY)
            client.connect(client_address, username='ubuntu', pkey=private_key)
            print(f"[EXEC] Connected to {client_address}", flush=True)

            _, stdout, stderr = client.exec_command(f"python3 client-eval/eval.py run -d {duration} -t {threads} -s {sleep} -addr {rendezvous_address} -m {metadata_size}")
            
            stderr = stderr.read().decode()
            if len(stderr) > 0:
                print(f"[ERROR] {stderr}", flush=True)
            
            stdout = stdout.read().decode()
            lines = stdout.strip().split('\n')
            for line in lines:
                for key, value_type in {'requests': int, 'responses': int, 'throughput': float, 'avg_latency': int}.items():
                    if line.lower().startswith(key + ':'):
                        results[key] = value_type(line.split(':')[1].strip())

            client.close()
            thread_results[task_id] = results
            return results

        except paramiko.AuthenticationException:
            print(f"Authentication failed for {client_address}.", flush=True)
        except paramiko.SSHException as ssh_ex:
            print(f"Error occurred while connecting to {client_address}: {ssh_ex}", flush=True)
        except Exception as ex:
            print(f"An error occurred for {client_address}: {ex}", flush=True)

        return None
                        


# Usage: python3 eval.py run -d 5 -t 275
# OR   : python3 eval.py remote-run -d 30 -t 275 -m 1000 -c 1
# OR   : python3 eval.py remote-deploy
# OR   : python3 client.py plot
if __name__ == '__main__':
    main_parser = argparse.ArgumentParser()
    command_parser = main_parser.add_subparsers(help='commands', dest='command')

    run_parser = command_parser.add_parser('run', help="Run")
    run_parser.add_argument('-d', '--duration', type=int, default=2, help="Duration in s")
    run_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    run_parser.add_argument('-s', '--sleep', type=float, default=0.0, help="Sleep between requests")
    run_parser.add_argument('-addr', '--rendezvous_address', type=str, default=RENDEZVOUS_ADDRESS, help="Address of rendezvous metadata server")
    run_parser.add_argument('-m', '--metadata_size', type=int, default=1, help="Metadata size (number of regions)")

    remote_deploy_parser = command_parser.add_parser('remote-deploy', help="Remote Deploy")

    remote_run_parser = command_parser.add_parser('remote-run', help="Remote Run")
    remote_run_parser.add_argument('-d', '--duration', type=int, default=1, help="Duration in s")
    remote_run_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    remote_run_parser.add_argument('-s', '--sleep', type=float, default=0.0, help="Sleep between requests")
    remote_run_parser.add_argument('-c', '--clients', type=int, default=len(CLIENTS_IP), help="Number of clients")
    remote_run_parser.add_argument('-addr', '--rendezvous_address', type=str, default=RENDEZVOUS_ADDRESS, help="Address of rendezvous metadata server")
    remote_run_parser.add_argument('-m', '--metadata_size', type=int, default=1, help="Metadata size (number of regions)")

    plot_parser = command_parser.add_parser('plot', help="Plot")

    args = vars(main_parser.parse_args())
    print("Arguments:", args, flush=True)
    command = args.pop('command')

    if 'rendezvous_address' in args:
        evalClient = EvalClient(args['rendezvous_address'])
    else:
        evalClient = EvalClient()

    if command == 'run':
        current_time = datetime.now()
        target_datetime = current_time + timedelta(seconds=STARTUP_DELAY_S)
        print(f"Sleeping for {STARTUP_DELAY_S} seconds, until {target_datetime}...", flush=True)

        pause.until(target_datetime)
        print(f"Starting evaluation", flush=True)
        evalClient.run(**args)

    elif command in ['remote-run', 'remote-deploy', 'plot']:

        if command == 'remote-run' and args['clients'] > len(CLIENTS_IP):
            print(f"Invalid number of clients! Max value = {len(CLIENTS_IP)}")
            exit(-1)

        function_name = command.replace('-', '_')
        function = getattr(evalClient, function_name, None)
        function(**args)

    else:
        print("Invalid arguments!")
        print("Usage: eval.py {run, remote-run, remote-deploy, plot} -d <duration_s> -t <#threads> -s <time_s> -addr <rdv_addr>")
        exit(-1)