#include "server_service_impl.h"

using namespace service;

ServerServiceImpl::ServerServiceImpl(std::shared_ptr<rendezvous::Server> server, bool async_replication)
    : _server(server), _async_replication(async_replication) {
      auto consistency_checks_env = std::getenv("CONSISTENCY_CHECKS");
      if (consistency_checks_env) {
      _consistency_checks = (atoi(consistency_checks_env) == 1);
    } else { // true by default
      _consistency_checks = true;
    }
}
grpc::Status ServerServiceImpl::RegisterRequest(grpc::ServerContext* context, const rendezvous_server::RegisterRequestMessage* request, rendezvous_server::Empty* response) {
  if (!_consistency_checks) return grpc::Status::OK;
  spdlog::trace("> [REPLICATED RR] register request '{}'", request->rid());

  metadata::Request * rdv_request;
  std::string rid = request->rid();
  _server->getOrRegisterRequest(rid);
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::RegisterBranch(grpc::ServerContext* context, 
  const rendezvous_server::RegisterBranchMessage* request, 
  rendezvous_server::Empty* response) {

  if (!_consistency_checks) return grpc::Status::OK;

  const std::string& service = request->service();
  const std::string& tag = request->tag();
  const std::string& root_rid = request->root_rid();
  const std::string& sub_rid = request->sub_rid();
  const std::string& core_bid = request->core_bid();
  const auto& regions = request->regions();
  bool monitor = request->monitor();
  bool async = request->async();
  int num = request->regions().size();

  spdlog::trace("> [REPLICATED RB] register #{} branches on service '{}' (monitor={}) for ids {}:{}:{}", num, service, monitor, core_bid, root_rid, sub_rid);

  metadata::Request * rdv_request = _server->getOrRegisterRequest(root_rid);
  if (rdv_request == nullptr) {
    spdlog::critical("< [REPLICATED CB] Error: invalid request for ids {}:{}:{}", core_bid, root_rid, sub_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  
  // wait for all remote versions in the context
  if (_async_replication) {
    auto versions_registry = rdv_request->getVersionsRegistry();
    // pair: <sid, version>
    for (const auto& v : request->context().versions()) {
      versions_registry->updateRemoteVersion(v.first, v.second);
    }
  }

  if (async) {
    _server->addNextSubRequest(rdv_request, sub_rid, false);
  }

  std::string res = _server->registerBranch(rdv_request, sub_rid, service, regions, tag, request->context().prev_service(), monitor, core_bid);

  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous_server::CloseBranchMessage* request, 
  rendezvous_server::Empty* response) {

  if (!_consistency_checks) return grpc::Status::OK;

  const std::string& root_rid = request->root_rid();
  const std::string& core_bid = request->core_bid();
  const std::string& region = request->region();
  
  spdlog::trace("> [REPLICATED CB] closing branch on region '{}' for ids {}:{}", region, core_bid, root_rid);

  metadata::Request * rdv_request = _server->getOrRegisterRequest(root_rid);
  if (rdv_request == nullptr) {
    spdlog::critical("< [REPLICATED CB] Error: invalid request for ids {}:{}", core_bid, root_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  // wait for all remote versions in the context
  if (_async_replication) {
    auto versions_registry = rdv_request->getVersionsRegistry();
    // pair: <sid, version>
    for (const auto& v : request->context().versions()) {
      versions_registry->updateRemoteVersion(v.first, v.second);
    }
  }

  // always force close branch when dealing with replicated requests
  int res = _server->closeBranch(rdv_request, core_bid, region, true);

  if (res == 0) {
    spdlog::critical("< [REPLICATED CB] Error: branch not found for ids {}:{}", core_bid, root_rid);
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  } else if (res == -1) {
    spdlog::critical("< [REPLICATED CB] Error: region '{}' not found for branch for ids {}:{}", region, core_bid, root_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }
  
  return grpc::Status::OK;
}