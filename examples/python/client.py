import argparse
import sys
import time
import threading
import grpc

from rendezvous.protos import rendezvous_pb2_grpc as rendezvous_service
from rendezvous.protos import rendezvous_pb2 as rendezvous

# Input commands
REGISTER_REQUEST = "rr";
REGISTER_BRANCH = "rb";
REGISTER_BRANCHES = "rbs";
CLOSE_BRANCH = "cb";
WAIT_REQUEST = "wr";
CHECK_REQUEST = "cr";
CHECK_REQUEST_BY_REGIONS = "crr";
CHECK_PREVENTED_INCONSISTENCIES = "gi";
SLEEP = "sleep";
EXIT = "exit";

def toBytes(ctx):
    return ctx.SerializeToString()

def fromBytes(ctx):
    ctx = rendezvous.RequestContext()
    ctx.ParseFromString(ctx)
    return ctx

def registerRequest(stub, rid):
    try:
        response = stub.registerRequest(rendezvous.RegisterRequestMessage(rid=rid))
        print(f"[Register Request] {response}")
        return toBytes(response.context)
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def registerBranch(stub, rid, service, region):
    try:
        response = stub.registerBranch(rendezvous.RegisterBranchMessage(rid=rid, service=service, region=region))
        print(f"[Register Branch] {response}")
        return toBytes(response.context)
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def registerBranches(stub, rid, service, regions):
    try:
        response = stub.registerBranches(rendezvous.RegisterBranchesMessage(rid=rid, service=service, regions=regions))
        print(f"[Register Branches] {response}")
        return toBytes(response.context)
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def closeBranch(stub, rid, bid, region):
    try:
        stub.closeBranch(rendezvous.CloseBranchMessage(rid=rid, bid=bid, region=region))
        print(f"[Close Branch] done!\n")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def waitRequest(stub, rid, service, region, ctx=None):
    try:
        request = rendezvous.WaitRequestMessage(rid=rid, region=region, service=service)
        request.context.CopyFrom(fromBytes(ctx))
        print(f"[Wait Request] > waiting: {request}")
        response = stub.waitRequest(request)
        print(f"[Wait Request] < returning: prevented inconsistency = {response.preventedInconsistency}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def checkRequest(stub, rid, service, region, ctx=None):
    try:
        response = stub.checkRequest(rendezvous.CheckRequestMessage(rid=rid, service=service, region=region, serverctx=fromBytes(ctx)))
        status = rendezvous.RequestStatus.Name(response.status)
        print(f"[Check Request] status:\"{status}\"")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def checkRequestByRegions(stub, rid, service, ctx=None):
    try:
        response = stub.checkRequestByRegions(rendezvous.CheckRequestByRegionsMessage(rid=rid, service=service, serverctx=fromBytes(ctx)))
        print(f"[Check Request By Regions]:")
        for regionStatus in response.regionStatus:
            status = rendezvous.RequestStatus.Name(regionStatus.status)
            print(f"\t{regionStatus.region} : {status}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def getPreventedInconsistencies(stub):
    try:
        response = stub.getPreventedInconsistencies(rendezvous.Empty())
        print(f"[Get Prevented Inconsistencies] {response}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def showOptions():
    print(f"- Register Request: \t \t \t {REGISTER_REQUEST} <rid>")
    print(f"- Register Branch: \t \t \t {REGISTER_BRANCH} <rid> <service> <region>")
    print(f"- Register Branches: \t \t \t {REGISTER_BRANCHES} <rid> <service> <regions>")
    print(f"- Close Branch: \t \t \t {CLOSE_BRANCH} <rid> <bid> <region>")
    print(f"- Wait Request: \t \t \t {WAIT_REQUEST} <rid> <service> <region>")
    print(f"- Check Request: \t \t \t {CHECK_REQUEST} <rid> <service> <region>")
    print(f"- Check Request by Regions: \t \t {CHECK_REQUEST_BY_REGIONS} <rid> <service>")
    print(f"- Check Prevented Inconsistencies: \t {CHECK_PREVENTED_INCONSISTENCIES}")
    print(f"- Sleep: \t \t \t \t {SLEEP} <time in seconds>")
    print(f"- Exit: \t \t \t \t {EXIT}")
    print(f"- Change Stub: \t \t \t \t change 0|1")
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

        elif command == REGISTER_BRANCH and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            ctx = registerBranch(stub, rid, service, region)

        elif command == REGISTER_BRANCHES and len(args) >= 2:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None

            regions = []
            for region in args[2:]:
                regions.append(region)
            ctx = registerBranches(stub, rid, service, regions)

        elif command == CLOSE_BRANCH and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            bid = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            t = threading.Thread(target=closeBranch, args=(stub, rid, bid, region))
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
            checkRequest(stub, rid, service, region, ctx)

        elif command == CHECK_REQUEST_BY_REGIONS and len(args) >= 1 and len(args) <= 2:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            checkRequestByRegions(stub, rid, service, ctx)

        elif command == CHECK_PREVENTED_INCONSISTENCIES:
            getPreventedInconsistencies(stub)

        elif command == SLEEP:
            sleepTime = float(args[0])
            print(f"sleeping for {sleepTime} seconds...")
            time.sleep(sleepTime)
            print("welcome back!")

        elif command == EXIT:
            print("exiting...")
            sys.exit(1)

        elif command == "change":
            i = args[0]
            stub = stubs[int(i)]
            print("Changed to stub ", i)

        else:
            print("ERROR: Invalid command\n")

def runStressTest(stub):
    #TODO
    pass

def runScript(stub, script):
    #TODO
    pass

def main():
    parser = argparse.ArgumentParser(description='** Rendezvous Client **')
    parser.add_argument('--stress_test', action='store_true', help='Run stress test')
    parser.add_argument('--script', type=str, help='Run script')
    args = parser.parse_args()

    stubs = []
    channel = grpc.insecure_channel('localhost:8001')
    stub = rendezvous_service.RendezvousServiceStub(channel)
    stubs.append(stub)
    channel = grpc.insecure_channel('localhost:8002')
    stub = rendezvous_service.RendezvousServiceStub(channel)
    stubs.append(stub)

    if args.script:
        runScript(stub, args.script)
    elif args.stress_test:
        runStressTest(stub)
    else:
        showOptions()
        readInput(stubs)
        
    


if __name__ == '__main__':
    main()