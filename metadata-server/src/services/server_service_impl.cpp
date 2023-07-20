#include "server_service_impl.h"

using namespace service;

ServerServiceImpl::ServerServiceImpl(std::shared_ptr<rendezvous::Server> server)
    : server(server) {
}
grpc::Status ServerServiceImpl::RegisterRequest(grpc::ServerContext* context, const rendezvous_server::RegisterRequestMessage* request, rendezvous_server::Empty* response) {
  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  std::string rid = request->rid();

  metadata::Request * rdv_request;

  spdlog::trace("[REPLICA] > registering request '{}'", rid.c_str());

  server->getOrRegisterRequest(rid);

  spdlog::trace("[REPLICA] < registered request '{}'", rid.c_str());
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::RegisterBranch(grpc::ServerContext* context, 
  const rendezvous_server::RegisterBranchMessage* request, 
  rendezvous_server::Empty* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;

  std::string rid = request->rid();
  const std::string& bid = request->bid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  const std::string& region = request->region();
  const std::string& replica_id = request->context().replica_id();
  const int& request_version = request->context().request_version();
  metadata::Request * rdv_request;

  spdlog::trace("[REPLICA] > registering branch for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());

  rdv_request = server->getOrRegisterRequest(rid);
  std::string res = server->registerBranch(rdv_request, service, region, tag, bid);

  // sanity check - must never happen
  if (res.empty()) {
    return grpc::Status(grpc::StatusCode::ABORTED, utils::ERR_MSG_BRANCH_ALREADY_EXISTS);
  }

  spdlog::trace("[REPLICA] < registered branch '{}' for request '{}' on service='{}' and region='{}'", bid.c_str(), rdv_request->getRid().c_str(), service.c_str(), region.c_str());

  rdv_request->getVersionsRegistry()->updateRemoteVersion(replica_id, request_version);
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::RegisterBranches(grpc::ServerContext* context, 
  const rendezvous_server::RegisterBranchesMessage* request, 
  rendezvous_server::Empty* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& bid = request->bid();
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  const std::string& replica_id = request->context().replica_id();
  const int& request_version = request->context().request_version();
  metadata::Request * rdv_request;

  const auto& regions = request->regions();
  int num = request->regions().size();

  spdlog::trace("[REPLICA] Registering {} branches for request '{}' on service '{}'", num, rid, service);

  rdv_request = server->getOrRegisterRequest(rid);
  std::string res = server->registerBranches(rdv_request, service, regions, tag, bid);

  // sanity check - must never happen
  if (res.empty()) {
    return grpc::Status(grpc::StatusCode::ABORTED, utils::ERR_MSG_BRANCH_ALREADY_EXISTS);
  }

  spdlog::trace("[REPLICA] Registered {} branches '{}' for request '{}' on service '{}'", num, bid, rid, service);

  rdv_request->getVersionsRegistry()->updateRemoteVersion(replica_id, request_version);

  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous_server::CloseBranchMessage* request, 
  rendezvous_server::Empty* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& region = request->region();
  const std::string& bid = request->bid();

  const std::string& rid = server->parseRid(bid);
  if (rid.empty()) {
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_RID);
  }

  spdlog::trace("[REPLICA] > closing branch '{} 'for request '{}' on region={}", bid.c_str(), rid.c_str(), region.c_str());

  metadata::Request * rdv_request = server->getOrRegisterRequest(rid);

  int res = server->closeBranch(rdv_request, bid, region);

  if (res == 0) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  }
  else if (res == -1) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }
  
  spdlog::trace("[REPLICA] < closed branch '{}' for request '{}' on region={}", bid.c_str(), rid.c_str(), region.c_str());
  return grpc::Status::OK;
}