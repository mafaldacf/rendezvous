#include "request.h"

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versions_registry)
    : _rid(rid), _next_id(0), _num_branches(0), _versions_registry(versions_registry) {
    
    _init_ts = std::chrono::system_clock::now();
    _final_ts = _init_ts;
    
    // <bid, branch>
    _branches = std::unordered_map<std::string, metadata::Branch*>();

    _num_branches_region = std::unordered_map<std::string, uint64_t>();
    _num_branches_service = std::unordered_map<std::string, uint64_t>();
    _num_branches_service_region = std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>>();
}

Request::~Request() {
    for (const auto& branch_it : _branches) {
        delete branch_it.second;
    }
    delete _versions_registry;
}

json Request::toJson() const {
    json j;

    std::time_t initTs_t = std::chrono::system_clock::to_time_t(_init_ts);
    std::time_t finalTs_t = std::chrono::system_clock::to_time_t(_final_ts);
    auto initialTs_tm = *std::localtime(&initTs_t);
    auto finalTs_tm = *std::localtime(&finalTs_t);

    std::ostringstream initialTs_oss;
    std::ostringstream finalTs_oss;
    initialTs_oss << std::put_time(&initialTs_tm, utils::TIME_FORMAT.c_str());
    finalTs_oss << std::put_time(&finalTs_tm, utils::TIME_FORMAT.c_str());
    auto initialTs_str = initialTs_oss.str();
    auto finalTs_str = finalTs_oss.str();

    auto duration = _final_ts - _init_ts;
    auto duration_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    j[_rid]["init_ts"] = initialTs_str;
    j[_rid]["last_ts"] = finalTs_str;
    j[_rid]["duration_ms"] = duration_seconds.count();

    for (const auto& branches_it : _branches) {
        j[_rid]["branches"].push_back(branches_it.second->toJson());
    }
    return j;
}

bool Request::canRemove(std::chrono::time_point<std::chrono::system_clock> now) {
    auto timeSince = now - _final_ts;
    return timeSince > std::chrono::hours(12);
}

std::string Request::getRid() {
    return _rid;
}

replicas::VersionRegistry * Request::getVersionsRegistry() {
    return _versions_registry;
}

std::string Request::genId() {
  return std::to_string(_next_id.fetch_add(1));
}

bool Request::registerBranch(const std::string& bid, const std::string& service, const std::string& region) {
    _mutex_branches.lock();

    auto branch_it = _branches.find(bid);

    // branches already exist
    if (branch_it != _branches.end()) {
        _mutex_branches.unlock();
        return false;
    }

    // branch not found
    if (branch_it == _branches.end()) {
        metadata::Branch * branch = new metadata::Branch(bid, service, region);
        _branches[bid] = branch;
        trackBranchOnContext(service, region, REGISTER);
    }

    _cond_new_branches.notify_all();
    _mutex_branches.unlock();
    return true;
}

bool Request::registerBranches(const std::string& bid, const std::string& service, const utils::ProtoVec& regions) {
    _mutex_branches.lock();

    auto branch_it = _branches.find(bid);

    // branches already exist
    if (branch_it != _branches.end()) {
        _mutex_branches.unlock();
        return false;
    }

    metadata::Branch * branch = new metadata::Branch(bid, service, regions);
    _branches[bid] = branch;
    int num = regions.size();
    trackBranchesOnContext(service, regions, REGISTER, num);

    _cond_new_branches.notify_all();
    _mutex_branches.unlock();
    return true;
}

bool Request::closeBranch(const std::string& bid, const std::string& region) {
    std::unique_lock<std::mutex> lock(_mutex_branches);

    bool region_found = false;
    auto branch_it = _branches.find(bid);

    // branch not found
    if (branch_it == _branches.end()) {
        std::thread([this, &branch_it, bid, region]() {
            std::unique_lock<std::mutex> lock_thread(_mutex_branches);

            while (branch_it == _branches.end()) {
                _cond_new_branches.wait(lock_thread);
                branch_it = _branches.find(bid);
            }
            metadata::Branch * branch = branch_it->second;
            if (branch->close(region)) {
                trackBranchOnContext(branch->getService(), region, REMOVE);
            }
        }).detach();
        return true;
    }

    metadata::Branch * branch = branch_it->second;
    int res = branch->close(region);
    if (res == 1) {
        trackBranchOnContext(branch->getService(), region, REMOVE);
        region_found = true;
    }
    else if (res == 0) {
        region_found = true;
    }

    _cond_branches.notify_all();
    return region_found;
}

void Request::trackBranchOnContext(const std::string& service, const std::string& region, const long& value) {
    _num_branches.fetch_add(value);

    if (value == REMOVE && _num_branches.load() == 0) {
        _final_ts = std::chrono::system_clock::now();
    }

    if (!service.empty()) {
        _mutex_num_branches_service.lock();

        _num_branches_service[service] += value;
        if (value == REMOVE) 
            _cond_num_branches_service.notify_all();

        _mutex_num_branches_service.unlock();
    }
    if (!region.empty()) {
        _mutex_num_branches_region.lock();

        _num_branches_region[region] += value;
        if (value == REMOVE) 
            _cond_num_branches_region.notify_all();
            
        _mutex_num_branches_region.unlock();
    }
    if (!service.empty() && !region.empty()) {
        _mutex_num_branches_service_region.lock();

        _num_branches_service_region[service][region] += value;
        if (value == REMOVE) 
            _cond_num_branches_service_region.notify_all();
            
        _mutex_num_branches_service_region.unlock();
    }
}

void Request::trackBranchesOnContext(const std::string& service, const utils::ProtoVec& regions, const long& value, const int& num) {
    _num_branches.fetch_add(value*num);

    if (!service.empty()) {
        _mutex_num_branches_service.lock();

        _num_branches_service[service] += value*num;
        if (value == REMOVE) 
            _cond_num_branches_service.notify_all();

        _mutex_num_branches_service.unlock();
    }

    if (regions.size() > 0) {
        _mutex_num_branches_region.lock();
        _mutex_num_branches_service_region.lock();

        for (const auto& region : regions) {
            // sanity check - region can never be empty when registering a set of branches
            if (!region.empty()) {

                _num_branches_region[region] += value;
                if (value == REMOVE)
                    _cond_num_branches_region.notify_all();

                if (!service.empty()) {
                    _num_branches_service_region[service][region] += value;
                    if (value == REMOVE)
                        _cond_num_branches_service_region.notify_all();
                }
            }
        }

        _mutex_num_branches_region.unlock();
        _mutex_num_branches_service_region.unlock();
    }

}

int Request::wait() {
    int inconsistency = 0;
    std::unique_lock<std::mutex> lock(_mutex_branches);
    while (_num_branches.load() != 0) {
        _cond_branches.wait(lock);
        inconsistency = 1;
    }
    return inconsistency;
}

int Request::waitOnService(const std::string& service) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(_mutex_num_branches_service);

    if (_num_branches_service.count(service) == 0) { // not found
        return NOT_FOUND;
    }

    uint64_t * valuePtr = &_num_branches_service[service];
    while (*valuePtr != 0) {
        _cond_num_branches_service.wait(lock);
        inconsistency = 1;
    }
    
    return inconsistency;
}

int Request::waitOnRegion(const std::string& region) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(_mutex_num_branches_region);

    if (_num_branches_region.count(region) == 0) { // not found
        return NOT_FOUND;
    }

    uint64_t * valuePtr = &_num_branches_region[region];
    while (*valuePtr != 0) {
        _cond_num_branches_region.wait(lock);
        inconsistency = 1;
    }

    return inconsistency;
}

int Request::waitOnServiceAndRegion(const std::string& service, const std::string& region) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(_mutex_num_branches_service_region);

    auto service_it = _num_branches_service_region.find(service);
    if (service_it == _num_branches_service_region.end()) {
        return NOT_FOUND;
    }

    auto region_it = service_it->second.find(region);
    if (region_it == service_it->second.end()) {
        return NOT_FOUND;
    }

    uint64_t * valuePtr = &_num_branches_service_region[service][region];

    while (*valuePtr != 0) {
        _cond_num_branches_service_region.wait(lock);
        inconsistency = 1;
    }
    return inconsistency;
}

int Request::getStatus() {
    if (_num_branches.load() == 0) {
        return CLOSED;
    }
    return OPENED;
}

int Request::getStatusOnService(const std::string& service) {
    _mutex_num_branches_service.lock();

    if (!_num_branches_service.count(service)) {
        _mutex_num_branches_service.unlock();
        return NOT_FOUND;
    }

    if (_num_branches_service[service] == 0) {
        _mutex_num_branches_service.unlock();
        return CLOSED;
    }

    _mutex_num_branches_service.unlock();
    return OPENED;
}

int Request::getStatusOnRegion(const std::string& region) {
    _mutex_num_branches_region.lock();

    if (!_num_branches_region.count(region)) {
        _mutex_num_branches_region.unlock();
        return NOT_FOUND;
    }

    if (_num_branches_region[region] == 0) {
        _mutex_num_branches_region.unlock();
        return CLOSED;
    }

    _mutex_num_branches_region.unlock();
    return OPENED;
}

int Request::getStatusOnServiceAndRegion(const std::string& service, const std::string& region) {
    _mutex_num_branches_service_region.lock();

    auto service_it = _num_branches_service_region.find(service);
    if (service_it == _num_branches_service_region.end()) {
        _mutex_num_branches_service_region.unlock();
        return NOT_FOUND;
    }

    auto region_it = service_it->second.find(region);
    if (region_it == service_it->second.end()) {
        _mutex_num_branches_service_region.unlock();
        return NOT_FOUND;
    }

    if (_num_branches_service_region[service][region] == 0) {
        _mutex_num_branches_service_region.unlock();
        return CLOSED;
    }

    _mutex_num_branches_service_region.unlock();
    return OPENED;
}

std::map<std::string, int> Request::getStatusByRegions() {
    std::map<std::string, int> result = std::map<std::string, int>();

    _mutex_num_branches_region.lock();
    for (const auto& it : _num_branches_region) {
        result[it.first] = it.second == 0 ? CLOSED : OPENED;
    }
    _mutex_num_branches_region.unlock();
    
    return result;
}

std::map<std::string, int> Request::getStatusByRegionsOnService(const std::string& service) {
    std::map<std::string, int> result = std::map<std::string, int>();

    _mutex_num_branches_service_region.lock();
    auto service_it = _num_branches_service_region.find(service);

    // no status found for a given service
    if (service_it == _num_branches_service_region.end()) {
        _mutex_num_branches_service_region.unlock();
        return result;
    }

    // <service, <region, number of opened branches>>
    for (const auto& region_it : service_it->second) {
        // sanity check: skip branches with no region
        if (!region_it.first.empty()) {
            result[region_it.first] = region_it.second == 0 ? CLOSED : OPENED;
        }
    }
    
    _mutex_num_branches_service_region.unlock();
    
    return result;
}