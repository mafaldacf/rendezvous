#include <unistd.h>
#include <iostream>
#include <thread>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <sstream>
#include <exception>
#include <fstream>

#include "monitor.grpc.pb.h"
#include "utils.h"

/* ------------------------------------

SIMPLE CLIENT CODE FOR TESTING PURPOSES

------------------------------------- */

#ifndef DEBUG 
#define DEBUG 1 // set debug mode
#endif

#if DEBUG
#define log(...) {\
    char str[100];\
    sprintf(str, __VA_ARGS__);\
    std::cout << "[" << __FUNCTION__ << "] " << str << std::endl;\
    }
#else
#define log(...)
#endif

class RendezvousClient {

  public:
      RendezvousClient(std::shared_ptr<grpc::Channel> channel) : stub(monitor::MonitorService::NewStub(channel))  { }

      void logError(grpc::Status status) {
        log("error: %s - %s", StatusCodeToString(status.error_code()).c_str(), status.error_message().c_str());
      }

      void registerRequest(std::string rid) {
        grpc::ClientContext context;
        monitor::RegisterRequestMessage request;
        monitor::RegisterRequestResponse response;

        request.set_rid(rid);

        auto status = stub->registerRequest(&context, request, &response);

        if (status.ok()) {
          log("registered request '%s'", response.rid().c_str());
        }
        else {
          logError(status);
        }                       
      }

      void registerBranch(std::string rid, std::string service, std::string region) {
        grpc::ClientContext context;
        monitor::RegisterBranchMessage request;
        monitor::RegisterBranchResponse response;

        request.set_rid(rid);
        request.set_service(service);
        request.set_region(region);

        auto status = stub->registerBranch(&context, request, &response);

        if (status.ok()) {
          log("registered branch %ld for request '%s' with context (serv=%s, reg=%s)", response.bid(), rid.c_str(), service.c_str(), region.c_str());
        }
        else {
          logError(status);
        } 
      }

      void registerBranches(std::string rid, int num, std::string service, std::string region) {
        grpc::ClientContext context;
        monitor::RegisterBranchesMessage request;
        monitor::RegisterBranchesResponse response;

        request.set_rid(rid);
        request.set_num(num);
        request.set_service(service);
        request.set_region(region);

        auto status = stub->registerBranches(&context, request, &response);

        if (status.ok()) {
          log("registered %d branches for request '%s' with context (serv=%s, reg=%s)", num, rid.c_str(), service.c_str(), region.c_str());                           
        }
        else {
          logError(status);
        } 
      }

      void closeBranch(std::string rid, long bid) {
        grpc::ClientContext context;
        monitor::CloseBranchMessage request;
        monitor::Empty response;

        request.set_rid(rid);
        request.set_bid(bid);

        auto status = stub->closeBranch(&context, request, &response);

        if (status.ok()) {
          log("closed branch %ld for request '%s'", bid, rid.c_str());
        }
        else {
          logError(status);
        }
      }

      void waitRequest(std::string rid, std::string service, std::string region) {
        grpc::ClientContext context;
        monitor::WaitRequestMessage request;
        monitor::Empty response;

        request.set_rid(rid);
        request.set_service(service);
        request.set_region(region);
        
        auto status = stub->waitRequest(&context, request, &response);

        if (status.ok()) {
          log("! successfully returned from wait request '%s' with context (serv=%s, reg=%s)", rid.c_str(), service.c_str(), region.c_str());                           
        }
        else {
          logError(status);
        }
      }

      void checkRequest(std::string rid, std::string service, std::string region) {
        grpc::ClientContext context;
        monitor::CheckRequestMessage request;
        monitor::CheckRequestResponse response;

        request.set_rid(rid);
        request.set_service(service);
        request.set_region(region);

        auto status = stub->checkRequest(&context, request, &response);

        if (status.ok()) {
          log("check request '%s' on context (serv=%s, reg=%s)", rid.c_str(), service.c_str(), region.c_str());
        }
        else {
          logError(status);
        }
      }

      void checkRequestByRegions(std::string rid, std::string service) {
        grpc::ClientContext context;
        monitor::CheckRequestByRegionsMessage request;
        monitor::CheckRequestByRegionsResponse response;

        request.set_rid(rid);
        request.set_service(service);

        auto status = stub->checkRequestByRegions(&context, request, &response);

        if (status.ok()) {
          log("check request '%s' by regions on context (serv=%s) and got the following status:", rid.c_str(), service.c_str());
          for (monitor::RegionStatus pair : response.regionstatus()) {
            log("\t\t region %s : %s", pair.region().c_str(), RequestStatusToString(pair.status()).c_str());
          }                            
        }
        else {
          logError(status);                       
        }
      }

      void getPreventedInconsistencies() {
        grpc::ClientContext context;
        monitor::Empty request;
        monitor::GetPreventedInconsistenciesResponse response;

        auto status = stub->getPreventedInconsistencies(&context, request, &response);

        if (status.ok()) {
          log("got number of prevented inconsistencies: %ld", response.inconsistencies());
        }
        else {
          logError(status);
        }                       
      }

  private:
      std::unique_ptr<monitor::MonitorService::Stub> stub;

};

void showOptions() {
  std::cout << "- Register request: \t \t " << REGISTER_REQUEST << " [<rid>]" << std::endl;
  std::cout << "- Register branch: \t \t " << REGISTER_BRANCH << " <rid> [<service>] [<region>]" << std::endl;
  std::cout << "- Register branches: \t \t " << REGISTER_BRANCHES << " <rid> <num> [<service>] [<region>]" << std::endl;
  std::cout << "- Close branch: \t \t " << CLOSE_BRANCH << " <rid> <bid>" << std::endl;
  std::cout << "- Wait request: \t \t " << WAIT_REQUEST << " <rid> [<service>] [<region>]" << std::endl;
  std::cout << "- Check request: \t \t " << CHECK_REQUEST << " <rid> [<service>] [<region>]" << std::endl;
  std::cout << "- Check request by regions: \t " << CHECK_REQUEST_BY_REGIONS << " <rid> [<service>]" << std::endl;
  std::cout << "- Get inconsistencies: \t \t " << GET_INCONSISTENCIES << std::endl;
  std::cout << "- Sleep: \t \t \t " << SLEEP << " <time in ms>" << std::endl;
  std::cout << "- Exit: \t  \t \t " << EXIT << std::endl;
}

void executeCommand(RendezvousClient* client, std::string cmd, std::string param1, std::string param2, std::string param3, std::string param4) {

    // switch case does not work for strings :(
    try {
      if (cmd == REGISTER_REQUEST) {
        client->registerRequest(param1);
      }
      else if (cmd == REGISTER_BRANCH) {
        client->registerBranch(param1, param2, param3);
      }
      else if (cmd == REGISTER_BRANCHES) {
        client->registerBranches(param1, std::stoi(param2), param3, param4);
      }
      else if (cmd == CLOSE_BRANCH) {
        client->closeBranch(param1, std::stol(param2));
      }
      else if (cmd == WAIT_REQUEST) {
        std::thread t([client, param1, param2, param3] {
          client->waitRequest(param1, param2, param3);
        });

        // let thread run independently
        t.detach();
      }
      else if (cmd == CHECK_REQUEST) {
        client->checkRequest(param1, param2, param3);
      }
      else if (cmd == CHECK_REQUEST_BY_REGIONS) {
        client->checkRequestByRegions(param1, param2);
      }
      else if (cmd == GET_INCONSISTENCIES) {
        client->getPreventedInconsistencies();
      }
      else if (cmd == SLEEP) {
        std::cout << "Sleeping for " << param1 << " ms..." << std::endl;
        sleep(stoi(param1)*0.001);
        std::cout << "Done! " << std::endl;
      }
      else if (cmd == EXIT) {
        std::cout << "Exiting..." << std::endl;
        exit(0);
      }
      else {
        std::cout << "Invalid command " << cmd << " " << param1 << " " << param2 << " " << param3 << " " << param4 << std::endl;
      }
    }
    catch (std::exception& e) {
      std::cout << "Invalid command: " << e.what() << std::endl;
    }
}

void run(RendezvousClient * client) {
  showOptions();

  while (true) {
    std::string input = "", cmd = "", param1 = "", param2 = "", param3 = "", param4 = "";
    std::getline(std::cin, input);
    std::stringstream ss(input);
    
    ss >> cmd >> param1 >> param2 >> param3 >> param4; // ignore if parameters are missing

    executeCommand(client, cmd, param1, param2, param3, param4);
  }
}

void runScript(RendezvousClient * client, std::string filename) {
  std::cout << "Running script file " << filename.c_str() << "..." << std::endl;

  std::ifstream file(filename);
  if (file.is_open()) {
    while (true) {
      std::string input = "", cmd = "", param1 = "", param2 = "", param3 = "", param4 = "";
      std::getline(file, input);
      std::stringstream ss(input);
      
      ss >> cmd >> param1 >> param2 >> param3 >> param4; // ignore if parameters are missing

      executeCommand(client, cmd, param1, param2, param3, param4);
    }
    file.close();
  }
  else {
    std::cerr << "Invalid script" << std::endl;
    exit(EXIT_FAILURE);
  }
}

void runStressTest(RendezvousClient * client) {
  std::cout << "Running stress test..." << std::endl;

  /*
  Max tested so far:
    - NUM_REQUESTS = 100000
    - NUM_BRANCHES = 1000
  */
  int num_requests = 100000;
  int num_branches = 1000;

  std::cout << "-- registering " << num_requests << " requests and " << num_branches << " branches for each request --" << std::endl;

  for (int rid = 0; rid < num_requests; rid++) {

    client->registerRequest(std::to_string(rid));

    client->registerBranches(std::to_string(rid), num_branches/4, "", "");
    client->registerBranches(std::to_string(rid), num_branches/4, "s", "");
    client->registerBranches(std::to_string(rid), num_branches/4, "", "r");
    client->registerBranches(std::to_string(rid), num_branches/4, "s", "r");
  }

  std::cout << "done!" << std::endl;

}

void displayUsage (const char* appName){
  fprintf(stderr, "Invalid format:\n");

  // provide script name if running with start.sh
  // provide script path if running client directly: e.g. ../../../scripts/<script name>
  fprintf(stderr, "Usage: %s [--script <script name>] | [--stress_test]\n", appName);

  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  std::cout << "** Rendezvous Client **" << std::endl << std::endl;

  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  RendezvousClient client(channel);

  if (argc == 1) {
    run(&client);
  }
  else if (argc == 2 && !strcmp(argv[1], "--stress_test")) {
    runStressTest(&client);
  }
  else if (argc == 3 && !strcmp(argv[1], "--script")) {
    runScript(&client, argv[2]);
  }
  else {
    displayUsage(argv[0]);
  }
  
  return 0;
}