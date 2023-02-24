#include "service.h"
#include <iostream>
#include <memory>
#include <string>
#include "utils.h"
#include "request.h"

using namespace service;
using namespace utils;

request::Request * MonitorServiceImpl::getRequest(long rid) {

  mutex_requests.lock();
  auto pair = requests.find(rid);

  if (pair == requests.end()) {
    return nullptr;
  }

  request::Request * request = pair->second;
  mutex_requests.unlock();

  return request;
}

grpc::Status MonitorServiceImpl::registerRequest(grpc::ServerContext* context, const monitor::Empty* request, monitor::RegisterRequestResponse* response) {
  long rid = nextRID.fetch_add(1);
  
  mutex_requests.lock();

  request::Request * req = new request::Request(rid);
  requests[rid] = req;

  mutex_requests.unlock();

  response->set_rid(rid);

  log("registered request %ld", rid);
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::registerBranch(grpc::ServerContext* context, const monitor::RegisterBranchMessage* request, monitor::RegisterBranchResponse* response) {
  long rid = request->rid();
  std::string service = request->service();
  std::string region = request->region();

  request::Request * req = getRequest(rid);

  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MSG_REQUEST_DOES_NOT_EXIST);
  }

  //TODO: check if rid was not provided -> create a new rid

  long bid = req->computeNextBID();
  branch::Branch * branch = new branch::Branch(bid, service, region);
  req->addBranch(branch);

  response->set_bid(bid);

  log("registered branch %ld for request %ld with context (serv=%s, reg=%s)", bid, rid, service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::registerBranches(grpc::ServerContext* context, const monitor::RegisterBranchesMessage* request, monitor::RegisterBranchesResponse* response) {
  std::string service = request->service();
  std::string region = request->region();
  long rid = request->rid();
  int num = request->num();

  request::Request * req = getRequest(rid);

  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MSG_REQUEST_DOES_NOT_EXIST);
  }

  //TODO: check if rid was not provided -> create a new rid

  while (num-- != 0) {
    long bid = req->computeNextBID();
    branch::Branch * branch = new branch::Branch(bid, service, region);
    req->addBranch(branch);

    response->add_bid(bid);
  }

  log("registered %d branches for request %ld with context (serv=%s, reg=%s)", request->num(), rid, service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::closeBranch(grpc::ServerContext* context, const monitor::CloseBranchMessage* request, monitor::Empty* response) {
  long bid = request->bid();
  long rid = request->rid();

  request::Request * req = getRequest(rid);

  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MSG_REQUEST_DOES_NOT_EXIST);
  }

  int result = req->removeBranch(bid);

  if (result == -1) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MSG_BRANCH_DOES_NOT_EXIST);
  }

  log("closed branch %ld for request %ld", bid, rid);
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::waitRequest(grpc::ServerContext* context, const monitor::WaitRequestMessage* request, monitor::Empty* response) {
  long rid = request->rid();
  std::string service = request->service();
  std::string region = request->region();

  log("> wait request call for request %ld", rid);

  request::Request * req = getRequest(rid);

  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MSG_REQUEST_DOES_NOT_EXIST);
  }

  req->waitRequest(service, region);

  log("< returning wait request call for request %ld on context (serv=%s, reg=%s)", rid, service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::checkRequest(grpc::ServerContext* context, const monitor::CheckRequestMessage* request, monitor::CheckRequestResponse* response) {
  long rid = request->rid();
  std::string service = request->service();
  std::string region = request->region();

  request::Request * req = getRequest(rid);

  if (request == nullptr) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MSG_REQUEST_DOES_NOT_EXIST);
  }

  int status = req->checkRequest(service, region);

  response->set_status(static_cast<monitor::RequestStatus>(status));

  log("checked request %ld and got status %d on context (serv=%s, reg=%s)", rid, status, service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status MonitorServiceImpl::checkRequestByRegions(grpc::ServerContext* context, const monitor::CheckRequestByRegionsMessage* request, monitor::CheckRequestByRegionsResponse* response) {
  long rid = request->rid();
  std::string service = request->service();

  request::Request * req = getRequest(rid);

  if (req == nullptr) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, ERROR_MSG_REQUEST_DOES_NOT_EXIST);
  }

  std::map<std::string, int> allStatus = req->checkRequestByRegions(service);
  auto last = allStatus.end();

  for (auto pair = allStatus.begin(); pair != last; pair++) {
      std::string region = pair->first;
      int status = pair->second;

      monitor::RegionStatus regionStatus;
      regionStatus.set_region(region);
      regionStatus.set_status(static_cast<monitor::RequestStatus>(status));

      response->add_regionstatus()->CopyFrom(regionStatus);
    }

  log("checked request %ld by regions on context (serv=%s)", rid, service.c_str());
  return grpc::Status::OK;
}