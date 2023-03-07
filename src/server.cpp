#include "server.h"

using namespace server;

Server::Server(std::string sid)
    : nextRid(0), inconsistencies(0), sid(sid) {
    
    requests = new std::unordered_map<std::string, metadata::Request*>;
}

Server::~Server() {
  auto last = requests->end();
  for (auto pair = requests->begin(); pair != last; pair++) {
    metadata::Request * request = pair->second;
    delete request;
  }

  delete requests;
}

metadata::Request * Server::getOrRegisterRequest(std::string rid) {
  metadata::Request * request = nullptr;
  if (rid.empty()) {
    request = registerRequest(""); // register new request with auto generated id
  }
  else {
    request = getRequest(rid);
    if (request == nullptr) {
      request = registerRequest(rid); // register new request with provided id
    }
  }
  return request;
}

metadata::Request * Server::getRequest(const std::string& rid) {

  mutex_requests.lock();
  auto pair = requests->find(rid);

  if (pair == requests->end()) {
    mutex_requests.unlock();
    return nullptr;
  }

  metadata::Request * request = pair->second;
  mutex_requests.unlock();

  return request;
}

metadata::Request * Server::registerRequest(std::string rid) {
  if (!rid.empty() && getRequest(rid) != nullptr) {
    return nullptr; // request already exists
  }

  if (rid.empty()) {
    rid = "rendezvous-" + std::to_string(nextRid.fetch_add(1));
  }
  
  mutex_requests.lock();

  // save request
  metadata::Request * request = new metadata::Request(rid);
  requests->insert({rid, request});

  mutex_requests.unlock();

  return request;
}

std::string Server::registerBranch(metadata::Request * request, const std::string& service, const std::string& region) {
  long id = request->computeNextId();
  std::string bid(sid + ":" + std::to_string(id));
  metadata::Branch * branch = new metadata::Branch(bid, service, region);
  request->addBranch(branch);
  return bid;
}

std::string Server::registerBranches(metadata::Request * request, const std::string& service, const google::protobuf::RepeatedPtrField<std::string>& regions) {
  metadata::Branch * branch;
  long id = request->computeNextId();
  std::string bid(sid + ":" + std::to_string(id));

  for (const auto& region : regions) {
    branch = new metadata::Branch(bid, service, region);
    request->addBranch(branch);
  }
  return bid;
}

int Server::closeBranch(const std::string& rid, const std::string& service, const std::string& region, const std::string& bid) {
  metadata::Request * request = getRequest(rid);
  if (request == nullptr) {
    return -1;
  }
  return request->closeBranch(service, region, bid);
}

int Server::waitRequest(const std::string& rid, const std::string& service, const std::string& region) {
  metadata::Request * request = getRequest(rid);
  if (request == nullptr) {
    return -1;
  }

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
    int num = inconsistencies.fetch_add(1) + 1;
    std::cout << "[INFO] prevented inconsistencies = " << num << std::endl;
  }

  return result;
}

int Server::checkRequest(const std::string& rid, const std::string& service, const std::string& region) {
  metadata::Request * request = getRequest(rid);
  if (request == nullptr) {
    return -1;
  }

  if (!service.empty() && !region.empty())
    return request->getStatusOnServiceAndRegion(service, region);

  else if (!service.empty())
    return request->getStatusOnService(service);

  else if (!region.empty())
    return request->getStatusOnRegion(region);

  return request->getStatus();
}

std::map<std::string, int> Server::checkRequestByRegions(const std::string& rid, const std::string& service, int * status) {
  *status = 0;
  
  metadata::Request * request = getRequest(rid);
  if (request == nullptr) {
    *status = -1;
    return std::map<std::string, int>();
  }

  if (!service.empty())
    return request->getStatusByRegionsOnService(service, status);

  return request->getStatusByRegions();
}

long Server::getPreventedInconsistencies() {
  return inconsistencies.load();
}