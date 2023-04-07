#include "client_service_impl.h"

using namespace service;

RendezvousServiceImpl::RendezvousServiceImpl(std::shared_ptr<rendezvous::Server> server, std::vector<std::string> addrs)
    : server(server), waitRequestCalls(0), replicaClient(addrs) {
}

grpc::Status RendezvousServiceImpl::registerRequest(grpc::ServerContext* context, const rendezvous::RegisterRequestMessage* request, rendezvous::RegisterRequestResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  std::string rid = request->rid();
  metadata::Request * req;

  log("> registering request '%s'", rid.c_str());

  req = server->getOrRegisterRequest(rid);
  response->set_rid(req->getRid());

  // initialize empty metadata
  rendezvous::RequestContext ctx;
  response->mutable_context()->CopyFrom(ctx);


  replicaClient.sendRegisterRequest(req->getRid());

  log("< registered request '%s'", req->getRid().c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::registerBranch(grpc::ServerContext* context, const rendezvous::RegisterBranchMessage* request, rendezvous::RegisterBranchResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;

  std::string rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  rendezvous::RequestContext ctx = request->context();
  metadata::Request * req;

  log("> registering branch for request '%s' on service='%s' and region='%s'", rid.c_str(), service.c_str(), region.c_str());

  if (region.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERROR_MESSAGE_EMPTY_REGION);
  }

  req = server->getOrRegisterRequest(rid);
  std::string bid = server->registerBranch(req, service, region);
  
  std::string sid = server->getSid();
  int version = req->getVersionsRegistry()->updateLocalVersion(sid);
  ctx.mutable_versions()->insert({sid, version});

  response->set_rid(req->getRid());
  response->set_bid(bid);
  response->mutable_context()->CopyFrom(ctx);


  replicaClient.sendRegisterBranch(req->getRid(), bid, service, region, sid, version);

  log("< registered branch '%s' for request '%s' on service='%s' and region='%s'", bid.c_str(), req->getRid().c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::registerBranches(grpc::ServerContext* context, const rendezvous::RegisterBranchesMessage* request, rendezvous::RegisterBranchesResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  std::string service = request->service();
  const std::string& rid = request->rid();
  rendezvous::RequestContext ctx = request->context();
  metadata::Request * req;

  int num = request->regions().size();

  // workaround of logging
  if (DEBUG) {
    std::cout << "[registerBranches] > registering " << num << " branches for request '" << rid << "' on service '" << service << "' and regions ";
    for (const std::string& region : request->regions()) {
      std::cout << "'" << region << "' ";
    }
    std::cout << std::endl;
  }

  if (num == 0) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERROR_MESSAGE_EMPTY_REGION);
  }

  const auto& regions = request->regions();
  req = server->getOrRegisterRequest(rid);
  std::string bid = server->registerBranches(req, service, regions);

  std::string sid = server->getSid();
  int version = req->getVersionsRegistry()->updateLocalVersion(sid);
  ctx.mutable_versions()->insert({sid, version});
  
  response->set_rid(req->getRid());
  response->set_bid(bid);
  response->mutable_context()->CopyFrom(ctx);


  replicaClient.sendRegisterBranches(req->getRid(), bid, service, regions, sid, version);

  // workaround of logging
  if (DEBUG) {
    std::cout << "[registerBranches] < registered " << num << " branches '" << bid << "' for request '" << req->getRid() << "' on service '" << service << "' and regions ";
    for (const std::string& region : request->regions()) {
      std::cout << "'" << region << "' ";
    }
    std::cout << std::endl;
  }

  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::closeBranch(grpc::ServerContext* context, const rendezvous::CloseBranchMessage* request, rendezvous::Empty* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  std::string bid = request->bid();

  log("> closing branch for request '%s' on service=%s and region=%s", rid.c_str(), service.c_str(), region.c_str());

  if (region.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERROR_MESSAGE_EMPTY_REGION);
  }

  metadata::Request * req = server->getOrRegisterRequest(rid);
  server->closeBranch(req, service, region, bid);

  replicaClient.sendCloseBranch(rid, bid, service, region);
  
  log("< closed branch '%s' for request '%s' on service=%s and region=%s", bid.c_str(), rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::waitRequest(grpc::ServerContext* context, const rendezvous::WaitRequestMessage* request, rendezvous::WaitRequestResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  rendezvous::RequestContext ctx = request->context();

  log("> wait request call for request '%s' on service='%s' and region='%s'", rid.c_str(), service.c_str(), region.c_str());

  metadata::Request * req = server->getOrRegisterRequest(rid);
  req->getVersionsRegistry()->waitRemoteVersions(ctx);

  int result = server->waitRequest(req, service, region);

  if (result == -2) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERROR_MESSAGE_SERVICE_NOT_FOUND);
  }
  else if (result == -3) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERROR_MESSAGE_REGION_NOT_FOUND);
  }

  // inconsistency was prevented
  if (result == 1) {
    response->set_preventedinconsistency(true);
    if (DEBUG) {
      std::cout << "[INFO] prevented inconsistencies = " << server->getNumInconsistencies() << std::endl;
    }
  }

  if (DEBUG) {
    int n = waitRequestCalls.fetch_add(1) + 1;
    std::cout << "[INFO] wait request calls = " << n << std::endl;
  }

  log("< returning wait request call for request '%s' on service='%s' and region='%s'", rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::checkRequest(grpc::ServerContext* context, const rendezvous::CheckRequestMessage* request, rendezvous::CheckRequestResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  rendezvous::RequestContext ctx = request->context();

  log("> check request call for request '%s' on service='%s' and region='%s'", rid.c_str(), service.c_str(), region.c_str());

  metadata::Request * req = server->getOrRegisterRequest(rid);
  req->getVersionsRegistry()->waitRemoteVersions(ctx);

  int result = server->checkRequest(req, service, region);

  if (result == -2) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERROR_MESSAGE_SERVICE_NOT_FOUND);
  }
  else if (result == -3) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERROR_MESSAGE_REGION_NOT_FOUND);
  }

  response->set_status(static_cast<rendezvous::RequestStatus>(result));

  log("< returning check request call for request '%s' on service='%s' and region='%s'", rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::checkRequestByRegions(grpc::ServerContext* context, const rendezvous::CheckRequestByRegionsMessage* request, rendezvous::CheckRequestByRegionsResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  rendezvous::RequestContext ctx = request->context();

  log("> check request by regions call for request '%s' on service='%s'", rid.c_str(), service.c_str());

  metadata::Request * req = server->getOrRegisterRequest(rid);
  req->getVersionsRegistry()->waitRemoteVersions(ctx);

  int status = 0;
  std::map<std::string, int> result = server->checkRequestByRegions(req, service, &status);

  if (status == -2) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERROR_MESSAGE_SERVICE_NOT_FOUND);
  }

  auto last = result.end();

  for (auto pair = result.begin(); pair != last; pair++) {
      std::string region = pair->first;
      int status = pair->second;

      rendezvous::RegionStatus regionStatus;
      regionStatus.set_region(region);
      regionStatus.set_status(static_cast<rendezvous::RequestStatus>(status));

      response->add_regionstatus()->CopyFrom(regionStatus);
    }

  log("< returning check request by regions call for request '%s' on service='%s'", rid.c_str(), service.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::getPreventedInconsistencies(grpc::ServerContext* context, const rendezvous::Empty* request, rendezvous::GetPreventedInconsistenciesResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  long value = server->getPreventedInconsistencies();
  response->set_inconsistencies(value);

  log("num = %ld", value);
  return grpc::Status::OK;
}