import grpc
from rendezvous.protos import rendezvous_pb2 as pb
from rendezvous.protos import rendezvous_pb2_grpc as rdv

def subscribe(stub, service, region):
    try:
        while True:
            reader = stub.Subscribe(pb.SubscribeMessage(service=service, region=region))
            print("waiting for new messages...")
            for response in reader:
                print(f"[Subscriber] {response}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

# Input commands
SUBSCRIBE = "sub";

def showOptions():
    print(f"- Subscribe: \t \t \t {SUBSCRIBE} <service> <region>")

def readInput(stubs):
    stub = stubs[0]
    while True:
        user_input = input()
        parts = user_input.split()
        command = parts[0]
        args = parts[1:]

        if command == SUBSCRIBE and len(args) == 2:
            service = args[0]
            region = args[1]
            subscribe(stub, service, region)

        else:
            print("ERROR: Invalid command\n")

def main():
    print("***** RENDEZVOUS MONITOR *****")
    stubs = []
    channel = grpc.insecure_channel('localhost:8001')
    stub = rdv.ClientServiceStub(channel)
    stubs.append(stub)
    channel = grpc.insecure_channel('localhost:8002')
    stub = rdv.ClientServiceStub(channel)
    stubs.append(stub)
    showOptions()
    readInput(stubs)

if __name__ == '__main__':
    main()