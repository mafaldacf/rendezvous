from __future__ import print_function
import argparse
import time
import threading
import grpc
import datetime
from datetime import datetime
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
from threading import Lock

RESULTS_DIR = "results/eval_6" # needs subdirectories 'datastores', 'clients'
SSH_KEY_PATH = "/home/leafen/.ssh/rendezvous-eu-2.pem"
CLIENTS_IP = ["18.195.154.38"]
SERVER_IP = "54.93.48.225"
SERVER_ADDRESS = f"{SERVER_IP}:8001"
CURRENT_THREAD_GATHER_DELAY_S = 1


class EvalClient():
    def __init__(self):
        self.channel = grpc.insecure_channel(SERVER_ADDRESS)
        self.stub = rdv.ClientServiceStub(self.channel)
        self.do_send_requests = True
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
            shift = [(5, 8), (-7, 5), (-14, 3), (-15, 0), (-17, -1)]
            for i, (throughput, latency) in enumerate(datapoints):
                plt.annotate(f"{(i+1)*200}", (throughput, latency), xytext=shift[i], textcoords='offset points', ha='center', size=10)

    def annotate_datastores(self, plt, datapoints):
        # hard coded :(
        if len(datapoints) == 5:
            shift = [(1, 5), (0, 5), (0, 5), (0, 5), (0, 5)]
            num_datastores = [20, 15, 10, 5, 1]

        elif len(datapoints) == 7:
            shift = [(10, 5), (5, 5), (5, 5), (5, 5), (5, 5), (5, 5), (5, 5)]
            num_datastores = [100, 80, 70, 60, 50, 30, 1]

        else:
            return

        for i, (throughput, latency) in enumerate(datapoints):
            plt.annotate(f"{num_datastores[i]}", (throughput, latency), xytext=shift[i], textcoords='offset points', ha='center', size=10)

    def plot(self, directory):
        sns.set_theme(style='ticks')
        plt.rcParams["figure.figsize"] = [6, 2.25] 
        plt.rcParams["figure.dpi"] = 600
        plt.rcParams['axes.labelsize'] = 'small'

        dps_clients = self.get_datapoints(directory + '/clients')
        data_clients = [
            {
            'throughput': dp[0],
            'latency': dp[1],
            'variation': '# clients'
            } for dp in dps_clients
        ]

        dps_datastores = self.get_datapoints(directory + '/datastores')
        data_datastores = [
            {
            'throughput': dp[0],
            'latency': dp[1],
            'variation': '# datastores'
            } for dp in dps_datastores
        ]

        df_clients = pd.DataFrame.from_records(data_clients)
        df_datastores = pd.DataFrame.from_records(data_datastores)
        pp(df_clients)
        pp(df_datastores)

        # create figure for both plots
        fig, axes = plt.subplots(1, 2)

        # plot datapoints with datastore variation and force secondary color of searborn palette
        ax_datastores = sns.lineplot(data=df_datastores, x="throughput", y="latency", hue='variation', style='variation', markers=['o'], ax=axes[0])
        ax_datastores.set_xlabel(None)
        ax_datastores.set_ylabel(None)
        ax_datastores.legend_.set_title(None)
        # use same y limits for both subplots
        #ax_datastores.set_xlim(right=5100)
        ax_datastores.set_ylim(top=138, bottom=35)
        self.annotate_datastores(ax_datastores, dps_datastores)

        # plot datapoints with client variation
        ax_clients = sns.lineplot(data=df_clients, x="throughput", y="latency", hue='variation', style='variation', markers=['o'], ax=axes[1], palette=[sns.color_palette()[1]])
        ax_clients.set_xlabel(None)
        ax_clients.set_ylabel(None)
        ax_clients.legend_.set_title(None)
        # use same y limits for both subplots
        #ax_clients.set_xlim(right=10000)
        ax_clients.set_ylim(top=138, bottom=35) # use same y limits for both subplots
        self.annotate_clients(ax_clients, dps_clients)
        # remove y numeration since it is already present in the first subplot on the left side
        ax_clients.set_yticklabels([])

        # set common x and y labels
        fig.text(0.5, -0.10, 'Throughput (req/s)', ha='center')
        fig.text(0.02, 0.5, 'Latency (ms)', va='center', rotation='vertical')

        # reverse order of legend for both subplots
        handles, labels = ax_clients.get_legend_handles_labels()
        ax_clients.legend(handles[::-1], labels[::-1])
        handles, labels = ax_datastores.get_legend_handles_labels()
        ax_datastores.legend(handles[::-1], labels[::-1])

        plot_name = f'plots/throughput_latency_datastores_clients_{time.time()}.png'
        plt.savefig(plot_name, bbox_inches='tight', pad_inches=0.1)
        print(f"Successfuly saved plot figure in {plot_name}!")
    

    def gather_current_thread(self, task_id, requests, responses, latencies):
        total_latency = 0
        for i in range(responses):
            total_latency += latencies[i]
        avg_latency = total_latency/responses
        with self.mutex:
            self.results[task_id] = [requests, responses, avg_latency]
    
    def gather_all_threads(self, duration):
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

        with open(f"results.txt", 'w') as file:
            file.write(f"requests: {num_requests}\n")
            file.write(f"responses: {num_responses}\n")
            file.write(f"throughput: {throughput}\n")
            file.write(f"avg_latency: {avg_latency_round}\n")

    def send_requests(self, sleep, task_id, datastores):
        requests = 0
        responses = 0
        latencies = []
        
        datastores = []
        for i in range(datastores):
            datastores.append(f"a-very-interesting-datastore-{i}")
        
        request = pb.RegisterBranchesMessage2(rid=str(task_id), datastores=datastores, regions=["EU", "US"])
        base_request_id = 5000
        request_id = 5000

        while self.do_send_requests:
            if requests > request_id:
                request_id += base_request_id
            try:
                requests += 1
                start_ts = datetime.utcnow().timestamp()
                request.rid = f'{task_id}-{request_id}'
                self.stub.RegisterBranches2(request, timeout=30)
                end_ts = datetime.utcnow().timestamp()
                latency_ms = int((end_ts - start_ts) * 1000)
                latencies.append(latency_ms)
                responses += 1

            except grpc.RpcError as e:
                print("[ERROR]", e.details(), flush=True)
                
            time.sleep(sleep)
            
        time.sleep(CURRENT_THREAD_GATHER_DELAY_S)
    
        self.gather_current_thread(task_id, requests, responses, latencies)

    def local_run(self, duration, threads, sleep, datastores):
        thread_pool = []

        for i in range(threads):
            self.results[i] = None
            thread_pool.append(threading.Thread(target=self.send_requests, args=(sleep, i, datastores)))

        for t in thread_pool:
            t.start()

        time.sleep(duration)
        self.do_send_requests = False

        for t in thread_pool:
            t.join()

        self.gather_all_threads(duration)

    def remote_deploy(self):
        for client_hostname in CLIENTS_IP:
            client_hostname = paramiko.SSHClient()
            client_hostname.set_missing_host_key_policy(paramiko.AutoAddPolicy())

            try:
                private_key = paramiko.RSAKey.from_private_key_file(SSH_KEY_PATH)
                client_hostname.connect(client_hostname, username='ubuntu', pkey=private_key)
                print(f"[SCP] Connected to {client_hostname}", flush=True)
                # kill existing python programs from previous interrupted executions
                client_hostname.exec_command("pkill -9 python")
                # clean previous results and code
                client_hostname.exec_command("rm -rf results.txt && mkdir -p client-eval")
                with SCPClient(client_hostname.get_transport()) as scp:
                    scp.put('eval.py', '/home/ubuntu/client-eval')
                    scp.put('requirements.txt', '/home/ubuntu/client-eval')
                    scp.put('proto', '/home/ubuntu/client-eval', recursive=True)

                _, _, stderr = client_hostname.exec_command(f"cd client-eval && sudo pip install -r requirements.txt")
                stderr = stderr.read().decode()
                if len(stderr) > 0:
                    print(f"[ERROR] {stderr}", flush=True)

            except Exception as ex:
                print(f"An error occurred for {client_hostname}: {ex}", flush=True)

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

        except Exception as ex:
            print(f"An error occurred for {SERVER_IP}: {ex}", flush=True)
            exit(-1)
            
    def remote_run(self, duration, threads, sleep, clients, datastores):
        thread_results = [None] * clients
        thread_pool = []

        for client_id in range(clients):
            client_addr = CLIENTS_IP[client_id]
            thread_pool.append(threading.Thread(target=self.connect_remote, args=(thread_results, client_id, client_addr, duration, threads, sleep, datastores)))
        
        for t in thread_pool:
            t.start()

        for t in thread_pool:
            t.join()

        results_filename = f"datastores_{datastores}_clients_{clients}_duration_{duration}_threads_{threads}__{datetime.now().strftime('%Y%m%d%H%M')}.txt"
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

    def connect_remote(self, thread_results, task_id, client_address, duration, threads, sleep, datastores):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        results = {}
        try:
            private_key = paramiko.RSAKey.from_private_key_file(SSH_KEY_PATH)
            client.connect(client_address, username='ubuntu', pkey=private_key)
            print(f"[EXEC] Connected to {client_address}", flush=True)

            _, stdout, _ = client.exec_command(f"python3 client-eval/eval.py run -d {duration} -t {threads} -s {sleep} -d {datastores}", get_pty=True)

            # display progress bar
            waiting_time = duration + 5
            print(f"[INFO] Waiting for results file in remote for {waiting_time} seconds")
            progress_bar = tqdm(total=waiting_time, desc='Progress', bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt}')
            for _ in range(waiting_time):
                time.sleep(1)
                progress_bar.update(1)
            progress_bar.close()
            
            # read results from remote client
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
                        


# Usages
# > python3 eval.py run -d 1 -t 1 -m 1
# > python3 eval.py remote-deploy
# > python3 eval.py remote-run -d 30 -t 200 -m 100 -c 1
# > python3 eval.py plot -a

if __name__ == '__main__':
    main_parser = argparse.ArgumentParser()
    command_parser = main_parser.add_subparsers(help='commands', dest='command')

    local_run_parser = command_parser.add_parser('local-run', help="Run client application locally")
    local_run_parser.add_argument('-d', '--duration', type=int, default=2, help="Duration in s")
    local_run_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    local_run_parser.add_argument('-s', '--sleep', type=float, default=0.0, help="Sleep between requests")
    local_run_parser.add_argument('-m', '--datastores', type=int, default=1, help="Number of datastores")

    remote_deploy_parser = command_parser.add_parser('remote-deploy', help="Remote Deploy")

    remote_run_parser = command_parser.add_parser('remote-run', help="Start application in remote clients")
    remote_run_parser.add_argument('-d', '--duration', type=int, default=1, help="Duration in s")
    remote_run_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    remote_run_parser.add_argument('-s', '--sleep', type=float, default=0.0, help="Sleep between requests")
    remote_run_parser.add_argument('-c', '--clients', type=int, default=len(CLIENTS_IP), help="Number of clients")
    remote_run_parser.add_argument('-d', '--datastores', type=int, default=1, help="Number of datastores")

    plot_parser = command_parser.add_parser('plot', help="Plot")
    plot_parser.add_argument('-d', '--directory', type=str, default=RESULTS_DIR, help="Base directory of results")
    plot_parser.add_argument('-a', '--annotate', type=bool, default=False, help="Annotate datapoints")


    args = vars(main_parser.parse_args())
    print("Arguments:", args, flush=True)
    command = args.pop('command')

    evalClient = EvalClient()

    print("------------------------------------")
    print(f"EVAL START TIME: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("------------------------------------")

    if command == 'remote-run' and args['clients'] > len(CLIENTS_IP):
        print(f"Invalid number of clients! Max value = {len(CLIENTS_IP)}")
        exit(-1)

    function_name = command.replace('-', '_')
    function = getattr(evalClient, function_name, None)
    function(**args)