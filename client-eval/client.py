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

from proto import rendezvous_pb2 as pb
from proto import rendezvous_pb2_grpc as rdv

class EvalClient():
    def __init__(self):
        self.channel = grpc.insecure_channel("localhost:8001")
        self.stub = rdv.ClientServiceStub(self.channel)
        self.do_send = True
        self.results = {}

    def gather_thread(self, responses, latencies, total, task_id):
        num_responses = 0
        sum_latencies = 0
        for i, response in enumerate(responses):
            if response:
                num_responses += 1
                sum_latencies += latencies[i]
                

        #print(f"[Task {task_id}] Responses received: {num_responses} out of {total}", flush=True)
        #print(f"[Task {task_id}] Average latency: {sum_latencies/num_responses} ms", flush=True)

        self.results[task_id] = (num_responses, sum_latencies)
    
    def gather_all(self, duration, threads, rate):
        throughput = threads*rate
        total = duration*throughput
        sum_latencies = 0
        num_responses = 0

        for v in self.results.values():
            num_responses += v[0]
            sum_latencies += v[1]

        avg_latencies = sum_latencies/num_responses

        with open(f"./results/duration_{duration}_threads_{threads}_rate_{rate}.txt", "w") as file:
            file.write(f"Responses: {num_responses}/{total}\n")
            file.write(f"Avg. Latency: {avg_latencies} ms\n")
            file.write(f"Total throughput: {throughput} req/s\n")
            file.write(f"Results={throughput};{avg_latencies}")
            file.close()

        #print("\n-------------------------------------------------------", flush=True)
        #print(f"Total responses received: {num_responses} out of {total}", flush=True)
        #print(f"Total average latency: {avg_latencies} ms", flush=True)
        #print(f"Total throughput: {throughput} req/s\n", flush=True)



    def send(self, rate, duration, task_id):
        current = 0
        total = rate * duration

        responses = [None] * total
        latencies = [None] * total

        def process_async_response(response_future, i):
            end = time.time()
            try:
                responses[i] = response_future.result()
                latencies[i] = end - latencies[i]
            except grpc.RpcError as e:
                print("[ERROR]", e.details())
                latencies[i] = -1

        request = pb.RegisterBranchesMessage(rid=str(task_id), regions=["eu", "us"], service='eval_service')

        while current < total:
            now = time.time()
            next_second = now+1
            for _ in range(rate):
                copy_current = copy.deepcopy(current)
                latencies[current] = time.time()
                response_future = self.stub.RegisterBranches.future(request)

                callback_with_args = partial(process_async_response, i=copy_current)
                response_future.add_done_callback(callback_with_args)
                current += 1

            sleep_time = (next_second-time.time())/1000
            if sleep_time > 0:
                time.sleep(sleep_time)
            else:
                raise
            
        
        time.sleep(1)
    
        self.gather_thread(responses, latencies, total, task_id)

    def init(self, duration, threads, rate):
        thread_pool = []
        for i in range(threads):
            self.results[i] = None
            thread_pool.append(threading.Thread(target=self.send, args=(rate, duration, i)))

        for t in thread_pool:
            t.start()

        for t in thread_pool:
            t.join()

        self.gather_all(duration, threads, rate)

    def plot(self):
        latencies = []
        throughputs = []
        plt.clf()

        for file_path in glob.glob("./results/" + "*.txt"):
            with open(file_path, "r") as file:
                for line in file:
                    if "Results=" in line:
                        result_values = line.split("Results=")[1].strip()
                        avg_latency, throughput = result_values.split(";")
                        latencies.append(float(avg_latency))
                        throughputs.append(float(throughput))
        
        sorted_points = sorted(zip(latencies, throughputs))
        sorted_latencies, sorted_throughputs = zip(*sorted_points)
        plt.plot(sorted_latencies, sorted_throughputs, marker='o', color='blue', label='Data Points')
        plt.title("Latency by throughput")
        plt.xlabel("Throughput (req/s)")
        plt.ylabel("Latency (ms)")
        plot_name = f'plots/{time.time()}.png'
        plt.savefig(plot_name)
        print(f"Successfuly saved plot figure in {plot_name}!")
                        


# Usage: python3 client.py run -d 5 -t 15 -r 2
# OR   : python3 client.py run -d 1 -t 1 -r 1
# OR   : python3 client.py plot
if __name__ == '__main__':
    main_parser = argparse.ArgumentParser()
    command_parser = main_parser.add_subparsers(help='commands', dest='command')

    run_parser = command_parser.add_parser('run', help="Run")
    run_parser.add_argument('-d', '--duration', type=int, default=1, help="Duration in s")
    run_parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    run_parser.add_argument('-r', '--rate', type=int, default=1, required=True, help="Throughput (rate/s)")
    run_parser.add_argument('-s', '--start-time', type=str, help="Time of day to start executing")

    plot_parser = command_parser.add_parser('plot', help="Plot")

    args = vars(main_parser.parse_args())
    print("Arguments:", args)
    command = args.pop('command')

    evalClient = EvalClient()

    if command == 'run':
        start_time = args.pop('start_time')
        #target_datetime = datetime.datetime(2023, 6, 10, 23, 59)
        #print(f"Sleeping until {target_datetime}")
        #pause.until(target_datetime)

        evalClient.init(**args)

    elif command == 'plot':
        evalClient.plot()