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
#include "utils/settings.h"

using json = nlohmann::json;

static std::string _replica_id;
static std::string _connections_filename;
static std::string _replica_addr;
static std::vector<std::string> _replicas_addrs;
static json _settings;
static bool _consistency_checks;
static bool _async_replication;

std::unique_ptr<grpc::Server> server;
std::unique_ptr<service::ClientServiceImpl> client_service;
std::unique_ptr<service::ServerServiceImpl> server_service;

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
  client_service = std::make_unique<service::ClientServiceImpl>(rendezvous_server, _replicas_addrs, _consistency_checks);
  server_service = std::make_unique<service::ServerServiceImpl>(rendezvous_server, _consistency_checks);

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
  // load consistency checks flag from environment variable
  auto consistency_checks_env = std::getenv("CONSISTENCY_CHECKS");
  if (consistency_checks_env) {
    _consistency_checks = (atoi(consistency_checks_env) == 1);
  } else { // true by default
    _consistency_checks = true;
  }

  utils::CONSISTENCY_CHECKS = _consistency_checks;

  /* Parse settings config */
  std::ifstream settings_file("../config/settings.json");
  if (!settings_file.is_open()) {
    spdlog::error("Error opening 'settings.json' config");
    exit(-1);
  }
  spdlog::info("Parsing 'settings.json'...");
  try {
    json root;
    settings_file >> root;
    _settings = root;
    utils::ASYNC_REPLICATION = _settings["async_replication"].get<bool>();
    utils::CONTEXT_VERSIONING = _settings["context_versioning"].get<bool>();
  }
  catch (json::exception &e) {
    spdlog::error("Error parsing 'settings.json'");
    exit(-1);
  }

  /* Parse connections config */
  std::ifstream connections_file("../config/connections/" + _connections_filename);
  if (!connections_file.is_open()) {
    spdlog::error("Error opening JSON config");
    exit(-1);
  }
  spdlog::info("Parsing connections file '" + _connections_filename + "'...");
  try {
    json root;
    connections_file >> root;

    // load replicas addresses
    for (const auto &replica : root.items()) {
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
  spdlog::error("Usage: {} REPLICA_ID CONFIG", argv[0]);

  // TODO for oficial code
  // std::cout << "Usage: " << argv[0] << " [--debug] [--logs] [--no_consistency_checks] <_replica_id>" << std::endl;
  exit(-1);
}

int main(int argc, char *argv[]) {
  // levels: critical, error, warn, info, debug, trace
  spdlog::set_level(spdlog::level::trace);

  if (argc == 3) {
    _replica_id = argv[1];
    _connections_filename = argv[2];
  }
  else {
    spdlog::error("Invalid number of arguments");
    usage(argv);
  }
  spdlog::info("--------------- RENDEZVOUS SERVER ({}) --------------- ", _replica_id);
  loadConfig();
  spdlog::info("CONSYSTENCY CHECKS: '{}'", utils::CONSISTENCY_CHECKS);
  spdlog::info("ASYNC REPLICATION: '{}'", utils::ASYNC_REPLICATION);
  run();
  return 0;
}