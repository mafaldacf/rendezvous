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
from tqdm import tqdm
import time

from proto import rendezvous_pb2 as pb
from proto import rendezvous_pb2_grpc as rdv

from threading import Thread, Lock

RESULTS_DIR = "results/eval_5"
SSH_KEY_PATH = "/home/leafen/.ssh/rendezvous-eu-2.pem"
CLIENTS_IP = ["18.192.63.137", "18.196.239.101", "3.122.56.149", "18.195.203.9", "3.70.234.216"]
SERVER_IP = "18.184.132.153"
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
        return sorted(zip(throughputs, latencies))
    
    def annotate_clients(self, plt, datapoints):
        # hard coded :(
        if len(datapoints) == 5:
            #shift = [(10, 15), (-35, 5), (-30, 5), (-30, 0), (-30, 0)]
            shift = [(5, 8), (-7, 5), (-14, 3), (-15, 0), (-17, -1)]
            for i, (throughput, latency) in enumerate(datapoints):
                #plt.annotate(f"{(i+1)*200} clients", (throughput, latency), xytext=shift[i], textcoords='offset points', ha='center', size=8)
                plt.annotate(f"{(i+1)*200}", (throughput, latency), xytext=shift[i], textcoords='offset points', ha='center', size=10)

    def annotate_datastores(self, plt, datapoints):
        # hard coded :(
        if len(datapoints) == 7:
            shift = [(10, 5), (5, 5), (5, 5), (5, 5), (5, 5), (5, 5), (5, 5)]
            num_datastores = [100, 80, 70, 60, 50, 30, 1]
            for i, (throughput, latency) in enumerate(datapoints):
                plt.annotate(f"{num_datastores[i]}", (throughput, latency), xytext=shift[i], textcoords='offset points', ha='center', size=10)


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
        self.annotate_datastores(plt, datapoints)

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
            'latency': dp[1],
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
    
    def plot_double_line(self, directory):
        sns.set_theme(style='ticks')
        plt.rcParams["figure.figsize"] = [6, 3] 
        plt.rcParams["figure.dpi"] = 600
        plt.rcParams['axes.labelsize'] = 'small'

        datapoints = self.get_datapoints(directory + '/clients')
        data_by_clients = [
            {
            'throughput': dp[0],
            'latency': dp[1],
            'variation': '# clients'
            } for dp in datapoints
        ]

        datapoints = self.get_datapoints(directory + '/datastores')
        data_by_datastores = [
            {
            'throughput': dp[0],
            'latency': dp[1],
            'variation': '# datastores'
            } for dp in datapoints
        ]

        data = data_by_clients + data_by_datastores
        df = pd.DataFrame.from_records(data)
        pp(df)
        ax = sns.lineplot(data=df, x="throughput", y="latency", dashes=False, hue='variation', style='variation', markers=['o', 'o'])

        # reverse order of legend
        handles, labels = ax.get_legend_handles_labels()
        ax.legend(handles[::-1], labels[::-1],)

        ax.set_xlabel('Throughput (req/s)')
        ax.set_ylabel('Latency (ms)')
        ax.legend_.set_title(None)

        

        plot_name = f'plots/double_line_{time.time()}.png'
        plt.savefig(plot_name, bbox_inches = 'tight', pad_inches = 0.1)
        print(f"Successfuly saved plot figure in {plot_name}!")

    def plot_double_sub_plots(self, directory):
        sns.set_theme(style='ticks')
        plt.rcParams["figure.figsize"] = [6, 2.5] 
        plt.rcParams["figure.dpi"] = 600
        plt.rcParams['axes.labelsize'] = 'small'

        datapoints_by_clients = self.get_datapoints(directory + '/clients')
        data_by_clients = [
            {
            'throughput': dp[0],
            'latency': dp[1],
            'variation': '# clients'
            } for dp in datapoints_by_clients
        ]

        datapoints_by_datastores = self.get_datapoints(directory + '/datastores')
        data_by_datastores = [
            {
            'throughput': dp[0],
            'latency': dp[1],
            'variation': '# datastores'
            } for dp in datapoints_by_datastores
        ]

        cl_df = pd.DataFrame.from_records(data_by_clients)
        pp(cl_df)
        ds_df = pd.DataFrame.from_records(data_by_datastores)
        pp(ds_df)

        # create figure for both plots
        fig, axes = plt.subplots(1, 2)

        # plot datapoints with datastore variation and force secondary color of searborn palette
        ds_ax = sns.lineplot(data=ds_df, x="throughput", y="latency", hue='variation', style='variation', markers=['o'], ax=axes[0])
        ds_ax.set_xlabel(None)
        ds_ax.set_ylabel(None)
        ds_ax.legend_.set_title(None)
        ds_ax.set_xlim(right=5100)
        # use same y limits for both subplots
        ds_ax.set_ylim(top=138, bottom=35)
        self.annotate_datastores(ds_ax, datapoints_by_datastores)

        # plot datapoints with client variation
        cl_ax = sns.lineplot(data=cl_df, x="throughput", y="latency", hue='variation', style='variation', markers=['o'], ax=axes[1], palette=[sns.color_palette()[1]])
        cl_ax.set_xlabel(None)
        cl_ax.set_ylabel(None)
        cl_ax.legend_.set_title(None)
        # use same y limits for both subplots
        cl_ax.set_ylim(top=138, bottom=35) # use same y limits for both subplots
        self.annotate_clients(cl_ax, datapoints_by_clients)
        # remove y numeration since it is already present in the first subplot on the left side
        cl_ax.set_yticklabels([])

        # set common x and y labels
        fig.text(0.5, -0.10, 'Throughput (req/s)', ha='center')
        fig.text(0.02, 0.5, 'Latency (ms)', va='center', rotation='vertical')

        # Reverse order of legend for both subplots
        handles, labels = cl_ax.get_legend_handles_labels()
        cl_ax.legend(handles[::-1], labels[::-1])
        handles, labels = ds_ax.get_legend_handles_labels()
        ds_ax.legend(handles[::-1], labels[::-1])

        plot_name = f'plots/double_sub_plots_{time.time()}.png'
        plt.savefig(plot_name, bbox_inches='tight', pad_inches=0.1)
        print(f"Successfuly saved plot figure in {plot_name}!")
    
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

    def plot_four(self, directory):
        sns.set_theme(style='ticks')
        plt.rcParams["figure.figsize"] = [6,3.5] 
        plt.rcParams["figure.dpi"] = 600
        plt.rcParams['axes.labelsize'] = 'small'

        data = []

        dps = self.get_datapoints(directory + '/both', 'datastores_10_clients_1')
        data += [{'throughput': dp[0], 'latency': dp[1], 'params': '200 clients - 10 datastores'} for dp in dps]

        dps = self.get_datapoints(directory + '/both', 'datastores_100_clients_1')
        data += [{'throughput': dp[0], 'latency': dp[1], 'params': '200 clients - 100 datastores'} for dp in dps]

        dps = self.get_datapoints(directory + '/both', 'datastores_10_clients_5')
        data += [{'throughput': dp[0], 'latency': dp[1], 'params': '1000 clients - 10 datastores'} for dp in dps]

        dps = self.get_datapoints(directory + '/both', 'datastores_100_clients_5')
        data += [{'throughput': dp[0], 'latency': dp[1], 'params': '1000 clients - 100 datastores'} for dp in dps]

        df = pd.DataFrame.from_records(data)
        pp(df)
        ax = sns.scatterplot(data=df, x="throughput", y="latency", hue='params', marker='o')

        ax.set_xlabel('Throughput (req/s)')
        ax.set_ylabel('Latency (ms)')
        ax.legend_.set_title(None)

        plot_name = f'plots/four_{time.time()}.png'
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
        avg_latency = avg_latency/len(self.results)
        avg_latency_round = int(avg_latency)

        print(f"requests: {num_requests}", flush=True)
        print(f"responses: {num_responses}", flush=True)
        print(f"throughput: {throughput}", flush=True)
        print(f"avg_latency: {avg_latency_round}", flush=True)
        
        with open(f"results.txt", 'w') as file:
            file.write(f"requests: {num_requests}\n")
            file.write(f"responses: {num_responses}\n")
            file.write(f"throughput: {throughput}\n")
            file.write(f"avg_latency: {avg_latency_round}\n")



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
        base_request_id = 5000
        request_id = 5000

        while self.do_send:
            if requests > request_id:
                request_id += base_request_id
            try:
                requests += 1
                start_ts = datetime.utcnow().timestamp()
                #self.stub.RegisterBranches(request)
                request.rid = f'{task_id}-{request_id}'
                self.stub.RegisterBranches2(request, timeout=30)
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

    def start_client_threads(self, server_address, duration, threads, sleep, clients, metadata_size):
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
            file.write(f"Responses: {final_results['responses']} ({(final_results['responses']/final_results['requests'])*100}%)\n")
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
                self.start_client_threads(server_address, duration, threads, sleep, clients, metadata_size)

        else:
            self.start_client_threads(server_address, duration, threads, sleep, clients, metadata_size)

    def copy_remote(self, hostname):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        try:
            private_key = paramiko.RSAKey.from_private_key_file(SSH_KEY_PATH)
            client.connect(hostname, username='ubuntu', pkey=private_key)
            print(f"[SCP] Connected to {hostname}", flush=True)
            # kill existing python programs from previous interrupted executions
            client.exec_command("pkill -9 python")
            # clean previous results and code
            client.exec_command("rm -rf results.txt && mkdir -p client-eval")
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

            _, stdout, stderr = client.exec_command(f"python3 client-eval/eval.py run -d {duration} -t {threads} -s {sleep} -addr {server_address} -m {metadata_size}", get_pty=True)
            
            #stderr = stderr.read().decode()
            #if len(stderr) > 0:
            #    print(f"[ERROR] {stderr}", flush=True)

            waiting_time = duration + 5
            print(f"[INFO] Waiting for results file in remote for {waiting_time} seconds")
            progress_bar = tqdm(total=waiting_time, desc='Progress', bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt}')
            for _ in range(waiting_time):
                time.sleep(1)  # Sleep for 1 second
                progress_bar.update(1)  # Increment the progress bar by 1
            progress_bar.close()
                
            _, stdout, _ = client.exec_command("cat results.txt")
            stdout = stdout.read().decode()
            lines = stdout.strip().split('\n')
            print(lines)
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

    def restart_server(self):
        print(f"Restarting metadata server on {SERVER_IP}...")
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
            
            # start server but do not wait for command to be executed
            _, stdout, stderr = client.exec_command(f"cd rendezvous/rendezvous-server && ./rendezvous.sh run server eu", get_pty=True)


        except Exception as ex:
            print(f"An error occurred for {SERVER_IP}: {ex}", flush=True)
            exit(-1)
                        


# Usage: python3 eval.py run -d 5 -t 275
# OR   : python3 eval.py run -d 1 -t 1 -m 1
# OR   : python3 eval.py remote-deploy
# OR   : python3 eval.py remote-run -d 30 -t 200 -m 100 -c 1
# OR   : python3 eval.py remote-run -d 30 -t 200 -m 10 -mc 5
# OR   : python3 eval.py plot -t multiline
# OR   : python3 eval.py plot -t line-clients
# OR   : python3 eval.py plot -t line-datastores
# OR   : python3 eval.py plot -t double-line
# OR   : python3 eval.py plot -t double-sub-plots
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
    plot_parser.add_argument('-t', '--type', type=str, choices=['multiline', 'line-clients', 'line-datastores', 'double-line', 'double-sub-plots', 'four'], help="Type of plot")
    plot_parser.add_argument('-d', '--directory', type=str, default=RESULTS_DIR, help="Base directory of results")


    args = vars(main_parser.parse_args())
    print("Arguments:", args, flush=True)
    command = args.pop('command')

    evalClient = EvalClient(args.get('server_address'))

    print("------------------------------------")
    print(f"EVAL START TIME: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("------------------------------------")

    if command == 'run':
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