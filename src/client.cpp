#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <iostream>
#include <thread>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <sstream>
#include <exception>
#include <fstream>
#include "utils.h"

#include "monitor.grpc.pb.h"

using namespace utils;

class RendezvousClient {

  public:
      RendezvousClient(std::shared_ptr<grpc::Channel> channel) : stub(monitor::MonitorService::NewStub(channel))  { }

      void registerRequest() {
        grpc::ClientContext context;
        monitor::Empty request;
        monitor::RegisterRequestResponse response;

        auto status = stub->registerRequest(&context, request, &response);

        if (status.ok()) {
          log("registered request %ld", response.rid());                           
        }
        else {
          log("error: %d - %s", status.error_code(), status.error_message().c_str());                       
        }
      }

      void registerBranch(long rid, std::string service, std::string region) {
        grpc::ClientContext context;
        monitor::RegisterBranchMessage request;
        monitor::RegisterBranchResponse response;

        request.set_rid(rid);
        request.set_service(service);
        request.set_region(region);

        auto status = stub->registerBranch(&context, request, &response);

        if (status.ok()) {
          log("registered branch %ld for request %ld with context (serv=%s, reg=%s)", response.bid(), rid, service.c_str(), region.c_str());                           
        }
        else {
          log("error: %d - %s", status.error_code(), status.error_message().c_str());                       
        }
      }

      void registerBranches(long rid, int num, std::string service, std::string region) {
        grpc::ClientContext context;
        monitor::RegisterBranchesMessage request;
        monitor::RegisterBranchesResponse response;

        request.set_rid(rid);
        request.set_num(num);
        request.set_service(service);
        request.set_region(region);

        auto status = stub->registerBranches(&context, request, &response);

        if (status.ok()) {
          log("registered %d branches for request %ld with context (serv=%s, reg=%s)", num, rid, service.c_str(), region.c_str());                           
        }
        else {
          log("error: %d - %s", status.error_code(), status.error_message().c_str());                       
        }
      }

      void closeBranch(long rid, long bid) {
        grpc::ClientContext context;
        monitor::CloseBranchMessage request;
        monitor::Empty response;

        request.set_rid(rid);
        request.set_bid(bid);

        auto status = stub->closeBranch(&context, request, &response);

        if (status.ok()) {
          log("closed branch %ld for request %ld", bid, rid);                           
        }
        else {
          log("error: %d - %s", status.error_code(), status.error_message().c_str());                       
        }
      }

      void waitRequest(long rid, std::string service, std::string region) {
        grpc::ClientContext context;
        monitor::WaitRequestMessage request;
        monitor::Empty response;

        request.set_rid(rid);
        request.set_service(service);
        request.set_region(region);
        
        auto status = stub->waitRequest(&context, request, &response);

        if (status.ok()) {
          log("> successfully returned from wait request %ld on context (serv=%s, reg=%s) ", rid, service.c_str(), region.c_str());                           
        }
        else {
          log("error: %d - %s", status.error_code(), status.error_message().c_str());                       
        }
      }

      void checkRequest(long rid, std::string service, std::string region) {
        grpc::ClientContext context;
        monitor::CheckRequestMessage request;
        monitor::CheckRequestResponse response;

        request.set_rid(rid);
        request.set_service(service);
        request.set_region(region);

        auto status = stub->checkRequest(&context, request, &response);

        if (status.ok()) {
          log("> checked request %ld and got status %d on context (serv=%s, reg=%s) ", rid, response.status(), service.c_str(), region.c_str());                           
        }
        else {
          log("error: %d - %s", status.error_code(), status.error_message().c_str());                       
        }
      }

      void checkRequestByRegions(long rid, std::string service) {
        grpc::ClientContext context;
        monitor::CheckRequestByRegionsMessage request;
        monitor::CheckRequestByRegionsResponse response;

        request.set_rid(rid);
        request.set_service(service);

        auto status = stub->checkRequestByRegions(&context, request, &response);

        if (status.ok()) {
          log("> checked request %ld by regions on context (serv=%s) and got the following status: ", rid, service.c_str());
          for (monitor::RegionStatus pair : response.regionstatus()) {
            std::cout << "\t\t region " << pair.region() << " : " << pair.status() << std::endl;
          }                            
        }
        else {
          log("error: %d - %s", status.error_code(), status.error_message().c_str());                       
        }
      }

  private:
      std::unique_ptr<monitor::MonitorService::Stub> stub;

};

void showOptions() {
  std::cout << "- Register request: \t \t " << REGISTER_REQUEST << std::endl;
  std::cout << "- Register branch: \t \t " << REGISTER_BRANCH << " <rid> [<service>] [<region>]" << std::endl;
  std::cout << "- Register branches: \t \t " << REGISTER_BRANCHES << " <rid> <num> [<service>] [<region>]" << std::endl;
  std::cout << "- Close branch: \t \t " << CLOSE_BRANCH << " <rid> <bid>" << std::endl;
  std::cout << "- Wait request: \t \t " << WAIT_REQUEST << " <rid> [<service>] [<region>]" << std::endl;
  std::cout << "- Check request: \t \t " << CHECK_REQUEST << " <rid> [<service>] [<region>]" << std::endl;
  std::cout << "- Check request by regions: \t " << CHECK_REQUEST_BY_REGIONS << " <rid> [<service>]" << std::endl;
  std::cout << "- Sleep: \t \t \t " << SLEEP << " <time in ms>" << std::endl;
  std::cout << "- Exit: \t \t \t " << EXIT << std::endl;
}

void executeCommand(RendezvousClient* client, std::string cmd, std::string param1, std::string param2, std::string param3, std::string param4) {

    // switch case does not work for strings :(
    try {
      if (cmd == REGISTER_REQUEST) {
        client->registerRequest();
      }
      else if (cmd == REGISTER_BRANCH) {
        client->registerBranch(std::stol(param1), param2, param3);
      }
      else if (cmd == REGISTER_BRANCHES) {
        client->registerBranches(std::stol(param1), std::stoi(param2), param3, param4);
      }
      else if (cmd == CLOSE_BRANCH) {
        client->closeBranch(std::stol(param1), std::stol(param2));
      }
      else if (cmd == WAIT_REQUEST) {
        std::thread t([client, param1, param2, param3] {
          client->waitRequest(std::stol(param1), param2, param3);
        });

        // let thread run independently
        t.detach();
      }
      else if (cmd == CHECK_REQUEST) {
        client->checkRequest(std::stol(param1), param2, param3);
      }
      else if (cmd == CHECK_REQUEST_BY_REGIONS) {
        client->checkRequestByRegions(std::stol(param1), param2);
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

void runScript(RendezvousClient * client, std::string filepath) {
  showOptions();

  std::ifstream file(filepath);
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
}

void displayUsage (const char* appName){
  fprintf(stderr, "Invalid format:\n");
  fprintf(stderr, "Usage: %s [<script_path>]\n", appName);
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  std::cout << "** Rendezvous Client **" << std::endl << std::endl;

  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  RendezvousClient client(channel);

  if (argc == 1) {
    run(&client);
  }
  else if (argc == 2) {
    std::cout << "Running script file " << argv[1] << std::endl;
    runScript(&client, argv[1]);
  }
  else {
    displayUsage(argv[0]);
  }
  
  return 0;
}