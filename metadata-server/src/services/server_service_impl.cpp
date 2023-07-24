#include "server_service_impl.h"

using namespace service;

ServerServiceImpl::ServerServiceImpl(std::shared_ptr<rendezvous::Server> server)
    : _server(server) {
}
grpc::Status ServerServiceImpl::RegisterRequest(grpc::ServerContext* context, const rendezvous_server::RegisterRequestMessage* request, rendezvous_server::Empty* response) {
  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  spdlog::trace("[REPLICA] > registering request '{}'", request->rid().c_str());
  metadata::Request * rdv_request;
  std::string rid = request->rid();
  _server->getOrRegisterRequest(rid);
  spdlog::trace("[REPLICA] < registered request '{}'", request->rid().c_str());
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::RegisterBranch(grpc::ServerContext* context, 
  const rendezvous_server::RegisterBranchMessage* request, 
  rendezvous_server::Empty* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  spdlog::trace("[REPLICA] > registering branch for request '{}' on service='{}' and region='{}'", request->rid().c_str(), request->service().c_str(), request->region().c_str());

  std::string rid = request->rid();
  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  std::string bid = _server->parseFullBid(request->bid()).first; // full_bid format: <bid>:<rid>

  std::string res = _server->registerBranch(rdv_request, request->service(), request->region(), request->tag(), bid);
  spdlog::trace("[REPLICA] < registered branches with bid '{}' for request '{}' on service='{}' and region='{}'", bid, rdv_request->getRid().c_str(), request->service().c_str(), request->region().c_str());
  rdv_request->getVersionsRegistry()->updateRemoteVersion(request->context().replica_id(), request->context().request_version());
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::RegisterBranches(grpc::ServerContext* context, 
  const rendezvous_server::RegisterBranchesMessage* request, 
  rendezvous_server::Empty* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  spdlog::trace("[REPLICA] > registering {} branches with bid '' for request '{}' on service '{}'", request->regions().size(), request->bid(), request->rid(), request->service());

  const auto& regions = request->regions();
  int num = request->regions().size();
  metadata::Request * rdv_request = _server->getOrRegisterRequest(request->rid());
  std::string bid = _server->parseFullBid(request->bid()).first; // full_bid format: <bid>:<rid>

  std::string res = _server->registerBranches(rdv_request, request->service(), regions, request->tag(), bid);
  spdlog::trace("[REPLICA] < registered {} branches with bid '{}' for request '{}' on service '{}'", num, bid, request->rid(), request->service());
  rdv_request->getVersionsRegistry()->updateRemoteVersion(request->context().replica_id(), request->context().request_version());
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous_server::CloseBranchMessage* request, 
  rendezvous_server::Empty* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  spdlog::trace("[REPLICA] > closing branch w/ full bid '{}' on region={}", request->bid().c_str(), request->region().c_str());
  
  // full_bid format: <bid>:<rid>
  auto ids = _server->parseFullBid(request->bid());
  std::string bid = ids.first;
  std::string rid = ids.second;
  if (rid.empty() || bid.empty()) {
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_BID);
  }

  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  int res = _server->closeBranch(rdv_request, bid, request->region());
  if (res == 0) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  }
  else if (res == -1) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }
  
  spdlog::trace("[REPLICA] < closed branch '{}' for request '{}' on region={}", bid.c_str(), rid.c_str(), request->region().c_str());
  return grpc::Status::OK;
}