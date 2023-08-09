from __future__ import print_function
import grpc
from proto import rendezvous_pb2 as pb
from proto import rendezvous_pb2_grpc as rdv
import argparse


class EvalClient():
    def __init__(self):
        self.channel = grpc.insecure_channel("localhost:8001")
        self.stub = rdv.ClientServiceStub(self.channel)

    def do_send(self):
        request = pb.RegisterBranchesMessage(rid="rid", service="post-storage", regions=["eu-central-1"])
        try:
            response = self.stub.RegisterBranches(request)
            print(response)
        except grpc.RpcError as e:
            print("[ERROR]", e.details(), flush=True)

if __name__ == '__main__':
    main_parser = argparse.ArgumentParser()
    client = EvalClient()
    client.do_send()