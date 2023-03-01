#include "server.h"

using namespace server;

Server::Server()
    : nextRID(0), inconsistencies(0) {
    
    requests = new std::map<std::string, request::Request*>;
}

Server::~Server() {
  auto last = requests->end();
  for (auto pair = requests->begin(); pair != last; pair++) {
    request::Request * request = pair->second;
    delete request;
  }

  delete requests;
}

request::Request * Server::getRequest(std::string rid) {

  mutex_requests.lock();
  auto pair = requests->find(rid);

  if (pair == requests->end()) {
    mutex_requests.unlock();
    return nullptr;
  }

  request::Request * request = pair->second;
  mutex_requests.unlock();

  return request;
}

request::Request * Server::registerRequest(std::string rid) {

  if (rid.empty()) {
    rid = "rendezvous-" + std::to_string(nextRID.fetch_add(1));
  }
  
  mutex_requests.lock();

  // save request
  request::Request * request = new request::Request(rid);
  requests->insert({rid, request});

  mutex_requests.unlock();

  return request;
}

long Server::registerBranch(request::Request * request, std::string service, std::string region) {
  
  long bid = request->computeNextBID();
  branch::Branch * branch = new branch::Branch(bid, service, region);
  request->addBranch(branch);

  return bid;
}

int Server::closeBranch(request::Request * request, long bid) {
  return request->removeBranch(bid);
}

int Server::waitRequest(request::Request * request, std::string service, std::string region) {
  int result;

  if (!service.empty() && !region.empty())
    result = request->waitOnServiceAndRegion(service, region);

  else if (!service.empty())
    result = request->waitOnService(service);

  else if (!region.empty())
    result = request->waitOnRegion(region);

  else
    result = request->wait();

  if (result > 0)
    inconsistencies.fetch_add(1);

  return result;
}

int Server::checkRequest(request::Request * request, std::string service, std::string region) {

  if (!service.empty() && !region.empty())
    return request->getStatusOnServiceAndRegion(service, region);

  else if (!service.empty())
    return request->getStatusOnService(service);

  else if (!region.empty())
    return request->getStatusOnRegion(region);

  return request->getStatus();
}

std::map<std::string, int> Server::checkRequestByRegions(request::Request * request, std::string service, int * status) {
  if (!service.empty())
    return request->getStatusByRegionsOnService(service, status);

  return request->getStatusByRegions(status);
}

long Server::getPreventedInconsistencies() {
  return inconsistencies.load();
}