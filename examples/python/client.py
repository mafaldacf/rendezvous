import argparse
import sys
import time
import grpc
import monitor_pb2 as pb
import monitor_pb2_grpc as monitor
import threading

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

def registerRequest(stub, rid):
    try:
        response = stub.registerRequest(pb.RegisterRequestMessage(rid=rid))
        print(f"[Register Request] {response}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def registerBranch(stub, rid, service, region):
    try:
        response = stub.registerBranch(pb.RegisterBranchMessage(rid=rid, service=service, region=region))
        print(f"[Register Branch] {response}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def registerBranches(stub, rid, num, service, region):
    try:
        response = stub.registerBranches(pb.RegisterBranchesMessage(rid=rid, num=num, service=service, region=region))
        print(f"[Register Branches] {response}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def closeBranch(stub, rid, bid):
    try:
        stub.closeBranch(pb.CloseBranchMessage(rid=rid, bid=bid))
        print(f"[Close Branch] done!\n")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def waitRequest(stub, rid, service, region):
    try:
        request = pb.WaitRequestMessage(rid=rid, service=service, region=region)
        print(f"[Wait Request] > waiting: {request}")
        stub.waitRequest(request)
        print(f"[Wait Request] < returning: {request}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def checkRequest(stub, rid, service, region):
    try:
        response = stub.checkRequest(pb.CheckRequestMessage(rid=rid, service=service, region=region))
        status = pb.RequestStatus.Name(response.status)
        print(f"[Check Request] status:\"{status}\"")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def checkRequestByRegions(stub, rid, service):
    try:
        response = stub.checkRequestByRegions(pb.CheckRequestByRegionsMessage(rid=rid, service=service))
        print(f"[Check Request By Regions]:")
        for regionStatus in response.regionStatus:
            status = pb.RequestStatus.Name(regionStatus.status)
            print(f"\t{regionStatus.region} : {status}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def getPreventedInconsistencies(stub):
    try:
        response = stub.getPreventedInconsistencies(pb.Empty())
        print(f"[Get Prevented Inconsistencies] {response}")
    except grpc.RpcError as e:
        print(f"[Error] {e.details()}")

def showOptions():
    print(f"- Register Request: \t \t \t {REGISTER_REQUEST} [<rid>]")
    print(f"- Register Branch: \t \t \t {REGISTER_BRANCH} <rid> [<service>] [<region>]")
    print(f"- Register Branches: \t \t \t {REGISTER_BRANCHES} <rid> <num> [<service>] [<region>]")
    print(f"- Close Branch: \t \t \t {CLOSE_BRANCH} <rid> <bid>")
    print(f"- Wait Request: \t \t \t {WAIT_REQUEST} <rid> [<service>] [<region>]")
    print(f"- Check Request: \t \t \t {CHECK_REQUEST} <rid> [<service>] [<region>]")
    print(f"- Check Request by Regions: \t \t {CHECK_REQUEST_BY_REGIONS} <rid> [<service>]")
    print(f"- Check Prevented Inconsistencies: \t {CHECK_PREVENTED_INCONSISTENCIES}")
    print(f"- Sleep: \t \t \t \t {SLEEP} <time in seconds>")
    print(f"- Exit: \t \t \t \t {EXIT}")
    print()

def readInput(stub):
    while True:
        user_input = input()
        parts = user_input.split()
        command = parts[0]
        args = parts[1:]

        if command == REGISTER_REQUEST and len(args) <= 1:
            rid = args[0] if len(args) == 1 else None
            registerRequest(stub, rid)

        elif command == REGISTER_BRANCH and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            registerBranch(stub, rid, service, region)

        elif command == REGISTER_BRANCHES and len(args) >= 2 and len(args) <= 4:
            rid = args[0]
            num = int(args[1])
            service = args[2] if len(args) >= 3 else None
            region = args[3] if len(args) >= 4 else None
            registerBranches(stub, rid, num, service, region)

        elif command == CLOSE_BRANCH and len(args) == 2:
            rid = args[0]
            bid = int(args[1])
            closeBranch(stub, rid, bid)
            
        elif command == WAIT_REQUEST and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            t = threading.Thread(target=waitRequest, args=(stub, rid, service, region))
            t.start()

        elif command == CHECK_REQUEST and len(args) >= 1 and len(args) <= 3:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            region = args[2] if len(args) >= 3 else None
            checkRequest(stub, rid, service, region)

        elif command == CHECK_REQUEST_BY_REGIONS and len(args) >= 1 and len(args) <= 2:
            rid = args[0]
            service = args[1] if len(args) >= 2 else None
            checkRequestByRegions(stub, rid, service)

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

    with grpc.insecure_channel('localhost:8000') as channel:
        stub = monitor.MonitorServiceStub(channel)

        if args.script:
            runScript(stub, args.script)
        elif args.stress_test:
            runStressTest(stub)
        else:
            showOptions()
            readInput(stub)
        
    


if __name__ == '__main__':
    main()