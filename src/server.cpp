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

void Server::initCleanRequests() {
  std::thread([this]() {

    while (true) {
      std::this_thread::sleep_for(std::chrono::hours(12));

      std::cout << "[INFO] initializing clean requests procedure..." << std::endl;

      int num = 0;
      auto now = std::chrono::system_clock::now();

      std::vector<metadata::Request*> oldRequests;

      mutex_requests.lock();
      // collect old requests
      for (const auto & request_it : *requests) {
        if (request_it.second->canRemove(now)) {
          oldRequests.emplace_back(request_it.second);
        }
      }
      // remove old requests from the requests map
      for (const auto & request : oldRequests) {
        requests->erase(request->getRid());
      }
      mutex_requests.unlock();

      // log request and delete it
      if (LOG_REQUESTS && oldRequests.size() > 0) {
        json j;
        for (const auto& request : oldRequests) {
          j["requests"].push_back(request->toJson());
          num++;
        }

        // compute filename based on the current timestamp
        auto now = std::chrono::system_clock::now();
        std::time_t now_ts = std::chrono::system_clock::to_time_t(now);
        std::tm * now_tm = std::localtime(&now_ts);
        char now_buffer[20];
        std::strftime(now_buffer, 20, utils::TIME_FORMAT.c_str(), now_tm);

        std::string filename = now_buffer;
        std::replace(filename.begin(), filename.end(), ' ', '-');
        std::replace(filename.begin(), filename.end(), ':', '-');
        std::ofstream outfile("../../../logs/" + filename);

        if (outfile) {
          outfile << j.dump(4);
          outfile.close();
          std::cout << "[INFO] successfully save logs to json file " << filename << "!" << std::endl;
        }
        else {
          std::cout << "[ERROR] could not save logs to json file " << filename << ": " << std::strerror(errno) << std::endl;
        }
      }

      std::cout << "[INFO] successfully cleaned " << num << " requests!" << std::endl;
    }
    
  }).detach();
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
  int res = request->registerBranch(bid, service, region);

  if (res == -1) {
    return "";
  }

  return bid;
}

std::string Server::registerBranches(metadata::Request * request, const std::string& service, const utils::ProtoVec& regions, std::string bid) {
  if (bid.empty()) {
    bid = genBid(request);
  }
  int res = request->registerBranches(bid, service, regions);

  if (res == -1) {
    return "";
  }

  return bid;
}

bool Server::closeBranch(metadata::Request * request, const std::string& bid, const std::string& region) {
  return request->closeBranch(bid, region);
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