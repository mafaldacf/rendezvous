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
  //spdlog::trace("> [REPLICATED RR] register request '{}'", request->rid());

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
  const std::string& rid = request->rid();
  const std::string& full_bid = request->bid();
  const auto& regions = request->regions();
  bool monitor = request->monitor();
  int num = request->regions().size();

  //spdlog::trace("> [REPLICATED RB] register #{} branches with bid '{}' for request '{}' on service '{}' (monitor={})", num, full_bid, rid, service, monitor);

  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  // parse bid from <bid>:<rid>
  std::string bid = _server->parseFullBid(full_bid).first;
  
  // wait for all remote versions in the context
  if (_async_replication) {
    auto versions_registry = rdv_request->getVersionsRegistry();
    // pair: <sid, version>
    for (const auto& v : request->context().versions()) {
      versions_registry->updateRemoteVersion(v.first, v.second);
    }
  }

  std::string res = _server->registerBranch(rdv_request, service, regions, tag, monitor, bid);

  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous_server::CloseBranchMessage* request, 
  rendezvous_server::Empty* response) {

  if (!_consistency_checks) return grpc::Status::OK;
  
  //spdlog::trace("> [REPLICATED CB] closing branch with full bid '{}' on region '{}'", request->bid(), request->region());
  
  // parse identifiers from <bid>:<rid>
  auto ids = _server->parseFullBid(request->bid());
  std::string bid = ids.first;
  std::string rid = ids.second;
  if (rid.empty() || bid.empty()) {
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_BID);
  }
  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);

  // wait for all remote versions in the context
  if (_async_replication) {
    auto versions_registry = rdv_request->getVersionsRegistry();
    // pair: <sid, version>
    for (const auto& v : request->context().versions()) {
      versions_registry->updateRemoteVersion(v.first, v.second);
    }
  }

  // always force close branch when dealing with replicated requests
  int res = _server->closeBranch(rdv_request, bid, request->region(), true);

  if (res == 0) {
    spdlog::error("> [REPLICATED CB] Error: branch with full bid '{}' not found", request->bid());
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  }
  else if (res == -1) {
    spdlog::error("> [REPLICATED CB] Error: invalid region {} for branch with full bid {}", 
      request->region(), request->bid());
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }
  
  return grpc::Status::OK;
}