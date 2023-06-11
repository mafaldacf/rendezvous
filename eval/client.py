from __future__ import print_function
import argparse
import sys
import time
import threading
import asyncio
import grpc
import grpc.experimental.aio as aiogrpc
from functools import partial
import copy
import pause
import datetime

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
    
    def gather_all(self, duration, threads, requests):
        throughput = threads*requests
        total = duration*throughput
        sum_latencies = 0
        num_responses = 0

        for v in self.results.values():
            num_responses += v[0]
            sum_latencies += v[1]

        avg_latencies = sum_latencies/num_responses

        print("\n-------------------------------------------------------", flush=True)
        print(f"Total responses received: {num_responses} out of {total}", flush=True)
        print(f"Total average latency: {avg_latencies} ms", flush=True)
        print(f"Total throughput: {throughput} req/s\n", flush=True)



    def send(self, requests, duration, task_id):
        current = 0
        total = requests * duration

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


        while current < total:
            now = time.time()
            next_second = now+1
            for _ in range(requests):
                copy_current = copy.deepcopy(current)
                latencies[current] = time.time()
                response_future = self.stub.RegisterBranches.future(pb.RegisterBranchesMessage(rid=str(task_id), regions=["eu", "us"], service='eval_service'))

                callback_with_args = partial(process_async_response, i=copy_current)
                response_future.add_done_callback(callback_with_args)
                current += 1

            sleep_time = (next_second-time.time())/1000
            if sleep_time > 0:
                time.sleep(sleep_time)
            
        
        time.sleep(5)
    
        self.gather_thread(responses, latencies, total, task_id)

    def init(self, duration, threads, requests):
        thread_pool = []
        for i in range(threads):
            self.results[i] = None
            thread_pool.append(threading.Thread(target=self.send, args=(requests, duration, i)))

        for t in thread_pool:
            t.start()

        for t in thread_pool:
            t.join()

        self.gather_all(duration, threads, requests)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--duration', type=int, default=1, help="Duration in s")
    parser.add_argument('-t', '--threads', type=int, default=1, help="Number of threads")
    parser.add_argument('-r', '--requests', type=int, default=1, required=True, help="Throughput (requests/s)")
    parser.add_argument('-s', '--start-time', type=str, help="Time of day to start executing")

    target_datetime = datetime.datetime(2023, 6, 10, 23, 59)
    print(f"Sleeping until {target_datetime}")
    pause.until(target_datetime)
    print("READY!")

    args = vars(parser.parse_args())
    args.pop('start_time')
    print("Arguments:", args)

    evalClient = EvalClient()
    evalClient.init(**args)