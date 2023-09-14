import argparse
import sys
import time
import threading
import grpc
from rendezvous.protos import rendezvous_pb2 as pb
from rendezvous.protos import rendezvous_pb2_grpc as rdv

def toBytes(ctx):
    return ctx.SerializeToString()

def fromBytes(ctx):
    ctx = pb.RequestContext()
    ctx.ParseFromString(ctx)
    return ctx

def registerRequest(stub, rid):
    try:
        response = stub.RegisterRequest(pb.RegisterRequestMessage(rid=rid))
        print(f"[Register Request] {response}")
        return toBytes(response.context)
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def registerBranch(stub, rid, service, regions, tag=""):
    try:
        response = stub.RegisterBranch(pb.RegisterBranchMessage(rid=rid, service=service, regions=regions, tag=tag, monitor=True))
        print(f"[Register Branches] {response}")
        return toBytes(response.context)
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def closeBranch(stub, bid, region):
    try:
        stub.CloseBranch(pb.CloseBranchMessage(bid=bid, region=region))
        print(f"[Close Branch] done!\n")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def waitRequest(stub, rid, service, region, ctx=None):
    try:
        request = pb.WaitRequestMessage(rid=rid, region=region, service=service)
        #request.context.CopyFrom(fromBytes(ctx))
        print(f"[Wait Request] > waiting: {request}")
        response = stub.WaitRequest(request)
        print(f"[Wait Request] < returning: {response}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def checkRequest(stub, rid, service, region, detailed=False, ctx=None):
    try:
        #response = stub.CheckRequest(pb.CheckRequestMessage(rid=rid, service=service, region=region, serverctx=fromBytes(ctx)))
        response = stub.CheckRequest(pb.CheckRequestMessage(rid=rid, service=service, region=region, detailed=detailed))
        
        # python does not output OPENED (denoted as 0) values if we print the whole response
        status = pb.RequestStatus.Name(response.status)
        print(f"[Check Request] status:\"{status}\"")
        if detailed:
            print(f'--> detailed status: {response.detailed}')
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def checkRequestByRegions(stub, rid, service, ctx=None):
    try:
        #response = stub.CheckRequestByRegions(pb.CheckRequestByRegionsMessage(rid=rid, service=service, serverctx=fromBytes(ctx)))
        response = stub.CheckRequestByRegions(pb.CheckRequestByRegionsMessage(rid=rid, service=service))
        print(f"[Check Request By Regions]:")
        for regionStatus in response.regionStatus:
            status = rdv.RequestStatus.Name(regionStatus.status)
            print(f"\t{regionStatus.region} : {status}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

# Input commands
REGISTER_REQUEST = "rr";
REGISTER_BRANCH = "rb";
REGISTER_BRANCH_WITH_TAG = "rbtag";
CLOSE_BRANCH = "cb";
WAIT_REQUEST = "wr";
CHECK_REQUEST = "cr";
CHECK_DETAILED_REQUEST = "cdr";
CHECK_REQUEST_BY_REGIONS = "crr";
CHANGE_STUB = "stub"
SLEEP = "sleep";
EXIT = "exit";

def showOptions():
    print(f"- Register Request: \t \t \t {REGISTER_REQUEST} <rid>")
    print(f"- Register Branch: \t \t \t {REGISTER_BRANCH} <rid> <service> <regions>")
    print(f"- Register Branch w/ tag: \t \t {REGISTER_BRANCH_WITH_TAG} <rid> <service> <regions> <tag> ")
    print(f"- Close Branch: \t \t \t {CLOSE_BRANCH} <bid> <region>")
    print(f"- Wait Request: \t \t \t {WAIT_REQUEST} <rid> <service> <region>")
    print(f"- Check Request: \t \t \t {CHECK_REQUEST} <rid> <service> <region>")
    print(f"- Check Detailed Request: \t \t {CHECK_DETAILED_REQUEST} <rid> <service> <region>")
    print(f"- Check Request by Regions: \t \t {CHECK_REQUEST_BY_REGIONS} <rid> <service>")
    print(f"- Sleep: \t \t \t \t {SLEEP} <time in seconds>")
    print(f"- Exit: \t \t \t \t {EXIT}")
    print(f"- Change Stub: \t \t \t \t {CHANGE_STUB} 0|1")
    print()

def readInput(stubs):
    stub = stubs[0]
    ctx = None
    while True:
        user_input = input()
        parts = user_input.split()
        command = parts[0]
        args = parts[1:]

        if command == REGISTER_REQUEST and len(args) <= 1:
            rid = args[0] if len(args) == 1 else None
            ctx = registerRequest(stub, rid)

        elif command == REGISTER_BRANCH and len(args) >= 2:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None

            regions = []
            for region in args[2:]:
                regions.append(region)
            ctx = registerBranch(stub, rid, service, regions)

        elif command == REGISTER_BRANCH_WITH_TAG and len(args) >= 3:
            rid = args[0]
            service = args[1]

            regions = []
            for region in args[2:]:
                regions.append(region)

            tag = regions.pop()
            ctx = registerBranch(stub, rid, service, regions, tag)

        elif command == CLOSE_BRANCH and len(args) >= 1 and len(args) <= 2:
            bid = args[0]
            region = args[1]
            t = threading.Thread(target=closeBranch, args=(stub, bid, region))
            t.start()
            
        elif command == WAIT_REQUEST and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            t = threading.Thread(target=waitRequest, args=(stub, rid, service, region, ctx))
            t.start()

        elif command == CHECK_REQUEST and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            checkRequest(stub, rid, service, region, False, ctx)

        elif command == CHECK_DETAILED_REQUEST and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            checkRequest(stub, rid, service, region, True, ctx)

        elif command == CHECK_REQUEST_BY_REGIONS and len(args) >= 1 and len(args) <= 2:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            checkRequestByRegions(stub, rid, service, ctx)

        elif command == SLEEP:
            sleepTime = float(args[0])
            print(f"sleeping for {sleepTime} seconds...")
            time.sleep(sleepTime)
            print("welcome back!")

        elif command == EXIT:
            print("exiting...")
            sys.exit(1)

        elif command == CHANGE_STUB:
            i = args[0]
            stub = stubs[int(i)]
            print("Changed to stub ", i)

        else:
            print("ERROR: Invalid command\n")

def main():
    print("***** RENDEZVOUS CLIENT *****")
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