#include "serviceImpl.h"
#include <iostream>
#include <memory>
#include <string>
#include "request.h"

using namespace service;

grpc::Status MonitorServiceImpl::registerRequest(grpc::ServerContext* context, const monitor::RegisterRequestMessage* request, monitor::RegisterRequestResponse* response) {
  std::string rid = request->rid();
  request::Request * req;

  req = server.registerRequest(rid); // if rid is empty server generates a new one 'rendezvous-<id>'

  rid = req->getRid();
  response->set_rid(rid);

  log("registered request '%s'", rid.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::registerBranch(grpc::ServerContext* context, const monitor::RegisterBranchMessage* request, monitor::RegisterBranchResponse* response) {
  std::string rid = request->rid();
  std::string service = request->service();
  std::string region = request->region();
  request::Request * req;

  if (rid.empty()) {
    req = server.registerRequest("");
    rid = req->getRid();
  }

  else {
    req = server.getRequest(rid);
    if (req == nullptr) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_REQUEST);
    }
  }

  long bid = server.registerBranch(req, service, region);

  response->set_rid(rid);
  response->set_bid(bid);

  log("registered branch %ld for request '%s' with context (serv=%s, reg=%s)", bid, rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::registerBranches(grpc::ServerContext* context, const monitor::RegisterBranchesMessage* request, monitor::RegisterBranchesResponse* response) {
  std::string service = request->service();
  std::string region = request->region();
  std::string rid = request->rid();
  int num = request->num();
  request::Request * req;

  if (rid.empty()) {
    req = server.registerRequest("");
    rid = req->getRid();
  }

  else {
    req = server.getRequest(rid);
    if (req == nullptr) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_REQUEST);
    }
  }

  while (num-- != 0) {
    long bid = server.registerBranch(req, service, region);

    response->add_bid(bid);
  }
  response->set_rid(rid);

  log("registered %d branches for request '%s' with context (serv=%s, reg=%s)", request->num(), rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::closeBranch(grpc::ServerContext* context, const monitor::CloseBranchMessage* request, monitor::Empty* response) {
  long bid = request->bid();
  std::string rid = request->rid();

  request::Request * req = server.getRequest(rid);

  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_REQUEST);
  }

  int result = server.closeBranch(req, bid);

  if (result == -1) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_BRANCH);
  }

  log("closed branch %ld for request '%s'", bid, rid.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::waitRequest(grpc::ServerContext* context, const monitor::WaitRequestMessage* request, monitor::Empty* response) {
  std::string rid = request->rid();
  std::string service = request->service();
  std::string region = request->region();

  log("> wait request call for request '%s' on context (serv=%s, reg=%s)", rid.c_str(), service.c_str(), region.c_str());

  request::Request * req = server.getRequest(rid);

  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_REQUEST);
  }

  int result = server.waitRequest(req, service, region);

  if (result < 0) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MESSAGE_CONTEXT_NOT_FOUND);
  }

  log("< returning wait request call for request '%s' on context (serv=%s, reg=%s)", rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::checkRequest(grpc::ServerContext* context, const monitor::CheckRequestMessage* request, monitor::CheckRequestResponse* response) {
  std::string rid = request->rid();
  std::string service = request->service();
  std::string region = request->region();

  request::Request * req = server.getRequest(rid);

  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_REQUEST);
  }

  int result = server.checkRequest(req, service, region);

  if (result < 0) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MESSAGE_CONTEXT_NOT_FOUND);
  }

  response->set_status(static_cast<monitor::RequestStatus>(result));

  log("checked request '%s' and got status %d on context (serv=%s, reg=%s)", rid.c_str(), result, service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::checkRequestByRegions(grpc::ServerContext* context, const monitor::CheckRequestByRegionsMessage* request, monitor::CheckRequestByRegionsResponse* response) {
  std::string rid = request->rid();
  std::string service = request->service();

  request::Request * req = server.getRequest(rid);

  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ERROR_MESSAGE_INVALID_REQUEST);
  }

  int status = 0;
  std::map<std::string, int> result = server.checkRequestByRegions(req, service, &status);

  if (status == -1) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MESSAGE_SERVICE_NOT_FOUND);
  }

  if (status == -2) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MESSAGE_REGIONS_NOT_FOUND);
  }

  auto last = result.end();

  for (auto pair = result.begin(); pair != last; pair++) {
      std::string region = pair->first;
      int status = pair->second;

      monitor::RegionStatus regionStatus;
      regionStatus.set_region(region);
      regionStatus.set_status(static_cast<monitor::RequestStatus>(status));

      response->add_regionstatus()->CopyFrom(regionStatus);
    }

  log("checked request '%s' by regions on context (serv=%s)", rid.c_str(), service.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::getPreventedInconsistencies(grpc::ServerContext* context, const monitor::Empty* request, monitor::GetPreventedInconsistenciesResponse* response) {
  long value = server.getPreventedInconsistencies();
  response->set_inconsistencies(value);

  log("get number of prevented inconsistencies: %ld", value);
  return grpc::Status::OK;
}