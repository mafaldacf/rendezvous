from __future__ import print_function
import grpc
from proto import rendezvous_pb2 as pb
from proto import rendezvous_pb2_grpc as rdv


class EvalClient():
    def __init__(self):
        self.channel = grpc.insecure_channel("localhost:8001")
        self.stub = rdv.ClientServiceStub(self.channel)

    def do_send(self):
        request = pb.RegisterBranchesMessage2(rid="rid", datastores=["datastore"], regions=["EU", "US"])
        
        try:
            response = self.stub.RegisterBranches2(request)
            print(response)
        except grpc.RpcError as e:
            print("[ERROR]", e.details(), flush=True)

if __name__ == '__main__':
    client = EvalClient()
    client.do_send()