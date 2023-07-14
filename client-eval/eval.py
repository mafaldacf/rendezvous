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

RESULTS_DIR = "results/fourth_eval"
SSH_KEY_PATH = "/home/leafen/.ssh/rendezvous-eu-2.pem"
CLIENTS_IP = ["3.70.70.228", "3.66.235.64", "3.64.216.49", "3.77.53.5", "18.156.69.235"]
SERVER_IP = "localhost"
SERVER_ADDRESS = f"{SERVER_IP}:8001"
STARTUP_DELAY_S = 2
GATHER_DELAY_S = 1


class EvalClient():
    def __init__(self, server_address=None):
        if server_address:
            self.channel = grpc.insecure_channel(server_address)
            self.stub = rdv.ClientServiceStub(self.channel)
        self.do_send = True
        self.results = {}
        self.mutex = Lock()

    def get_datapoints(self, directory, prefix=''):
        latencies = []
        throughputs = []

        for file_path in glob.glob(f"{directory}/{prefix}*.txt"):
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
    
    def annotate_clients(self, plt, datapoints):
        # hard coded :(
        if len(datapoints) == 5:
            #xy_pos = [(-25, 5), (-30, 5), (-30, 0), (-30, 0)]
            xy_pos = [(10, 15), (-35, 5), (-30, 5), (-30, 0), (-30, 0)]
            for i, (throughput, latency) in enumerate(datapoints):
                #if i == 0:
                #    plt.annotate(f"1 client", (throughput, latency), xytext=(5, 15), textcoords='offset points', ha='center')
                #else:
                #    plt.annotate(f"{i+1} clients", (throughput, latency), xytext=xy_pos[i-1], textcoords='offset points', ha='center')
                plt.annotate(f"{(i+1)*200} clients", (throughput, latency), xytext=xy_pos[i], textcoords='offset points', ha='center', size=8)

    # Plot throughput-latency
    # Each datapoints represents a different number of datastores (1, 5, 10, 25, 50, 75, 100)
    def plot_line_datastores(self, directory):
        datapoints = self.get_datapoints(directory + '/datastores')
        data = [
            {
            'throughput': dp[0],
            'latency': dp[1]
            } for dp in datapoints
        ]
        plt = self.plot_line(data)

        plot_name = f'plots/line_datastores_{time.time()}.png'
        plt.savefig(plot_name, bbox_inches = 'tight', pad_inches = 0.1)
        print(f"Successfuly saved plot figure in {plot_name}!")

    # Plot throughput-latency
    # Each datapoints represents a different number of clients (1 to 5)
    def plot_line_clients(self, directory):
        datapoints = self.get_datapoints(directory + '/clients')
        data = [
            {
            'throughput': dp[0],
            'latency': dp[1]
            } for dp in datapoints
        ]

        plt = self.plot_line(data)
        self.annotate_clients(plt, datapoints)

        plot_name = f'plots/line_clients_{time.time()}.png'
        plt.savefig(plot_name, bbox_inches = 'tight', pad_inches = 0.1)
        print(f"Successfuly saved plot figure in {plot_name}!")

    def plot_line(self, data):
        sns.set_theme(style='ticks')
        plt.rcParams["figure.figsize"] = [6,3.5] 
        plt.rcParams["figure.dpi"] = 600
        plt.rcParams['axes.labelsize'] = 'small'

        df = pd.DataFrame.from_records(data)
        pp(df)
        ax = sns.lineplot(data=df, x="throughput", y="latency", marker='o')
        ax.set_xlabel('Throughput (req/s)')
        ax.set_ylabel('Latency (ms)')
        return plt


    
    # Multiline plot throughput-latency
    # 3 lines for different number of datastores (1, 10 and 100)
    def plot_multiline(self, directory):
        sns.set_theme(style='ticks')
        plt.rcParams["figure.figsize"] = [6,3.5] 
        plt.rcParams["figure.dpi"] = 600
        plt.rcParams['axes.labelsize'] = 'small'

        results = {
            '1 datastore': self.get_datapoints(directory, 'datastores_1_'),
            '10 datastores': self.get_datapoints(directory, 'datastores_10_'),
            '100 datastores': self.get_datapoints(directory, 'datastores_100_')
        }

        data = [
            {
            'throughput': dp[0],
            'latency': dp[1],
            'datastores': metadata,
            } for metadata, dps in results.items() for dp in dps
        ]
        

        df = pd.DataFrame.from_records(data)
        pp(df)
        ax = sns.lineplot(data=df, x="throughput", y="latency", hue='datastores', marker='o')
        # reverse order of legend
        handles, labels = ax.get_legend_handles_labels()
        ax.legend(handles[::-1], labels[::-1],)

        ax.set_xlabel('Throughput (req/s)')
        ax.set_ylabel('Latency (ms)')
        ax.legend_.set_title(None)

        plot_name = f'plots/multiline_{time.time()}.png'
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
        
        """ regions = []
        for i in range(metadata_size):
            regions.append(f"a-very-interesting-region-{i}") """
        
        datastores = []
        for i in range(metadata_size):
            datastores.append(f"a-very-interesting-datastore-{i}")
        
        #request = pb.RegisterBranchesMessage(rid=str(task_id), regions=regions, service='eval_service')
        request = pb.RegisterBranchesMessage2(rid=str(task_id), datastores=datastores, regions=["EU", "US"])
        
        while self.do_send:
            try:
                requests += 1
                start_ts = datetime.utcnow().timestamp()
                #self.stub.RegisterBranches(request)
                response = self.stub.RegisterBranches2(request)
                print(response)
                exit(0)
                end_ts = datetime.utcnow().timestamp()
                latency_ms = int((end_ts - start_ts) * 1000)
                latencies.append(latency_ms)
                responses += 1

            except grpc.RpcError as e:
                print("[ERROR]", e.details(), flush=True)
                
            time.sleep(sleep)
            
        time.sleep(GATHER_DELAY_S)
    
        self.gather_current_thread(task_id, requests, responses, latencies)

    def run(self, server_address, duration, threads, sleep, metadata_size):
        thread_pool = []

        if threads > 1:
            for i in range(threads):
                self.results[i] = None
                thread_pool.append(threading.Thread(target=self.send, args=(sleep, i, metadata_size)))

            for t in thread_pool:
                t.start()

            time.sleep(duration)
            self.do_send = False

            for t in thread_pool:
                t.join()
        else:
            self.send(sleep, 0, metadata_size)

        self.gather(duration)

    def remote_deploy(self):
        for client in CLIENTS_IP:
            self.copy_remote(client)

    def restart_server(self):
        print(f"Restarting metadata server on {SERVER_ADDRESS}...")
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        try:
            private_key = paramiko.RSAKey.from_private_key_file(SSH_KEY_PATH)
            client.connect(SERVER_IP, username='ubuntu', pkey=private_key)

            _, _, stderr = client.exec_command(f"fuser -k 8001/tcp")
            stderr = stderr.read().decode()
            if len(stderr) > 0 and '8001/tcp' not in stderr:
                print(f"[ERROR] {stderr}", flush=True)
                exit(-1)

            _, _, stderr = client.exec_command(f"cd rendezvous/rendezvous-server && ./rendezvous.sh run server eu")
            stderr = stderr.read().decode()
            if len(stderr) > 0:
                print(f"[ERROR] {stderr}", flush=True)
                exit(-1)

        except paramiko.AuthenticationException:
            print(f"Authentication failed for {SERVER_IP}.", flush=True)
            exit(-1)
        except paramiko.SSHException as ssh_ex:
            print(f"Error occurred while connecting to {SERVER_IP}: {ssh_ex}", flush=True)
            exit(-1)
        except Exception as ex:
            print(f"An error occurred for {SERVER_IP}: {ex}", flush=True)
            exit(-1)

    def start_clients(self, server_address, duration, threads, sleep, clients, metadata_size):
        thread_results = [None] * clients
        thread_pool = []

        for client_id in range(clients):
            client_addr = CLIENTS_IP[client_id]
            thread_pool.append(threading.Thread(target=self.exec_remote, args=(thread_results, client_id, client_addr, server_address, duration, threads, sleep, metadata_size)))
        
        for t in thread_pool:
            t.start()

        print(f"Waiting for clients in {STARTUP_DELAY_S} seconds", flush=True)

        for t in thread_pool:
            t.join()

        results_filename = f"datastores_{metadata_size}_clients_{clients}_duration_{duration}_threads_{threads}__{datetime.now().strftime('%Y%m%d%H%M')}.txt"
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

    def remote_run(self, server_address, duration, threads, sleep, clients, max_clients, metadata_size):
        if max_clients:
            t = threading.Thread(target=self.restart_server)
            t.start()
            print("Waiting for server to start...")
            time.sleep(5)
            for clients in range(1, max_clients+1):
                self.start_clients(server_address, duration, threads, sleep, clients, metadata_size)

        else:
            self.start_clients(server_address, duration, threads, sleep, clients, metadata_size)

    def copy_remote(self, hostname):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        try:
            private_key = paramiko.RSAKey.from_private_key_file(SSH_KEY_PATH)
            client.connect(hostname, username='ubuntu', pkey=private_key)
            print(f"[SCP] Connected to {hostname}", flush=True)

            client.exec_command(f"mkdir -p client-eval")
            with SCPClient(client.get_transport()) as scp:
                scp.put('eval.py', '/home/ubuntu/client-eval')
                scp.put('requirements.txt', '/home/ubuntu/client-eval')
                scp.put('proto', '/home/ubuntu/client-eval', recursive=True)

            _, _, stderr = client.exec_command(f"cd client-eval && sudo pip install -r requirements.txt")
            stderr = stderr.read().decode()
            if len(stderr) > 0:
                print(f"[ERROR] {stderr}", flush=True)

        except paramiko.AuthenticationException:
            print(f"Authentication failed for {hostname}.", flush=True)
        except paramiko.SSHException as ssh_ex:
            print(f"Error occurred while connecting to {hostname}: {ssh_ex}", flush=True)
        except Exception as ex:
            print(f"An error occurred for {hostname}: {ex}", flush=True)

    def exec_remote(self, thread_results, task_id, client_address, server_address, duration, threads, sleep, metadata_size):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        results = {}
        try:
            private_key = paramiko.RSAKey.from_private_key_file(SSH_KEY_PATH)
            client.connect(client_address, username='ubuntu', pkey=private_key)
            print(f"[EXEC] Connected to {client_address}", flush=True)

            _, stdout, stderr = client.exec_command(f"python3 client-eval/eval.py run -d {duration} -t {threads} -s {sleep} -addr {server_address} -m {metadata_size}")
            
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
            exit(-1)
        except paramiko.SSHException as ssh_ex:
            print(f"Error occurred while connecting to {client_address}: {ssh_ex}", flush=True)
            exit(-1)
        except Exception as ex:
            print(f"An error occurred for {client_address}: {ex}", flush=True)
            exit(-1)

        return None
                        


# Usage: python3 eval.py run -d 5 -t 275
# OR   : python3 eval.py run -d 1 -t 1 -m 1
# OR   : python3 eval.py remote-run -d 30 -t 200 -m 10 -c 1
# OR   : python3 eval.py remote-run -d 30 -t 200 -m 10 -mc 5
# OR   : python3 eval.py remote-deploy
# OR   : python3 eval.py plot -t multiline
# OR   : python3 eval.py plot -t line-clients
# OR   : python3 eval.py plot -t line-datastores
if __name__ == '__main__':
    main_parser = argparse.ArgumentParser()
    command_parser = main_parser.add_subparsers(help='commands', dest='command')

    run_parser = command_parser.add_parser('run', help="Run")
    run_parser.add_argument('-d', '--duration', type=int, default=2, help="Duration in s")
    run_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    run_parser.add_argument('-s', '--sleep', type=float, default=0.0, help="Sleep between requests")
    run_parser.add_argument('-addr', '--server_address', type=str, default=SERVER_ADDRESS, help="Address of rendezvous metadata server")
    run_parser.add_argument('-m', '--metadata_size', type=int, default=1, help="Metadata size (number of regions)")

    remote_deploy_parser = command_parser.add_parser('remote-deploy', help="Remote Deploy")

    remote_run_parser = command_parser.add_parser('remote-run', help="Remote Run")
    remote_run_parser.add_argument('-d', '--duration', type=int, default=1, help="Duration in s")
    remote_run_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    remote_run_parser.add_argument('-s', '--sleep', type=float, default=0.0, help="Sleep between requests")
    remote_run_parser.add_argument('-c', '--clients', type=int, default=len(CLIENTS_IP), help="Number of clients")
    remote_run_parser.add_argument('-mc', '--max_clients', type=int, default=None, help="Maximum number of clients")
    remote_run_parser.add_argument('-addr', '--server_address', type=str, default=SERVER_ADDRESS, help="Address of rendezvous metadata server")
    remote_run_parser.add_argument('-m', '--metadata_size', type=int, default=1, help="Metadata size (number of regions)")

    plot_parser = command_parser.add_parser('plot', help="Plot")
    plot_parser.add_argument('-t', '--type', type=str, choices=['multiline', 'line-clients', 'line-datastores'], help="Type of plot")
    plot_parser.add_argument('-d', '--directory', type=str, default=RESULTS_DIR, help="Base directory of results")


    args = vars(main_parser.parse_args())
    print("Arguments:", args, flush=True)
    command = args.pop('command')

    if 'server_address' in args:
        evalClient = EvalClient(args['server_address'])
    else:
        evalClient = EvalClient()

    if command == 'run':
        current_time = datetime.now()
        target_datetime = current_time + timedelta(seconds=STARTUP_DELAY_S)
        print(f"Sleeping for {STARTUP_DELAY_S} seconds, until {target_datetime}...", flush=True)

        pause.until(target_datetime)
        print(f"Starting evaluation", flush=True)
        evalClient.run(**args)

    elif command in ['remote-run', 'remote-deploy']:

        if command == 'remote-run' and args['clients'] > len(CLIENTS_IP):
            print(f"Invalid number of clients! Max value = {len(CLIENTS_IP)}")
            exit(-1)

        function_name = command.replace('-', '_')
        function = getattr(evalClient, function_name, None)
        function(**args)
    
    elif command == 'plot' and args['type']:
        plot_type = args.pop('type')
        function_name = command.replace('-', '_') + '_' + plot_type.replace('-', '_')
        function = getattr(evalClient, function_name, None)
        function(**args)

    else:
        print("Invalid arguments!")
        print("Usage: eval.py {run, remote-run, remote-deploy, plot} -d <duration_s> -t <#threads> -s <time_s> -addr <rdv_addr>")
        exit(-1)