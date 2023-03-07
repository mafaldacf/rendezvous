#include "serviceImpl.h"

using namespace service;

RendezvousServiceImpl::RendezvousServiceImpl(std::string sid)
    : server(sid) {
}

grpc::Status RendezvousServiceImpl::registerRequest(grpc::ServerContext* context, const rendezvous::RegisterRequestMessage* request, rendezvous::RegisterRequestResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  std::string rid = request->rid();
  metadata::Request * req;

  log("> registering request '%s'", rid.c_str());

  req = server.registerRequest(rid);
  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, ERROR_MESSAGE_REQUEST_ALREADY_EXISTS);
  }
  
  response->set_rid(req->getRid());

  log("< registered request '%s'", req->getRid().c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::registerBranch(grpc::ServerContext* context, const rendezvous::RegisterBranchMessage* request, rendezvous::RegisterBranchResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;

  std::string rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  metadata::Request * req;

  log("> registering branch for request '%s' on service='%s' and region='%s'", rid.c_str(), service.c_str(), region.c_str());

  if (region.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_EMPTY_REGION);
  }

  req = server.getOrRegisterRequest(rid);
  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, ERROR_MESSAGE_REQUEST_ALREADY_EXISTS);
  }

  std::string bid = server.registerBranch(req, service, region);

  response->set_rid(req->getRid());
  response->set_bid(bid);

  log("< registered branch '%s' for request '%s' on service='%s' and region='%s'", bid.c_str(), req->getRid().c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::registerBranches(grpc::ServerContext* context, const rendezvous::RegisterBranchesMessage* request, rendezvous::RegisterBranchesResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;

  std::string service = request->service();
  const std::string& rid = request->rid();
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
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_EMPTY_REGION);
  }

  req = server.getOrRegisterRequest(rid);

  // this should never happen but we need to handle the error
  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, ERROR_MESSAGE_REQUEST_ALREADY_EXISTS);
  }

  response->set_rid(req->getRid());
  const auto& regions = request->regions();
  std::string bid = server.registerBranches(req, service, regions);
  response->set_bid(bid);

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
  int result;

  log("> closing branch for request '%s' on service=%s and region=%s", rid.c_str(), service.c_str(), region.c_str());

  if (region.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_EMPTY_REGION);
  }

  result = server.closeBranch(rid, service, region, bid);

  if (result == -1) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_REQUEST);
  }
  else if (result == -2) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_BRANCH_SERVICE);
  }
  else if (result == -3) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_BRANCH_REGION);
  }
  else if (result == -4) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_BRANCH);
  }

  log("< closed branch '%s' for request '%s' on service=%s and region=%s", bid.c_str(), rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::waitRequest(grpc::ServerContext* context, const rendezvous::WaitRequestMessage* request, rendezvous::WaitRequestResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();

  log("> wait request call for request '%s' on service='%s' and region='%s'", rid.c_str(), service.c_str(), region.c_str());

  int result = server.waitRequest(rid, service, region);

  if (result == -1) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MESSAGE_INVALID_REQUEST);
  }
  else if (result == -2) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_SERVICE_NOT_FOUND);
  }
  else if (result == -3) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_REGION_NOT_FOUND);
  }

  // inconsistency was prevented
  if (result == 1) {
    response->set_preventedinconsistency(true);
  }

  log("< returning wait request call for request '%s' on service='%s' and region='%s'", rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::checkRequest(grpc::ServerContext* context, const rendezvous::CheckRequestMessage* request, rendezvous::CheckRequestResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();

  int result = server.checkRequest(rid, service, region);

  if (result == -1) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MESSAGE_INVALID_REQUEST);
  }
  else if (result == -2) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_SERVICE_NOT_FOUND);
  }
  else if (result == -3) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_REGION_NOT_FOUND);
  }

  response->set_status(static_cast<rendezvous::RequestStatus>(result));

  log("checked request '%s' and got status %d on service='%s' and region='%s'", rid.c_str(), result, service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::checkRequestByRegions(grpc::ServerContext* context, const rendezvous::CheckRequestByRegionsMessage* request, rendezvous::CheckRequestByRegionsResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();

  int status = 0;
  std::map<std::string, int> result = server.checkRequestByRegions(rid, service, &status);

  if (status == -1) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MESSAGE_INVALID_REQUEST);
  }
  else if (status == -2) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MESSAGE_SERVICE_NOT_FOUND);
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

  log("checked request '%s' by regions on context (serv=%s)", rid.c_str(), service.c_str());
  return grpc::Status::OK;
}

grpc::Status RendezvousServiceImpl::getPreventedInconsistencies(grpc::ServerContext* context, const rendezvous::Empty* request, rendezvous::GetPreventedInconsistenciesResponse* response) {
  if (NO_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  long value = server.getPreventedInconsistencies();
  response->set_inconsistencies(value);

  log("num = %ld", value);
  return grpc::Status::OK;
}