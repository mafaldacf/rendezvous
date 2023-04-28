#include "server.h"

using namespace rendezvous;

Server::Server(std::string sid)
    : _next_rid(0), _inconsistencies(0), _sid(sid) {
    
    _requests = std::unordered_map<std::string, metadata::Request*>();
}

Server::~Server() {
  for (auto pair = _requests.begin(); pair != _requests.end(); pair++) {
    metadata::Request * request = pair->second;
    delete request;
  }
}

void Server::initCleanRequests() {
  std::thread([this]() {

    while (true) {
      break; //TODO: REMOVE
      std::this_thread::sleep_for(std::chrono::hours(12));

      std::cout << "[INFO] initializing clean requests procedure..." << std::endl;

      int num = 0;
      auto now = std::chrono::system_clock::now();

      std::vector<metadata::Request*> old_requests;

      _mutex_requests.lock();
      // collect old requests
      for (const auto & request_it : _requests) {
        if (request_it.second->canRemove(now)) {
          old_requests.emplace_back(request_it.second);
        }
      }
      // remove old requests from the requests map
      for (const auto & request : old_requests) {
        _requests.erase(request->getRid());
      }
      _mutex_requests.unlock();

      // log request and delete them
      if (LOG_REQUESTS && old_requests.size() > 0) {
        json j;
        for (const auto& request : old_requests) {
          j["requests"].push_back(request->toJson());
          num++;
          //TODO DELETE
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
  return _sid;
}

std::string Server::genRid() {
  return _sid + ':' + std::to_string(_next_rid.fetch_add(1));
}

std::string Server::genBid(metadata::Request * request) {
  return _sid + ':' + request->genId();
}

long Server::getNumInconsistencies() {
  return _inconsistencies.load();
}

metadata::Request * Server::getRequest(const std::string& rid) {
  _mutex_requests.lock();

  auto pair = _requests.find(rid);
  
  // return request if it was found
  if (pair != _requests.end()) {
      _mutex_requests.unlock();
      return pair->second;
  }

  _mutex_requests.unlock();
  return nullptr;
}

metadata::Request * Server::getOrRegisterRequest(std::string rid) {
  _mutex_requests.lock();

  // rid is not empty so we try to get the request
  if (!rid.empty()) {
    auto pair = _requests.find(rid);

    // return request if it was found
    if (pair != _requests.end()) {
      _mutex_requests.unlock();
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
  _requests.insert({rid, request});

  _mutex_requests.unlock();
  return request;
}

std::string Server::registerBranch(metadata::Request * request, const std::string& service, const std::string& region, std::string bid) {
  if (bid.empty()) {
    bid = genBid(request);
  }
  bool registered = request->registerBranch(bid, service, region);

  if (!registered) {
    return "";
  }

  return bid;
}

std::string Server::registerBranches(metadata::Request * request, const std::string& service, const utils::ProtoVec& regions, std::string bid) {
  if (bid.empty()) {
    bid = genBid(request);
  }
  bool registered = request->registerBranches(bid, service, regions);

  if (!registered) {
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
    _inconsistencies.fetch_add(1);
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

std::map<std::string, int> Server::checkRequestByRegions(metadata::Request * request, const std::string& service) {
  if (!service.empty())
    return request->getStatusByRegionsOnService(service);

  return request->getStatusByRegions();
}

long Server::getPreventedInconsistencies() {
  return _inconsistencies.load();
}