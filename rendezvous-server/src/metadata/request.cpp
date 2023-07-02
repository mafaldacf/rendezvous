#include "request.h"

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versions_registry)
    : _rid(rid), _next_id(0), _num_branches(0), _versions_registry(versions_registry) {
    
    _init_ts = std::chrono::system_clock::now();
    _last_ts = _init_ts;
    
    // <bid, branch>
    _branches = std::unordered_map<std::string, metadata::Branch*>();

    _num_branches_region = std::unordered_map<std::string, int>();
    _num_branches_service = std::unordered_map<std::string, int>();
    _num_branches_service_region = std::unordered_map<std::string, std::unordered_map<std::string, int>>();
}

Request::~Request() {
    for (const auto& branch_it : _branches) {
        delete branch_it.second;
    }
    delete _versions_registry;
}

json Request::toJson() const {
    json j;

    std::time_t init_ts_t = std::chrono::system_clock::to_time_t(_init_ts);
    std::time_t last_ts_t = std::chrono::system_clock::to_time_t(_last_ts);
    auto init_ts_tm = *std::localtime(&init_ts_t);
    auto last_ts_tm = *std::localtime(&last_ts_t);

    std::ostringstream init_ts_oss;
    std::ostringstream last_ts_oss;
    init_ts_oss << std::put_time(&init_ts_tm, utils::TIME_FORMAT.c_str());
    last_ts_oss << std::put_time(&last_ts_tm, utils::TIME_FORMAT.c_str());
    auto init_ts_str = init_ts_oss.str();
    auto final_ts_str = last_ts_oss.str();

    auto duration = _last_ts - _init_ts;
    auto duration_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    j[_rid]["init_ts"] = init_ts_str;
    j[_rid]["last_ts"] = final_ts_str;
    j[_rid]["duration_ms"] = duration_seconds.count();

    for (const auto& branches_it : _branches) {
        j[_rid]["branches"].push_back(branches_it.second->toJson(branches_it.first));
    }
    return j;
}

std::chrono::time_point<std::chrono::system_clock> Request::getLastTs() {
    return _last_ts;
}

void Request::refreshLastTs() {
    _last_ts = std::chrono::system_clock::now();
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

metadata::Branch * Request::registerBranch(const std::string& bid, const std::string& service, const std::string& tag, const std::string& region) {
    metadata::Branch * branch = nullptr;
    _mutex_branches.lock();

    auto branch_it = _branches.find(bid);

    // branches already exist
    if (branch_it != _branches.end()) {
        _mutex_branches.unlock();
        return branch;
    }

    // branch not found
    branch = new metadata::Branch(service, tag, region);
    _branches[bid] = branch;
    trackBranchOnContext(service, region, REGISTER);

    refreshLastTs();
    _cond_new_branches.notify_all();
    _mutex_branches.unlock();
    return branch;
}

metadata::Branch * Request::registerBranches(const std::string& bid, const std::string& service, const std::string& tag, const utils::ProtoVec& regions) {
    metadata::Branch * branch = nullptr;

    _mutex_branches.lock();

    auto branch_it = _branches.find(bid);

    // branches already exist
    if (branch_it != _branches.end()) {
        _mutex_branches.unlock();
        return branch;
    }

    branch = new metadata::Branch(service, tag, regions);
    _branches[bid] = branch;
    int num = regions.size();
    trackBranchesOnContext(service, regions, REGISTER, num);
    
    refreshLastTs();
    _cond_new_branches.notify_all();
    _mutex_branches.unlock();
    return branch;
}

int Request::closeBranch(const std::string& bid, const std::string& region, std::string& service, std::string& tag) {
    std::unique_lock<std::mutex> lock(_mutex_branches);

    bool region_found = true;
    auto branch_it = _branches.find(bid);

    // not found
    if (branch_it == _branches.end()) {
        return 0;
    }

    metadata::Branch * branch = branch_it->second;
    service = branch->getService();
    tag = branch->getTag();

    int res = branch->close(region);
    if (res == 1) {
        trackBranchOnContext(branch->getService(), region, REMOVE);
    }
    else if (res == -1) {
        return -1;
    }

    refreshLastTs();
    _cond_branches.notify_all();
    return 1;
}

void Request::trackBranchOnContext(const std::string& service, const std::string& region, long value) {
    _num_branches.fetch_add(value);

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

void Request::trackBranchesOnContext(const std::string& service, const utils::ProtoVec& regions, long value, int num) {
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

std::chrono::seconds Request::computeRemainingTimeout(int timeout, const std::chrono::steady_clock::time_point& start_time) {
    if (timeout != 0) {
        auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        auto remaining_timeout = std::chrono::seconds(timeout) - std::chrono::duration_cast<std::chrono::seconds>(elapsed_time);
        return remaining_timeout;
    }
    return std::chrono::seconds(60);
}

int Request::wait(int timeout) {
    int inconsistency = 0;
    
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = computeRemainingTimeout(timeout, start_time);

    std::unique_lock<std::mutex> lock(_mutex_branches);
    while (_num_branches.load() != 0) {
        _cond_branches.wait_for(lock, std::chrono::seconds(remaining_timeout));
        inconsistency = 1;

        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) return -1;
    }
    return inconsistency;
}

int Request::waitOnService(const std::string& service, int timeout) {
    int inconsistency = 0;

    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = computeRemainingTimeout(timeout, start_time);

    std::unique_lock<std::mutex> lock(_mutex_num_branches_service);

    // wait until branch is registered
    while (_num_branches_service.count(service) == 0) {
        inconsistency = 1;
        _cond_branches.wait_for(lock, remaining_timeout);
        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) return -1;
    }

    int * num_branches_ptr = &_num_branches_service[service];
    while (*num_branches_ptr != 0) {
        _cond_branches.wait_for(lock, remaining_timeout);
        inconsistency = 1;
        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) return -1;
    }
    
    return inconsistency;
}

int Request::waitOnRegion(const std::string& region, int timeout) {
    int inconsistency = 0;

    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = computeRemainingTimeout(timeout, start_time);

    std::unique_lock<std::mutex> lock(_mutex_num_branches_region);

    // branch not registered yet
    while (_num_branches_region.count(region) == 0) {
        inconsistency = 1;
        _cond_num_branches_region.wait_for(lock, remaining_timeout);
        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) return -1;
    }

    int * num_branches_ptr = &_num_branches_region[region];
    while (*num_branches_ptr != 0) {
        inconsistency = 1;
        _cond_num_branches_region.wait_for(lock, remaining_timeout);
        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) return -1;
    }

    return inconsistency;
}

int Request::waitOnServiceAndRegion(const std::string& service, const std::string& region, int timeout) {
    int inconsistency = 0;

    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = computeRemainingTimeout(timeout, start_time);

    std::unique_lock<std::mutex> lock(_mutex_num_branches_service_region);

    auto service_it = _num_branches_service_region.find(service);
    while (service_it == _num_branches_service_region.end()) {
        _cond_num_branches_service_region.wait_for(lock, remaining_timeout);
        inconsistency = 1;

        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) return -1;

        service_it = _num_branches_service_region.find(service);
    }

    auto region_it = service_it->second.find(region);
    while (region_it == service_it->second.end()) {
        _cond_num_branches_service_region.wait_for(lock, remaining_timeout);
        inconsistency = 1;

        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) return -1;

        region_it = service_it->second.find(region);
    }

    int * num_branches_ptr = &region_it->second;

    while (*num_branches_ptr != 0) {
        _cond_num_branches_service_region.wait_for(lock, remaining_timeout);
        inconsistency = 1;

        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) return -1;
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
        return UNKNOWN;
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
        return UNKNOWN;
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
        return UNKNOWN;
    }

    auto region_it = service_it->second.find(region);
    if (region_it == service_it->second.end()) {
        _mutex_num_branches_service_region.unlock();
        return UNKNOWN;
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