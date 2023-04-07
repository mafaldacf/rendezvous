#include "server.h"

using namespace rendezvous;

Server::Server(std::string sid)
    : nextRid(0), inconsistencies(0), sid(sid) {
    
    requests = new std::unordered_map<std::string, metadata::Request*>();
}

Server::~Server() {
  for (auto pair = requests->begin(); pair != requests->end(); pair++) {
    metadata::Request * request = pair->second;
    delete request;
  }
  delete requests;
}

std::string Server::getSid() {
  return sid;
}

std::string Server::genRid() {
  return sid + ':' + std::to_string(nextRid.fetch_add(1));
}

std::string Server::genBid(metadata::Request * request) {
  return sid + ':' + request->genId();
}

long Server::getNumInconsistencies() {
  return inconsistencies.load();
}

metadata::Request * Server::getRequest(const std::string& rid) {
  mutex_requests.lock();

  auto pair = requests->find(rid);
  
  // return request if it was found
  if (pair != requests->end()) {
      mutex_requests.unlock();
      return pair->second;
  }

  mutex_requests.unlock();
  return nullptr;
}

metadata::Request * Server::getOrRegisterRequest(std::string rid) {
  mutex_requests.lock();

  // rid is not empty so we try to get the request
  if (!rid.empty()) {
    auto pair = requests->find(rid);

    // return request if it was found
    if (pair != requests->end()) {
      mutex_requests.unlock();
      return pair->second;
    }
  }

  // generate new rid
  if (rid.empty()) {
    rid = genRid();
  }

  // register request 
  replicas::VersionRegistry * versionsRegistry = new replicas::VersionRegistry();
  metadata::Request * request = new metadata::Request(rid, versionsRegistry);
  requests->insert({rid, request});

  mutex_requests.unlock();
  return request;
}

std::string Server::registerBranch(metadata::Request * request, const std::string& service, const std::string& region, std::string bid) {
  if (bid.empty()) {
    bid = genBid(request);
  }
  request->registerBranch(bid, service, region);
  return bid;
}

std::string Server::registerBranches(metadata::Request * request, const std::string& service, const utils::ProtoVec& regions, std::string bid) {
  if (bid.empty()) {
    bid = genBid(request);
  }
  request->registerBranches(bid, service, regions);
  return bid;
}

void Server::closeBranch(metadata::Request * request, const std::string& service, const std::string& region, const std::string& bid) {
  bool found = request->closeBranch(service, region, bid);

  if (DEBUG && !found) {
    std::cout << "[INFO] no branch was found and a new closed one was created" << std::endl;
  }
}

int Server::waitRequest(metadata::Request * request, const std::string& service, const std::string& region) {
  int result;

  if (!service.empty() && !region.empty())
    result = request->waitOnServiceAndRegion(service, region);

  else if (!service.empty())
    result = request->waitOnService(service);

  else if (!region.empty())
    result = request->waitOnRegion(region);

  else
    result = request->wait();

  if (result == 1) {
    inconsistencies.fetch_add(1);
  }

  return result;
}

int Server::checkRequest(metadata::Request * request, const std::string& service, const std::string& region) {
  if (!service.empty() && !region.empty())
    return request->getStatusOnServiceAndRegion(service, region);

  else if (!service.empty())
    return request->getStatusOnService(service);

  else if (!region.empty())
    return request->getStatusOnRegion(region);

  return request->getStatus();
}

std::map<std::string, int> Server::checkRequestByRegions(metadata::Request * request, const std::string& service, int * status) {
  *status = 0;

  if (!service.empty())
    return request->getStatusByRegionsOnService(service, status);

  return request->getStatusByRegions();
}

long Server::getPreventedInconsistencies() {
  return inconsistencies.load();
}