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

def registerBranch(stub, rid, service, regions, tag="", current_service=""):
    try:
        context = pb.RequestContext(current_service=current_service)
        response = stub.RegisterBranch(pb.RegisterBranchMessage(rid=rid, service=service, regions=regions, tag=tag, context=context, monitor=True))
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

def wait(stub, rid, service, region, ctx=None):
    try:
        request = pb.WaitRequestMessage(rid=rid, region=region, service=service)
        #request.context.CopyFrom(fromBytes(ctx))
        print(f"[Wait Request] > waiting: {request}")
        response = stub.Wait(request)
        print(f"[Wait Request] < returning: {response}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def checkStatus(stub, rid, service, region, detailed=False, ctx=None):
    try:
        #response = stub.CheckStatus(pb.CheckRequestMessage(rid=rid, service=service, region=region, serverctx=fromBytes(ctx)))
        response = stub.CheckStatus(pb.CheckStatusMessage(rid=rid, service=service, region=region, detailed=detailed))
        
        # python does not output OPENED (denoted as 0) values if we print the whole response
        status = pb.RequestStatus.Name(response.status)
        print(f"[Check Status] status:\"{status}\"")
        if detailed:
            print(f'--> tagged status: {response.tagged}')
            print(f'--> regions status: {response.regions}')
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

# Input commands
REGISTER_REQUEST = "rr";
REGISTER_BRANCH = "rb";
REGISTER_BRANCH_WITH_TAG = "rbtag";
REGISTER_BRANCH_CHILD = "rbchild";
CLOSE_BRANCH = "cb";
WAIT_REQUEST = "wr";
CHECK_STATUS = "cr";
CHECK_DETAILED_STATUS = "cdr";
CHANGE_STUB = "stub"
SLEEP = "sleep";
EXIT = "exit";

def showOptions():
    print(f"- Register Request: \t \t \t {REGISTER_REQUEST} <rid>")
    print(f"- Register Branch: \t \t \t {REGISTER_BRANCH} <rid> <service> <regions>")
    print(f"- Register Branch w/ tag: \t \t {REGISTER_BRANCH_WITH_TAG} <rid> <service> <regions> <tag> ")
    print(f"- Register Branch child: \t \t {REGISTER_BRANCH_CHILD} <rid> <service> <regions> <current_service> ")
    print(f"- Close Branch: \t \t \t {CLOSE_BRANCH} <bid> <region>")
    print(f"- Wait Request: \t \t \t {WAIT_REQUEST} <rid> <service> <region>")
    print(f"- Check Status: \t \t \t {CHECK_STATUS} <rid> <service> <region>")
    print(f"- Check Detailed Status: \t \t {CHECK_DETAILED_STATUS} <rid> <service> <region>")
    print(f"- Sleep: \t \t \t \t {SLEEP} <time in seconds>")
    print(f"- Exit: \t \t \t \t {EXIT}")
    print(f"- Change Stub: \t \t \t \t {CHANGE_STUB} 0|1")
    print()

def readInput(stubs):
    stub = stubs[0]
    ctx = None
    while True:
        user_input = input("> ")
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

        elif command == REGISTER_BRANCH_CHILD and len(args) >= 3:
            rid = args[0]
            service = args[1]

            regions = []
            for region in args[2:]:
                regions.append(region)

            current_service = regions.pop()
            ctx = registerBranch(stub, rid, service, regions, current_service=current_service)

        elif command == CLOSE_BRANCH and len(args) >= 1 and len(args) <= 2:
            bid = args[0]
            region = args[1]
            t = threading.Thread(target=closeBranch, args=(stub, bid, region))
            t.start()
            
        elif command == WAIT_REQUEST and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            t = threading.Thread(target=wait, args=(stub, rid, service, region, ctx))
            t.start()

        elif command == CHECK_STATUS and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            checkStatus(stub, rid, service, region, False, ctx)

        elif command == CHECK_DETAILED_STATUS and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            checkStatus(stub, rid, service, region, True, ctx)

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