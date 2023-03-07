#include <iostream>
#include <memory>
#include <string>
#include <cstdio> 
#include <grpcpp/grpcpp.h>
#include <thread>

#include "serviceImpl.h"
#include "request.h"
#include "rendezvous.grpc.pb.h"

void shutdown(std::unique_ptr<grpc::Server> & server, service::RendezvousServiceImpl * service) {
  std::cout << "Press any key to stop the server..." << std::endl << std::endl;
  getchar();

  std::cout << "Stopping server..." << std::endl;
  server->Shutdown();
}


void run(std::string id, std::string host, std::string port) {
  std::string server_address(host + ':' + port);
  service::RendezvousServiceImpl service(id);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  std::cout << "Server listening on " << server_address << "..." << std::endl;

  std::thread t(shutdown, std::ref(server), &service);
  
  server->Wait();

  t.join();
}

void usage(char* argv[]) {
  std::cout << "Usage: " << argv[0] << " <id> <host> <port>" << std::endl;
  std::cout << "Example: " << argv[0] << " eu localhost 8000" << std::endl;
  exit(-1);
}

int main(int argc, char* argv[]) {
    std::string id("eu"); // eu -> europe
    std::string host("0.0.0.0");
    std::string port("8001");

    if (argc > 1) {
      if (argc == 2) {
        id = argv[1];
      }
      if (argc == 3) {
        host = argv[2];
      }
      if (argc == 4) {
        port = argv[3];
      }
      if (argc > 4) {
        std::cout << "[ERROR] Invalid arguments!" << std::endl;
        usage(argv);
      }
    }

    std::cout << "** Rendezvous Server '" << id << "' **" << std::endl << std::endl;
    
    run(id, host, port);
    
    return 0;
}