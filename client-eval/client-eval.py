from __future__ import print_function
import argparse
import time
import threading
import grpc
from functools import partial
import copy
import pause
import datetime
import pandas as pd
from matplotlib import pyplot as plt
import glob
import sys
import paramiko
from scp import SCPClient

from proto import rendezvous_pb2 as pb
from proto import rendezvous_pb2_grpc as rdv

from threading import Thread, Lock

RENDEZVOUS_ADDRESS = "3.72.7.183:8001"
SLAVE_SSH_KEY = "/home/leafen/.ssh/rendezvous-us-2.pem"
SLAVE_IP = ["3.83.247.42"]

class EvalClient():
    def __init__(self, rendezvous_address):
        self.channel = grpc.insecure_channel(rendezvous_address)
        self.stub = rdv.ClientServiceStub(self.channel)
        self.do_send = True
        self.results = {}
        self.mutex = Lock()

    def gather_thread(self, requests, responses, task_id):
        with self.mutex:
            self.results[task_id] = (requests, responses)
    
    def gather(self, duration):
        num_requests = 0
        num_responses = 0

        for v in self.results.values():
            num_requests += v[0]
            num_responses += v[1]
        
        throughput = num_requests / duration

        #with open(f"./results/duration_{duration}.txt", "w") as file:
        #    file.write(f"Responses: {num_responses}/{num_requests}\n")
        #    file.write(f"Throughput (req/s): {throughput}\n")
        #    file.close()

        #print("\n-------------------------------------------------------", flush=True)
        print(f"requests: {num_requests}", flush=True)
        print(f"responses: {num_responses}", flush=True)
        print(f"throughput: {throughput}", flush=True)



    def send(self, sleep, task_id):
        current = 0
        mutex = Lock()
        requests = 0
        #global responses
        responses = 0

        def process_async_response(response_future, i):
            with mutex:
                try:
                    print("HERE")
                    response_future.result()
                    responses = responses + 1
                except grpc.RpcError as e:
                    print("[ERROR]", e.details(), flush=True)
        
        request = pb.RegisterBranchesMessage(rid=str(task_id), regions=["eu", "us"], service='eval_service')
        while self.do_send:
            #copy_current = copy.deepcopy(current)
            try:
                requests += 1
                response = self.stub.RegisterBranches(request)
                responses += 1

                requests += 1
                self.stub.CloseBranch(pb.CloseBranchMessage(bid=response.bid, region='eu'))
                responses += 1

            except grpc.RpcError as e:
                print("[ERROR]", e.details(), flush=True)
            #response_future = self.stub.RegisterBranches.future(request)
            #callback_with_args = partial(process_async_response, i=copy_current)
            #response_future.add_done_callback(callback_with_args)
            current += 1
            time.sleep(sleep)
            
        
        time.sleep(1)
        #print(f"[THREAD {task_id}] gathering", flush=True)
    
        self.gather_thread(requests, responses, task_id)

    def run(self, rendezvous_address, duration, threads, sleep):
        thread_pool = []
        for i in range(threads):
            self.results[i] = None
            thread_pool.append(threading.Thread(target=self.send, args=(sleep, i)))

        for t in thread_pool:
            t.start()

        time.sleep(duration)
        self.do_send = False

        for t in thread_pool:
            t.join()

        self.gather(duration)

    def remote(self, rendezvous_address, duration, threads, sleep):
        for slave in SLAVE_IP:
            self.copy_remote(slave)

        results = []
        for slave in SLAVE_IP:
            result = self.exec_remote(slave, rendezvous_address, duration, threads, sleep)
            results.append(result)
        
        final_results = {'requests': 0, 'responses': 0, 'throughput': 0.0}
        for result in results:
            final_results['requests'] += result['requests']
            final_results['responses'] += result['responses']
            final_results['throughput'] += result['throughput']
        
        print(final_results)
        

    def copy_remote(self, hostname):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        try:
            private_key = paramiko.RSAKey.from_private_key_file(SLAVE_SSH_KEY)
            client.connect(hostname, username='ubuntu', pkey=private_key)
            print(f"[SCP] Connected to {hostname}")

            client.exec_command(f"mkdir -p client-eval")
            with SCPClient(client.get_transport()) as scp:
                scp.put('client-eval.py', '/home/ubuntu/client-eval')
                scp.put('requirements.txt', '/home/ubuntu/client-eval')
                scp.put('proto', '/home/ubuntu/client-eval', recursive=True)

            client.exec_command(f"cd client-eval && sudo pip install -r requirements.txt")

        except paramiko.AuthenticationException:
            print(f"Authentication failed for {hostname}.")
        except paramiko.SSHException as ssh_ex:
            print(f"Error occurred while connecting to {hostname}: {ssh_ex}")
        except Exception as ex:
            print(f"An error occurred for {hostname}: {ex}")

    def exec_remote(self, slave_address, rendezvous_address, duration, threads, sleep):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        results = {}
        try:
            private_key = paramiko.RSAKey.from_private_key_file(SLAVE_SSH_KEY)
            client.connect(slave_address, username='ubuntu', pkey=private_key)
            print(f"[EXEC] Connected to {slave_address}")

            _, stdout, stderr = client.exec_command(f"python3 client-eval/client-eval.py run -d {duration} -t {threads} -s {sleep} -addr {rendezvous_address}")
            
            stderr = stderr.read().decode()
            if len(stderr) > 0:
                print(f"[ERROR] {stderr}")
            
            output = stdout.read().decode()
            lines = output.strip().split('\n')
            for line in lines:
                for key, value_type in {'requests': int, 'responses': int, 'throughput': float}.items():
                    if line.lower().startswith(key + ':'):
                        results[key] = value_type(line.split(':')[1].strip())

            client.close()
            return results

        except paramiko.AuthenticationException:
            print(f"Authentication failed for {slave_address}.")
        except paramiko.SSHException as ssh_ex:
            print(f"Error occurred while connecting to {slave_address}: {ssh_ex}")
        except Exception as ex:
            print(f"An error occurred for {slave_address}: {ex}")

        return None
                        


# Usage: python3 client.py run -d 5 -t 15 -r 2
# OR   : python3 client.py run -d 1 -t 1 -r 1
# OR   : python3 client.py plot
if __name__ == '__main__':
    main_parser = argparse.ArgumentParser()
    command_parser = main_parser.add_subparsers(help='commands', dest='command')

    run_parser = command_parser.add_parser('run', help="Run")
    run_parser.add_argument('-d', '--duration', type=int, default=2, help="Duration in s")
    run_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    run_parser.add_argument('-s', '--sleep', type=int, default=1, help="Sleep between requests")
    run_parser.add_argument('-addr', '--rendezvous_address', type=str, default=RENDEZVOUS_ADDRESS, help="Sleep between requests")

    remote_parser = command_parser.add_parser('remote', help="Remote")
    remote_parser.add_argument('-d', '--duration', type=int, default=1, help="Duration in s")
    remote_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    remote_parser.add_argument('-s', '--sleep', type=int, default=1, help="Sleep between requests")
    remote_parser.add_argument('-addr', '--rendezvous_address', type=str, default=RENDEZVOUS_ADDRESS, help="Sleep between requests")

    plot_parser = command_parser.add_parser('plot', help="Plot")

    args = vars(main_parser.parse_args())
    print("Arguments:", args)
    command = args.pop('command')

    evalClient = EvalClient(args['rendezvous_address'])

    if command == 'run':
        target_datetime = datetime.datetime(2023, 6, 10, 23, 59)
        print(f"Sleeping until {target_datetime}...")
        pause.until(target_datetime)
        print(f"Starting evaluation")
        evalClient.run(**args)

    elif command == 'remote':
        evalClient.remote(**args)

    elif command == 'plot':
        evalClient.plot()