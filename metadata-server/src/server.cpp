#include "server.h"

using namespace rendezvous;

Server::Server(std::string sid, json settings)
    : _next_rid(0), _sid(sid), 
    _cleanup_requests_interval_m(settings["cleanup_requests_interval_m"].get<int>()),
    _cleanup_requests_validity_m(settings["cleanup_requests_validity_m"].get<int>()),
    _cleanup_subscribers_interval_m(settings["cleanup_subscribers_interval_m"].get<int>()),
    _cleanup_subscribers_validity_m(settings["cleanup_subscribers_validity_m"].get<int>()),
    _subscribers_refresh_interval_s(settings["subscribers_refresh_interval_s"].get<int>()),
    _wait_replica_timeout_s(settings["wait_replica_timeout_s"].get<int>()) {
    
    spdlog::info("------------------------------------------------------");
    spdlog::info("- Garbage Collector Info (minutes):");
    spdlog::info("\t - Requests interval: {}", _cleanup_requests_interval_m);
    spdlog::info("\t - Requests validity: {}", _cleanup_requests_validity_m);
    spdlog::info("\t - Subscribers interval: {}", _cleanup_subscribers_interval_m);
    spdlog::info("\t - Subscribers validity: {}", _cleanup_subscribers_validity_m);
    spdlog::info("- Subscribers max wait time: {} seconds", _subscribers_refresh_interval_s);
    spdlog::info("- Wait replica timeout: {} seconds", _wait_replica_timeout_s);
    spdlog::info("------------------------------------------------------");
    
    _requests = std::unordered_map<std::string, metadata::Request*>();
    _subscribers = std::unordered_map<std::string, std::unordered_map<std::string, metadata::Subscriber*>>();
}

// testing purposes
Server::Server(std::string sid)
    : _next_rid(0), _sid(sid), 
    _cleanup_requests_interval_m(30),
    _cleanup_requests_validity_m(30),
    _cleanup_subscribers_interval_m(30),
    _cleanup_subscribers_validity_m(30),
    _subscribers_refresh_interval_s(60),
    _wait_replica_timeout_s(60) {
    
    _requests = std::unordered_map<std::string, metadata::Request*>();
    _subscribers = std::unordered_map<std::string, std::unordered_map<std::string, metadata::Subscriber*>>();
}


Server::~Server() {
  for (auto pair = _requests.begin(); pair != _requests.end(); pair++) {
    metadata::Request * request = pair->second;
    delete request;
  }
  for (auto regions_it = _subscribers.begin(); regions_it != _subscribers.end(); regions_it++) {
    for (auto subscribers_it = regions_it->second.begin(); subscribers_it != regions_it->second.end(); subscribers_it++) {
      metadata::Subscriber * subscriber = subscribers_it->second;
      delete subscriber;
    }
  }
}

// -----------------
// Publish-Subscribe
//------------------

metadata::Subscriber * Server::getSubscriber(const std::string& service, const std::string& tag, const std::string& region) {
  const std::string& subscriber_id = computeSubscriberId(service, tag);

  std::shared_lock<std::shared_mutex> read_lock(_mutex_subscribers);
  auto it_regions = _subscribers.find(subscriber_id);

  // found subscriber in the region
  if (it_regions != _subscribers.end()) {
    auto it_subscriber = it_regions->second.find(region);
    if (it_subscriber != it_regions->second.end()) {
      return it_subscriber->second;
    }
  }

  // manually upgrade lock
  read_lock.unlock();
  std::shared_lock<std::shared_mutex> write_lock(_mutex_subscribers);

  // register new subscriber
  metadata::Subscriber * subscriber = new metadata::Subscriber(_subscribers_refresh_interval_s);
  _subscribers[subscriber_id][region] = subscriber;
  return subscriber;
}

void Server::publishBranches(const std::string& service, const std::string& tag, const std::string& bid) {
  metadata::Subscriber * subscriber;
  const std::string& subscriber_id = computeSubscriberId(service, tag);
  
  std::shared_lock<std::shared_mutex> read_lock(_mutex_subscribers);
  auto it_regions = _subscribers.find(subscriber_id);

  // found subscriber in certain regions
  if (it_regions != _subscribers.end()) {
    for (const auto& subscriber : it_regions->second) {
      spdlog::debug("tracking branch '{}' for subscriber id '{}' in region '{}'", bid.c_str(), subscriber_id.c_str(), it_regions->first);
      subscriber.second->pushBranch(bid);
    }
  }
}

// ------------------
// Garbage Collectors
//-------------------

void Server::initSubscribersCleanup() {
  if (_cleanup_subscribers_interval_m == -1 || _cleanup_subscribers_validity_m == -1) {
    return;
  }
  std::thread([this]() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::minutes(_cleanup_subscribers_interval_m));
      auto now = std::chrono::system_clock::now();
      std::unique_lock<std::shared_mutex> write_lock(_mutex_subscribers);
      spdlog::info("[GC SUBSCRIBERS] initializing garbage collector...");

      for (auto subscribers_it = _subscribers.begin(); subscribers_it != _subscribers.cend(); /* no increment */) {
        for (auto regions_it = subscribers_it->second.cbegin(); regions_it != subscribers_it->second.cend(); /* no increment */) {
          if (now - regions_it->second->getLastTs() > std::chrono::minutes(_cleanup_subscribers_validity_m)) {
            delete regions_it->second;
            subscribers_it->second.erase(regions_it++);
          }
          else {
            ++regions_it;
          }
        }
        // current subscriber (id) is not associated with any region so we delete the id from the main subscriber map
        if (subscribers_it->second.empty()) {
          _subscribers.erase(subscribers_it++);
        }
        else {
          ++subscribers_it;
        }
      }
    }
  }).detach();

}

void Server::initRequestsCleanup() {
  if (_cleanup_requests_interval_m == -1 || _cleanup_requests_validity_m == -1) {
    return;
  }
  std::thread([this]() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::minutes(_cleanup_requests_interval_m));
      auto now = std::chrono::system_clock::now();
      std::unique_lock<std::shared_mutex> write_lock(_mutex_requests);
      std::size_t initial_size = _requests.size();
      spdlog::info("[GC REQUESTS] initializing garbage collector for {} requests...", initial_size);

      for (auto it = _requests.cbegin(); it != _requests.cend(); /* no increment */) {
        if (now - it->second->getLastTs() > std::chrono::minutes(_cleanup_requests_validity_m)) {
          delete it->second;
          _requests.erase(it++);
        }
        else {
          ++it;
        }
      }
      spdlog::info("[GC REQUESTS] done! collected {} requests", initial_size - _requests.size());
    }
    
  }).detach();
}

// -----------
// Identifiers
//------------

std::string Server::getSid() {
  return _sid;
}

std::string Server::genRid() {
  return _sid + '_' + std::to_string(_next_rid.fetch_add(1));
}

std::string Server::genBid(metadata::Request * request) {
  return _sid + '_' + request->genId();
}

std::string Server::getFullBid(metadata::Request * request, const std::string& bid) {
  return bid + ':' + request->getRid();
}

std::pair<std::string, std::string> Server::parseFullBid(const std::string& full_bid) {
  size_t delimiter_pos = full_bid.find(':');
  std::string rid = "";
  std::string bid = "";

  // format of full bid: <bid>:<rid>
  if (delimiter_pos != std::string::npos) {
    rid = full_bid.substr(delimiter_pos+1);
    bid = full_bid.substr(0, delimiter_pos);
  }
  return std::make_pair(bid, rid);
}

std::string Server::computeSubscriberId(const std::string& service, const std::string& tag) {
  return service + ":" + tag;
}

// -----------
// Helpers
//------------

metadata::Request * Server::getRequest(const std::string& rid) {
  std::shared_lock<std::shared_mutex> lock(_mutex_requests); 
  auto pair = _requests.find(rid);
  // return request if it was found
  if (pair != _requests.end()) {
      return pair->second;
  }
  return nullptr;
}

metadata::Request * Server::getOrRegisterRequest(std::string rid) {
  std::shared_lock<std::shared_mutex> read_lock(_mutex_requests); 

  // rid is not empty so we try to get the request
  if (!rid.empty()) {
    auto pair = _requests.find(rid);
    // return request if it was found
    if (pair != _requests.end()) {
      return pair->second;
    }
  }
  read_lock.unlock();

  // generate new rid
  if (rid.empty()) {
    rid = genRid();
  }

  // register request 
  std::unique_lock<std::shared_mutex> write_lock(_mutex_requests);
  replicas::VersionRegistry * versionsRegistry = new replicas::VersionRegistry(_wait_replica_timeout_s);
  metadata::Request * request = new metadata::Request(rid, versionsRegistry);
  _requests.insert({rid, request});
  return request;
}

// ---------------------
// Main Rendezvous Logic
//----------------------

std::string Server::registerBranch(metadata::Request * request, const std::string& service, const std::string& region, const std::string& tag, std::string bid) {
  if (bid.empty()) {
    bid = genBid(request);
  }
  metadata::Branch * branch = request->registerBranch(bid, service, tag, region);
  if (!branch) {
    return "";
  }
  const std::string& full_bid = getFullBid(request, bid);
  if (TRACK_SUBSCRIBED_BRANCHES) {
    publishBranches(service, tag, full_bid);
  }
  return full_bid;
}

std::string Server::registerBranches(metadata::Request * request, const std::string& service, const utils::ProtoVec& regions, const std::string& tag, std::string bid) {
  if (bid.empty()) {
    bid = genBid(request);
  }
  metadata::Branch * branch = request->registerBranches(bid, service, tag, regions);
  if (!branch) {
    return "";
  };
  const std::string& full_bid = getFullBid(request, bid);
  if (TRACK_SUBSCRIBED_BRANCHES) {
    publishBranches(service, tag, full_bid);
  }
  return full_bid;
}

int Server::closeBranch(metadata::Request * request, const std::string& bid, const std::string& region) {
  return request->closeBranch(bid, region);
}

int Server::waitRequest(metadata::Request * request, const std::string& service, const std::string& region, bool async, int timeout) {
  int result;
  metadata::Subscriber * subscriber;
  const std::string& rid = request->getRid();

  if (!service.empty() && !region.empty())
    result = request->waitOnServiceAndRegion(service, region, async, timeout);

  else if (!service.empty())
    result = request->waitOnService(service, async, timeout);

  else if (!region.empty())
    result = request->waitOnRegion(region, async, timeout);

  else
    result = request->wait(timeout);

  // TODO: REMOVE THIS FOR RELEASE!
  if (result == 1) {
    _prevented_inconsistencies.fetch_add(1);
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