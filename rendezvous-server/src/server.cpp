#include "server.h"

using namespace rendezvous;

Server::Server(std::string sid, json settings)
    : _next_rid(0), _prevented_inconsistencies(0), _sid(sid), 
    _requests_cleanup_sleep_m(settings["requests_cleanup_sleep_m"].get<int>()),
    _subscribers_cleanup_sleep_m(settings["subscribers_cleanup_sleep_m"].get<int>()),
    _subscribers_max_wait_time_s(settings["subscribers_max_wait_time_s"].get<int>()),
    _wait_replica_timeout_s(settings["wait_replica_timeout_s"].get<int>()) {

      std::cout << "- Requests cleanup sleep: " << _requests_cleanup_sleep_m << "m" << std::endl;
      std::cout << "- Subscribers cleanup sleep: " << _subscribers_cleanup_sleep_m << "m" << std::endl;
      std::cout << "- Subscribers max wait time: " << _subscribers_max_wait_time_s << "s" << std::endl;
      std::cout << "- Wait replica timeout: " << _wait_replica_timeout_s << "s" << std::endl;
    
    _requests = std::unordered_map<std::string, metadata::Request*>();
    _subscribers = std::unordered_map<std::string, std::unordered_map<std::string, metadata::Subscriber*>>();
}

// testing purposes
Server::Server(std::string sid)
    : _next_rid(0), _prevented_inconsistencies(0), _sid(sid), 
    _requests_cleanup_sleep_m(30),
    _subscribers_cleanup_sleep_m(30),
    _subscribers_max_wait_time_s(60),
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

  // manually upgrade lock (TODO: use tbb::wr_mutex later)
  read_lock.unlock();
  std::shared_lock<std::shared_mutex> write_lock(_mutex_subscribers);

  // register new subscriber
  metadata::Subscriber * subscriber = new metadata::Subscriber(_subscribers_max_wait_time_s);
  _subscribers[subscriber_id][region] = subscriber;
  return subscriber;
}

void Server::publishBranches(const std::string& service, const std::string& tag, const std::string& bid) {
  metadata::Subscriber * subscriber;
  const std::string& subscriber_id = computeSubscriberId(service, tag);

  //spdlog::debug("tracking branch {} for subscriber id {}", bid.c_str(), subscriber_id.c_str());
  
  std::shared_lock<std::shared_mutex> read_lock(_mutex_subscribers);
  auto it_regions = _subscribers.find(subscriber_id);

  // found subscriber in certain regions
  if (it_regions != _subscribers.end()) {
    for (const auto& subscriber : it_regions->second) {
      subscriber.second->pushBranch(bid);
    }
  }
}

// -----------------
// Garbage Collector
//------------------

void Server::initSubscribersCleanup() {
  std::thread([this]() {
    while (true) {
      break; //TODO: REMOVE
      std::this_thread::sleep_for(std::chrono::minutes(_subscribers_cleanup_sleep_m));

      auto now = std::chrono::system_clock::now();
      std::cout << "[INFO] initializing clean subscribers procedure..." << std::endl;

      std::unordered_map<std::string, metadata::Subscriber *> old_subscribers;

      // target old subscribers
      std::shared_lock<std::shared_mutex> read_lock(_mutex_subscribers);
      metadata::Subscriber * subscriber;
      for (auto regions_it = _subscribers.begin(); regions_it != _subscribers.end(); ++regions_it) {
        for (auto subscribers_it = regions_it->second.begin(); subscribers_it != regions_it->second.end(); subscribers_it++) {
          auto last_ts = subscribers_it->second->getLastTs();
          auto time_since = now - last_ts;

          if (time_since > std::chrono::minutes(_subscribers_cleanup_sleep_m)) {
            old_subscribers.insert({subscribers_it->first + "-" + regions_it->first, subscribers_it->second});
          }
        }
      }
      read_lock.unlock();
      std::unique_lock<std::shared_mutex> write_lock(_mutex_subscribers);

      // remove and delete old subscribers
      for (auto it = old_subscribers.begin(); it != old_subscribers.end(); it++) {
        _subscribers.erase(it->first);
        delete(it->second);
      }
    }
  }).detach();
}

void Server::initRequestsCleanup() {
  std::thread([this]() {

    while (true) {
      break; //TODO: REMOVE
      std::this_thread::sleep_for(std::chrono::minutes(_requests_cleanup_sleep_m));

      int num = 0;
      auto now = std::chrono::system_clock::now();
      std::cout << "[INFO] initializing clean requests procedure..." << std::endl;

      std::vector<metadata::Request*> requests;
      std::vector<metadata::Request*> old_requests;

      // copy requests
      std::shared_lock<std::shared_mutex> read_lock(_mutex_requests);
      std::transform(_requests.begin(), _requests.end(), requests.begin(),
                   [](const auto& pair){ return pair.second; });
      read_lock.unlock();

      // target old requests
      for (const auto & request : requests) {
        auto last_ts = request->getLastTs();
        auto time_since = now - last_ts;
        if (time_since > std::chrono::minutes(_requests_cleanup_sleep_m)) {
          old_requests.emplace_back(request);
        }
      }

      // remove and delete old requests
      std::unique_lock<std::shared_mutex> write_lock(_mutex_requests);
      for (const auto & request : old_requests) {
        _requests.erase(request->getRid());

        if (!LOG_REQUESTS) {
          delete request;
        }
      }
      write_lock.unlock();


      // log requests to output file and only delete after
      if (LOG_REQUESTS) {
        json j;
        for (const auto& request : old_requests) {
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

        std::cout << "[INFO] successfully cleaned " << num << " requests!" << std::endl;
      }
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
  return _sid + '_' + request->genId() + ':' + request->getRid();
}

std::string Server::parseRid(std::string bid) {
  size_t delimiter_pos = bid.find(':');
  std::string rid = "";

  if (delimiter_pos != std::string::npos) {
    rid = bid.substr(delimiter_pos+1);
  }
  return rid;
}

std::string Server::computeSubscriberId(const std::string& service, const std::string& tag) {
  return service + ":" + tag;
}

// -----------
// Helpers
//------------

long Server::getNumInconsistencies() {
  return _prevented_inconsistencies.load();
}

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

  std::unique_lock<std::shared_mutex> write_lock(_mutex_requests);

  // register request 
  metadata::Request * request = new metadata::Request(rid);
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

  if (TRACK_SUBSCRIBED_BRANCHES) {
    publishBranches(service, tag, bid);
  }

  return bid;
}

std::string Server::registerBranches(metadata::Request * request, const std::string& service, const utils::ProtoVec& regions, const std::string& tag, std::string bid) {
  if (bid.empty()) {
    bid = genBid(request);
  }
  metadata::Branch * branch = request->registerBranches(bid, service, tag, regions);

  if (!branch) {
    return "";
  }

  if (TRACK_SUBSCRIBED_BRANCHES) {
    publishBranches(service, tag, bid);
  }

  return bid;
}

int Server::closeBranch(metadata::Request * request, const std::string& bid, const std::string& region, bool client_request) {
  std::string service, tag;
  return request->closeBranch(bid, region, service, tag);
}

int Server::waitRequest(metadata::Request * request, const std::string& service, const std::string& region, int timeout) {
  int result;
  metadata::Subscriber * subscriber;
  const std::string& rid = request->getRid();

  if (!service.empty() && !region.empty())
    result = request->waitOnServiceAndRegion(service, region, timeout);

  else if (!service.empty())
    result = request->waitOnService(service, timeout);

  else if (!region.empty())
    result = request->waitOnRegion(region, timeout);

  else
    result = request->wait(timeout);

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

long Server::getNumPreventedInconsistencies() {
  return _prevented_inconsistencies.load();
}