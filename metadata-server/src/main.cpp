#include <iostream>
#include <csignal>
#include <cstdlib>
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
#include "client.grpc.pb.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#include <spdlog/sinks/stdout_color_sinks.h>

using json = nlohmann::json;

static std::string _replica_id;
static std::string _config_file;
static std::string _replica_addr;
static std::vector<std::string> _replicas_addrs;
static json _settings;

std::unique_ptr<grpc::Server> server;
std::unique_ptr<service::ClientServiceImpl> client_service;
std::unique_ptr<service::ServerServiceImpl> server_service;

// NOTE: causes mutex deadlock because of server->Wait
void sigintHandler(int sig) {
  server->Shutdown();
}

// NOTE: does not work using docker compose
void shutdown() {
  spdlog::info("Press any key to stop the server");
  getchar();
  spdlog::info("Shutting down server...");
  server->Shutdown();
}

void run() {
  auto rendezvous_server = std::make_shared<rendezvous::Server> (_replica_id, _settings);
  client_service = std::make_unique<service::ClientServiceImpl>(rendezvous_server, _replicas_addrs);
  server_service = std::make_unique<service::ServerServiceImpl>(rendezvous_server);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(_replica_addr, grpc::InsecureServerCredentials());
  //builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS, 2000);
  builder.RegisterService(client_service.get());
  builder.RegisterService(server_service.get());

  server = std::unique_ptr<grpc::Server>(builder.BuildAndStart());
  rendezvous_server->initRequestsCleanup();
  rendezvous_server->initSubscribersCleanup();
  //std::thread t(shutdown);
  //signal(SIGINT, sigintHandler);

  spdlog::info("Server listening on {}...", _replica_addr);
  server->Wait();
  //t.join();
}

void loadConfig() {
  std::ifstream file("../" + _config_file);
  if (!file.is_open()) {
    spdlog::error("Error opening JSON config");
    exit(-1);
  }

  spdlog::info("Parsing " + _config_file + "...");

  try {
    json root;
    file >> root;
    _settings = root["local_settings"];

    // load replicas addresses
    for (const auto &replica : root["replicas"].items()) {
      std::string id = replica.key();
      std::string addr = replica.value()["host"].get<std::string>() + ':' + std::to_string(replica.value()["port"].get<int>());

      if (_replica_id == id) {
        spdlog::info("{} --> {} (current replica)", id, addr);
        _replica_addr = "0.0.0.0:" + std::to_string(replica.value()["port"].get<int>());
      }
      else {
        _replicas_addrs.push_back(addr);
        spdlog::info("{} --> {} ", id, addr);
      }
    }
  }
  catch (json::exception &e) {
    spdlog::error("Error parsing JSON config");
    exit(-1);
  }
}

void usage(char *argv[]) {
  spdlog::error("Usage: {} <replica_id> <config_file>", argv[0]);

  // TODO for oficial code
  // std::cout << "Usage: " << argv[0] << " [--debug] [--logs] [--no_consistency_checks] <_replica_id>" << std::endl;
  exit(-1);
}

int main(int argc, char *argv[]) {
  spdlog::set_level(spdlog::level::trace);

  if (argc == 3) {
    _replica_id = argv[1];
    _config_file = argv[2];
  }
  else {
    spdlog::error("Invalid number of arguments");
    usage(argv);
  }
  spdlog::info("--------------- RENDEZVOUS SERVER ({}) --------------- ", _replica_id);
  loadConfig();
  run();
  return 0;
}