#include "request.h"

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versions_registry)
    : _rid(rid), _next_id(0), _num_opened_branches(0), _versions_registry(versions_registry) {
    _last_ts = std::chrono::system_clock::now();
    // <bid, branch>
    _branches = std::unordered_map<std::string, metadata::Branch*>();
    _opened_regions = std::unordered_map<std::string, int>();
    _service_branching = std::unordered_map<std::string, ServiceBranching>();
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

metadata::Branch * Request::registerBranch(const std::string& bid, const std::string& service, 
const std::string& tag, const std::string& region) {

    std::unique_lock<std::mutex> lock(_mutex_branches);
    auto branch_it = _branches.find(bid);
    // branches already exists
    if (branch_it != _branches.end()) {
        return nullptr;
    }

    metadata::Branch * branch = new metadata::Branch(service, tag, region);
    _branches[bid] = branch;
    trackBranchOnContext(service, region, REGISTER, branch);
    refreshLastTs();
    return branch;
}

metadata::Branch * Request::registerBranches(const std::string& bid, const std::string& service, 
    const std::string& tag, const utils::ProtoVec& regions) {

    std::unique_lock<std::mutex> lock(_mutex_branches);
    auto branch_it = _branches.find(bid);
    // branches already exist
    if (branch_it != _branches.end()) {
        return nullptr;
    }

    metadata::Branch * branch = new metadata::Branch(service, tag, regions);
    _branches[bid] = branch;
    int num = regions.size();
    trackBranchesOnContext(service, regions, num, branch);
    refreshLastTs();
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

    int r = branch->close(region);
    if (r == 1) {
        trackBranchOnContext(service, region, REMOVE);
        refreshLastTs();
        _cond_branches.notify_all();
    }
    return r;
}

void Request::trackBranchOnContext(const std::string& service, const std::string& region, long value, metadata::Branch * branch) {
    _num_opened_branches.fetch_add(value);

    // service context
    if (!service.empty()) {
        std::unique_lock<std::mutex> lock(_mutex_service_branching);
        _service_branching[service].num_opened_branches += value;
        // service and region context
        if (!region.empty()) {
            _service_branching[service].opened_regions[region] += value;
        }

        // specify tag for service operation
        if (branch != nullptr) {
            _service_branching[service].tagged_branches[branch->getTag()] = branch;
        }
        // branch is global to the current service (no tag specified)
        else {
            _service_branching[service].global = true;
        }

        // notify upon removal and creation (due to async waits)
        if (value == REMOVE) {
            _cond_service_branching.notify_all();
        } else {
            _cond_new_service_branching.notify_all();
        }
    }
    // region context
    if (!region.empty()) {
        std::unique_lock<std::mutex> lock(_mutex_opened_regions);
        _opened_regions[region] += value;

        // notify upon removal and creation (due to async waits)
        if (value == REMOVE) {
            _cond_opened_regions.notify_all();
        } else {
            _cond_new_opened_regions.notify_all();
        }
    }
}

void Request::trackBranchesOnContext(const std::string& service, const utils::ProtoVec& regions, int num, metadata::Branch * branch) {
    _num_opened_branches.fetch_add(num);

    // service context
    if (!service.empty()) {
        std::unique_lock<std::mutex> lock_services(_mutex_service_branching);
        _service_branching[service].num_opened_branches += num;
        // service and region context + region only context
        std::unique_lock<std::mutex> lock_regions(_mutex_opened_regions);
        for (const auto& region : regions) {
            if (!region.empty()) {
                _service_branching[service].opened_regions[region] += 1; // SERVICE + REGION
                _opened_regions[region] += 1; // REGION
            }
        }
        // specify tag for service operation
        if (branch != nullptr) {
            _service_branching[service].tagged_branches[branch->getTag()] = branch;
        }
        // branch is global to the current service (no tag specified)
        else {
            _service_branching[service].global = true;
        }

        // notify upon creation (due to async waits)
        _cond_new_service_branching.notify_all();
    }
    // region context
    else {
        std::unique_lock<std::mutex> lock(_mutex_opened_regions);
        for (const auto& region : regions) {
            if (!region.empty()) {
                _opened_regions[region] += 1; // REGION
            }
        }

        // notify upon creation (due to async waits)
        _cond_new_service_branching.notify_all();
    }
}

std::chrono::seconds Request::_computeRemainingTimeout(int timeout, const std::chrono::steady_clock::time_point& start_time) {
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
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_branches);
    while (_num_opened_branches.load() != 0) {
        _cond_branches.wait_for(lock, std::chrono::seconds(remaining_timeout));
        inconsistency = 1;
        remaining_timeout = _computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) {
            return -1;
        }
    }
    return inconsistency;
}

int Request::waitOnService(const std::string& service, const std::string& tag, bool async, int timeout) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_service_branching);

    // --------------
    // CONTEXT CHECKS
    //---------------
    // branch is expected to be asynchronously opened so we need to wait for the branch context
    if (async) {
        while (_service_branching.count(service) == 0) {
            _cond_new_service_branching.wait_for(lock, remaining_timeout);
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    // context error checking
    else {
        // no current branch for this service
        if (_service_branching.count(service) == 0) {
            return -2;
        }
        // no current branch for this service (non async) tag
        if (!tag.empty() && _service_branching[service].tagged_branches.count(tag) == 0) {
            return -2;
        }
    }

    // ----------
    // WAIT LOGIC
    //-----------
    // tag-specific
    if (!tag.empty()) {
        metadata::Branch * branch = _service_branching[service].tagged_branches[tag];
        while (!branch->isClosed()) {
            inconsistency = 1;
            _cond_service_branching.wait_for(lock, remaining_timeout);
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    // overall service
    else {
        int * num_branches_ptr = &(_service_branching[service].num_opened_branches);
        while (*num_branches_ptr != 0) {
            _cond_service_branching.wait_for(lock, remaining_timeout);
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    return inconsistency;
}

int Request::waitOnRegion(const std::string& region, bool async, int timeout) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_opened_regions);

    // --------------
    // CONTEXT CHECKS
    //---------------
    // branch is expected to be asynchronously opened so we need to wait for the branch context
    if (async) {
        while (_opened_regions.count(region) == 0) {
            _cond_new_opened_regions.wait_for(lock, remaining_timeout);
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    // context not found
    else if (_opened_regions.count(region) == 0) {
        return -2;
    }

    // ----------
    // WAIT LOGIC
    //-----------
    int * num_branches_ptr = &_opened_regions[region];
    while (*num_branches_ptr != 0) {
        inconsistency = 1;
        _cond_opened_regions.wait_for(lock, remaining_timeout);
        remaining_timeout = _computeRemainingTimeout(timeout, start_time);
        if (remaining_timeout <= std::chrono::seconds(0)) {
            return -1;
        }
    }

    return inconsistency;
}

int Request::waitOnServiceAndRegion(const std::string& service, const std::string& region, 
const::std::string& tag, bool async, int timeout) {

    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_service_branching);

    // --------------
    // CONTEXT CHECKS
    //---------------
    // wait for branch context (service and region)
    if (async) {
        auto service_it = _service_branching.find(service);
        while (service_it == _service_branching.end()) {
            _cond_new_service_branching.wait_for(lock, remaining_timeout);
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
            service_it = _service_branching.find(service);
        }
        auto opened_regions_it = service_it->second.opened_regions;
        auto region_it = opened_regions_it.find(region);
        while (region_it == opened_regions_it.end()) {
            _cond_new_service_branching.wait_for(lock, remaining_timeout);
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) return -1;

            region_it = opened_regions_it.find(region);
        }
    }
    // context not found
    else if (_service_branching.count(service) == 0 || _service_branching[service].opened_regions.count(region) == 0) {
        return -2;
    }

    // ----------
    // WAIT LOGIC
    //-----------
    // tag-specific
    if (!tag.empty()) {
        metadata::Branch * branch = _service_branching[service].tagged_branches[tag];
        while (!branch->isClosed(region)) {
            inconsistency = 1;
            _cond_service_branching.wait_for(lock, remaining_timeout);
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    // overall service
    else {
        int * num_branches_ptr = &_service_branching[service].opened_regions[region];
        while (*num_branches_ptr != 0) {
            _cond_service_branching.wait_for(lock, remaining_timeout);
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    return inconsistency;
}

int Request::getStatus() {
    if (_num_opened_branches.load() == 0) {
        return CLOSED;
    }
    return OPENED;
}

int Request::getStatusOnService(const std::string& service) {
    std::unique_lock<std::mutex> lock(_mutex_service_branching);
    if (!_service_branching.count(service)) {
        return UNKNOWN;
    }
    if (_service_branching[service].num_opened_branches == 0) {
        return CLOSED;
    }
    return OPENED;
}

int Request::getStatusOnRegion(const std::string& region) {
    std::unique_lock<std::mutex> lock(_mutex_opened_regions);
    if (!_opened_regions.count(region)) {
        return UNKNOWN;
    }
    if (_opened_regions[region] == 0) {
        return CLOSED;
    }
    return OPENED;
}

int Request::getStatusOnServiceAndRegion(const std::string& service, const std::string& region) {
    std::unique_lock<std::mutex> lock(_mutex_service_branching);
    auto service_it = _service_branching.find(service);
    if (service_it == _service_branching.end()) {
        return UNKNOWN;
    }
    auto region_it = service_it->second.opened_regions.find(region);
    if (region_it == service_it->second.opened_regions.end()) {
        return UNKNOWN;
    }
    if (_service_branching[service].opened_regions[region] == 0) {
        return CLOSED;
    }
    return OPENED;
}

std::map<std::string, int> Request::getStatusByRegions() {
    std::unique_lock<std::mutex> lock(_mutex_opened_regions);
    std::map<std::string, int> result = std::map<std::string, int>();
    for (const auto& it : _opened_regions) {
        result[it.first] = it.second == 0 ? CLOSED : OPENED;
    }
    return result;
}

std::map<std::string, int> Request::getStatusByRegionsOnService(const std::string& service) {
    std::unique_lock<std::mutex> lock(_mutex_service_branching);
    std::map<std::string, int> result = std::map<std::string, int>();
    auto service_it = _service_branching.find(service);

    // no status found for a given service
    if (service_it == _service_branching.end()) {
        return result;
    }
    // <service, <region, number of opened branches>>
    for (const auto& region_it : service_it->second.opened_regions) {
        // sanity check: skip branches with no region
        if (!region_it.first.empty()) {
            result[region_it.first] = region_it.second == 0 ? CLOSED : OPENED;
        }
    }
    return result;
}