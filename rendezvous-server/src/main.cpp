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

std::unique_ptr<grpc::Server> server;

static std::string _replica_id = "replica-eu";
static std::string _replica_addr;
static std::vector<std::string> _replicas_addrs;
static int _requests_cleanup_sleep_m;
static int _subscribers_cleanup_sleep_m;
static int _subscribers_max_wait_time_s;
static int _wait_replica_timeout_s;

void sigintHandler(int sig) {
  server->Shutdown();
  exit(EXIT_SUCCESS);
}

void shutdown(std::unique_ptr<grpc::Server> &server) {
  // doesn't work using docker compose
  std::cout << "Press any key to stop the server..." << std::endl << std::endl;
  getchar();

  std::cout << "Stopping server..." << std::endl;
  server->Shutdown();
}

void run() {

  auto rendezvous_server = std::make_shared<rendezvous::Server> (
    _replica_id, _requests_cleanup_sleep_m, 
    _subscribers_cleanup_sleep_m, _subscribers_max_wait_time_s,
    _wait_replica_timeout_s);

  service::ClientServiceImpl client_service(rendezvous_server, _replicas_addrs);
  service::ServerServiceImpl server_service(rendezvous_server);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(_replica_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&client_service);
  builder.RegisterService(&server_service);

  server = std::unique_ptr<grpc::Server>(builder.BuildAndStart());

  spdlog::info("Server listening on {}...", _replica_addr);

  rendezvous_server->initRequestsCleanup();
  rendezvous_server->initSubscribersCleanup();
  signal(SIGINT, sigintHandler);
  // std::thread t(shutdown, std::ref(server));

  server->Wait();
}

void loadConfig() {
  std::ifstream file("../config.json");
  if (!file.is_open()) {
    spdlog::error("Error opening JSON config");
    exit(-1);
  }

  spdlog::info("Parsing JSON config...");

  try {
    json root;
    file >> root;
    // load timers for garbage collector
    _requests_cleanup_sleep_m = root["requests_cleanup_sleep_m"].get<int>();
    _subscribers_cleanup_sleep_m = root["subscribers_cleanup_sleep_m"].get<int>();

    // load timeouts for subscribers and replicas
    _subscribers_max_wait_time_s = root["subscribers_max_wait_time_s"].get<int>();
    _subscribers_max_wait_time_s = root["wait_replica_timeout_s"].get<int>();

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
  spdlog::error("Usage: {} <replica_id>", argv[0]);

  // TODO for oficial code
  // std::cout << "Usage: " << argv[0] << " [--debug] [--logs] [--no_consistency_checks] <_replica_id>" << std::endl;
  exit(-1);
}

int main(int argc, char *argv[]) {
  spdlog::set_level(spdlog::level::info);

  if (argc > 1) {
    if (argc == 2) {
      _replica_id = argv[1];
    }
    else if (argc > 2) {
      spdlog::error("Invalid number of arguments");
      usage(argv);
    }
  }

  loadConfig();

  spdlog::info("--------------- Rendezvous Server ({}) --------------- ", _replica_id);

  run();

  return 0;
}