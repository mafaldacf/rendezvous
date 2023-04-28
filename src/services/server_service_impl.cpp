#include "server_service_impl.h"

using namespace service;

RendezvousServerServiceImpl::RendezvousServerServiceImpl(std::shared_ptr<rendezvous::Server> server)
    : server(server) {
}

grpc::Status RendezvousServerServiceImpl::registerRequest(grpc::ServerContext* context, const rendezvous_server::RegisterRequestMessage* request, rendezvous_server::Empty* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  std::string rid = request->rid();

  metadata::Request * req;

  log("[REPLICA] > registering request '%s'", rid.c_str());

  server->getOrRegisterRequest(rid);

  log("[REPLICA] < registered request '%s'", rid.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServerServiceImpl::registerBranch(grpc::ServerContext* context, const rendezvous_server::RegisterBranchMessage* request, rendezvous_server::Empty* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;

  std::string rid = request->rid();
  const std::string& bid = request->bid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  const std::string& replica_id = request->context().replica_id();
  const int& request_version = request->context().request_version();
  metadata::Request * req;

  log("[REPLICA] > registering branch for request '%s' on service='%s' and region='%s'", rid.c_str(), service.c_str(), region.c_str());

  req = server->getOrRegisterRequest(rid);
  std::string res = server->registerBranch(req, service, region, bid);

  // sanity check - must never happen
  if (res.empty()) {
    return grpc::Status(grpc::StatusCode::ABORTED, utils::ERROR_MESSAGE_BRANCH_ALREADY_EXISTS);
  }

  log("[REPLICA] < registered branch '%s' for request '%s' on service='%s' and region='%s'", bid.c_str(), req->getRid().c_str(), service.c_str(), region.c_str());

  req->getVersionsRegistry()->updateRemoteVersion(replica_id, request_version);
  return grpc::Status::OK;
}

grpc::Status RendezvousServerServiceImpl::registerBranches(grpc::ServerContext* context, const rendezvous_server::RegisterBranchesMessage* request, rendezvous_server::Empty* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;

  std::string service = request->service();
  const std::string& bid = request->bid();
  const std::string& rid = request->rid();
  const std::string& replica_id = request->context().replica_id();
  const int& request_version = request->context().request_version();
  metadata::Request * req;

  // workaround of logging
  if (DEBUG) {
    std::cout << "[registerBranches] [REPLICA] > registering " << request->regions().size() << " branches for request '" << rid << "' on service '" << service << "' and regions ";
    for (const std::string& region : request->regions()) {
      std::cout << "'" << region << "' ";
    }
    std::cout << std::endl;
  }

  const auto& regions = request->regions();
  req = server->getOrRegisterRequest(rid);
  std::string res = server->registerBranches(req, service, regions, bid);

  // sanity check - must never happen
  if (res.empty()) {
    return grpc::Status(grpc::StatusCode::ABORTED, utils::ERROR_MESSAGE_BRANCH_ALREADY_EXISTS);
  }

  // workaround of logging
  if (DEBUG) {
    std::cout << "[registerBranches] [REPLICA] < registered " << request->regions().size() << " branches '" << bid << "' for request '" << req->getRid() << "' on service '" << service << "' and regions ";
    for (const std::string& region : request->regions()) {
      std::cout << "'" << region << "' ";
    }
    std::cout << std::endl;
  }

  req->getVersionsRegistry()->updateRemoteVersion(replica_id, request_version);

  return grpc::Status::OK;
}

grpc::Status RendezvousServerServiceImpl::closeBranch(grpc::ServerContext* context, const rendezvous_server::CloseBranchMessage* request, rendezvous_server::Empty* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& region = request->region();
  const std::string& bid = request->bid();

  log("[REPLICA] > closing branch '%s 'for request '%s' on region=%s", bid.c_str(), rid.c_str(), region.c_str());

  metadata::Request * req = server->getOrRegisterRequest(rid);
  bool region_found = server->closeBranch(req, bid, region);

  if (!region_found) {
    return grpc::Status(grpc::StatusCode::ABORTED, utils::ERROR_MESSAGE_REGION_NOT_FOUND);
  }

  log("[REPLICA] < closed branch '%s' for request '%s' on region=%s", bid.c_str(), rid.c_str(), region.c_str());
  return grpc::Status::OK;
}