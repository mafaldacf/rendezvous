#include "request.h"

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versions_registry)
    : _rid(rid), _next_id(0), _num_branches(0), _versions_registry(versions_registry) {
    _last_ts = std::chrono::system_clock::now();
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
    _mutex_branches.unlock();
    return branch;
}

int Request::closeBranch(const std::string& bid, const std::string& region) {
    std::unique_lock<std::mutex> lock(_mutex_branches);
    bool region_found = true;
    auto branch_it = _branches.find(bid);

    // not found
    if (branch_it == _branches.end()) {
        return 0;
    }

    metadata::Branch * branch = branch_it->second;
    const std::string& service = branch->getService();
    const std::string& tag = branch->getTag();

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
        if (remaining_timeout <= std::chrono::seconds(0)) {
            return -1;
        }
    }
    return inconsistency;
}

int Request::waitOnService(const std::string& service, bool async, int timeout) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_num_branches_service);

    // branch is expected to be asynchronously opened so we need to wait for the branch context
    if (async) {
        while (_num_branches_service.count(service) == 0) {
            inconsistency = 1;
            _cond_branches.wait_for(lock, remaining_timeout);
            remaining_timeout = computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    // context not found
    else if (_num_branches_service.count(service) == 0) {
        return -2;
    }

    // original wait logic
    int * num_branches_ptr = &_num_branches_service[service];
    while (*num_branches_ptr != 0) {
        _cond_branches.wait_for(lock, remaining_timeout);
        inconsistency = 1;
        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) {
            return -1;
        }
    }
    
    return inconsistency;
}

int Request::waitOnRegion(const std::string& region, bool async, int timeout) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_num_branches_region);

    // branch is expected to be asynchronously opened so we need to wait for the branch context
    if (async) {
        while (_num_branches_region.count(region) == 0) {
            inconsistency = 1;
            _cond_num_branches_region.wait_for(lock, remaining_timeout);
            remaining_timeout = computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    // context not found
    else if (_num_branches_region.count(region) == 0) {
        return -2;
    }

    // original wait logic
    int * num_branches_ptr = &_num_branches_region[region];
    while (*num_branches_ptr != 0) {
        inconsistency = 1;
        _cond_num_branches_region.wait_for(lock, remaining_timeout);
        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) {
            return -1;
        }
    }

    return inconsistency;
}

int Request::waitOnServiceAndRegion(const std::string& service, const std::string& region, bool async, int timeout) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_num_branches_service_region);

    // branch is expected to be asynchronously opened so we need to wait for the branch context
    if (async) {
        auto service_it = _num_branches_service_region.find(service);
        while (service_it == _num_branches_service_region.end()) {
            _cond_num_branches_service_region.wait_for(lock, remaining_timeout);
            inconsistency = 1;
            remaining_timeout = computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
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
    }
    // context not found
    else if (_num_branches_service_region.count(service) == 0 || _num_branches_service_region[service].count(region) == 0) {
        return -2;
    }

    int * num_branches_ptr = &_num_branches_service_region[service][region];

    // original wait logic
    while (*num_branches_ptr != 0) {
        _cond_num_branches_service_region.wait_for(lock, remaining_timeout);
        inconsistency = 1;
        remaining_timeout = computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) {
            return -1;
        }
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
    std::unique_lock<std::mutex> lock(_mutex_num_branches_service);
    if (!_num_branches_service.count(service)) {
        return UNKNOWN;
    }
    if (_num_branches_service[service] == 0) {
        return CLOSED;
    }
    return OPENED;
}

int Request::getStatusOnRegion(const std::string& region) {
    std::unique_lock<std::mutex> lock(_mutex_num_branches_region);
    if (!_num_branches_region.count(region)) {
        return UNKNOWN;
    }
    if (_num_branches_region[region] == 0) {
        return CLOSED;
    }
    return OPENED;
}

int Request::getStatusOnServiceAndRegion(const std::string& service, const std::string& region) {
    std::unique_lock<std::mutex> lock(_mutex_num_branches_service_region);
    auto service_it = _num_branches_service_region.find(service);
    if (service_it == _num_branches_service_region.end()) {
        return UNKNOWN;
    }
    auto region_it = service_it->second.find(region);
    if (region_it == service_it->second.end()) {
        return UNKNOWN;
    }
    if (_num_branches_service_region[service][region] == 0) {
        return CLOSED;
    }
    return OPENED;
}

std::map<std::string, int> Request::getStatusByRegions() {
    std::unique_lock<std::mutex> lock(_mutex_num_branches_region);
    std::map<std::string, int> result = std::map<std::string, int>();
    for (const auto& it : _num_branches_region) {
        result[it.first] = it.second == 0 ? CLOSED : OPENED;
    }
    return result;
}

std::map<std::string, int> Request::getStatusByRegionsOnService(const std::string& service) {
    std::unique_lock<std::mutex> lock(_mutex_num_branches_service_region);
    std::map<std::string, int> result = std::map<std::string, int>();
    auto service_it = _num_branches_service_region.find(service);

    // no status found for a given service
    if (service_it == _num_branches_service_region.end()) {
        return result;
    }
    // <service, <region, number of opened branches>>
    for (const auto& region_it : service_it->second) {
        // sanity check: skip branches with no region
        if (!region_it.first.empty()) {
            result[region_it.first] = region_it.second == 0 ? CLOSED : OPENED;
        }
    }
    return result;
}