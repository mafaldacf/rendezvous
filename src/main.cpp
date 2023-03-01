#include <iostream>
#include <memory>
#include <string>
#include <cstdio> 
#include <grpcpp/grpcpp.h>
#include <thread>

#include "serviceImpl.h"
#include "request.h"
#include "monitor.grpc.pb.h"

void shutdown(std::unique_ptr<grpc::Server> & server, service::MonitorServiceImpl * service) {
  std::cout << "Press any key to stop the server..." << std::endl << std::endl;
  getchar();

  std::cout << "Stopping server..." << std::endl;
  server->Shutdown();
}


void run() {
  std::string server_address("localhost:8000");
  service::MonitorServiceImpl service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  std::cout << "Server listening on " << server_address << "..." << std::endl;

  std::thread t(shutdown, std::ref(server), &service);
  
  server->Wait();

  t.join();
}

int main(int argc, char* argv[]){
    std::cout << "** Rendezvous Server **" << std::endl << std::endl;
    
    run();
    
    return 0;
}