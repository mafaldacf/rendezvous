#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdio> 
#include <thread>
#include <vector>
#include <iostream>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <nlohmann/json.hpp>
#include "services/client_service_impl.h"
#include "services/server_service_impl.h"
#include "server.h"
#include "rendezvous.grpc.pb.h"

using json = nlohmann::json;

void shutdown(std::unique_ptr<grpc::Server> & server) {
  std::cout << "Press any key to stop the server..." << std::endl << std::endl;
  getchar();

  std::cout << "Stopping server..." << std::endl;
  server->Shutdown();
}


void run(std::string replicaId, std::string replicaAddr, std::vector<std::string> addrs) {

  std::shared_ptr<rendezvous::Server> rendezvousServer = std::make_shared<rendezvous::Server>(replicaId);
  service::RendezvousServiceImpl clientService(rendezvousServer, addrs);
  service::RendezvousServerServiceImpl serverService(rendezvousServer);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(replicaAddr, grpc::InsecureServerCredentials());
  builder.RegisterService(&clientService);
  builder.RegisterService(&serverService);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  std::cout << "Server listening on " << replicaAddr << "..." << std::endl;

  rendezvousServer->initCleanRequests();
  std::thread t(shutdown, std::ref(server));
  
  server->Wait();

  t.join();
}

std::string parseConfig(std::string replicaId, std::vector<std::string>& addrs) {
  std::string replicaAddr;

  std::ifstream file("../config.json");
  if (!file.is_open()) {
      std::cerr << "[ERROR] Failed to open config.json" << std::endl;
      exit(-1);
  }

  std::cout << "[INFO] Parsing JSON config..." << std::endl;

  try {
      json root;
      file >> root;

      for (const auto& replica : root.items()) {
        std::string id = replica.key();
        std::string addr = replica.value()["host"].get<std::string>() + ':' + std::to_string(replica.value()["port"].get<int>());

        if (replicaId == id) {
          std::cout << id << " --> " << addr << " (current replica)" << std::endl;
          replicaAddr = "0.0.0.0:" + std::to_string(replica.value()["port"].get<int>());
        }
        else {
          addrs.push_back(addr);
          std::cout << id << " --> " << addr << std::endl;
        }
    }
    std::cout << std::endl;
  } catch (json::exception& e) {
      std::cerr << "[ERROR] Failed to parse config.json: " << e.what() << std::endl;
      exit(-1);
  }
  return replicaAddr;
}

void usage(char* argv[]) {
  std::cout << "Usage: " << argv[0] << " <'replica id' as in config.json>" << std::endl;
  
  // TODO for oficial code
  //std::cout << "Usage: " << argv[0] << " [--debug] [--logs] [--no_consistency_checks] <replica_id>" << std::endl;
  exit(-1);
}

int main(int argc, char* argv[]) {
    std::string replicaId("replica-eu");

    std::vector<std::string> addrs;


    if (argc > 1) {
      if (argc == 2) {
        replicaId = argv[1];
      }
      else if (argc > 2) {
        std::cout << "[ERROR] Invalid number of arguments!" << std::endl;
        usage(argv);
      }
    }

    std::string replicaAddr = parseConfig(replicaId, addrs);
    
    std::cout << "** Rendezvous Server '" << replicaId << "' **" << std::endl << std::endl;
    
    run(replicaId, replicaAddr, addrs);
    
    return 0;
}