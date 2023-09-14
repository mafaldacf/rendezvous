#include "request.h"

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versions_registry)
    : _rid(rid), _next_id(0), _num_opened_branches(0), _versions_registry(versions_registry) {
    _last_ts = std::chrono::system_clock::now();
    // <bid, branch>
    _branches = std::unordered_map<std::string, metadata::Branch*>();
    _opened_regions = std::unordered_map<std::string, int>();
    _service_nodes = std::unordered_map<std::string, ServiceNode>();
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
    const std::string& tag, const utils::ProtoVec& regions, const std::string& parent_service) {

    std::unique_lock<std::mutex> lock(_mutex_branches);
    auto branch_it = _branches.find(bid);

    // branches already exist
    if (branch_it != _branches.end()) {
        return nullptr;
    }

    metadata::Branch * branch;
    int num = regions.size();

    // branch with specified regions
    if (num > 1) {
        branch = new metadata::Branch(service, tag, regions);
    }

    // no region specified - global region
    else {
        num = 1;
        branch = new metadata::Branch(service, tag);
    }

    // error tracking branch (tag already exists!)
    if (!trackBranch(service, regions, num, parent_service, branch)) {
        delete branch;
        return nullptr;
    }
    _branches[bid] = branch;

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
        untrackBranch(service, region);
        _cond_branches.notify_all();
    }
    return r;
}

bool Request::untrackBranch(const std::string& service, const std::string& region) {
    _num_opened_branches.fetch_add(-1);

    /* --------------- */
    /* service context */
    /* --------------- */
    if (!service.empty()) {
        std::unique_lock<std::mutex> lock(_mutex_service_nodes);
        _service_nodes[service].num_opened_branches -= 1;
        // service and region context
        if (!region.empty()) {
            _service_nodes[service].opened_regions[region] -= 1;
        }

        // get parent node
        ServiceNodeStruct * parent_node = &_service_nodes[service];
        // decrease opened branches in all parents
        while (parent_node != nullptr) {
            parent_node->num_opened_branches--;
            parent_node = parent_node->parent;
        }

        // notify creation of new branch
        _cond_service_nodes.notify_all();
    }

    /* -------------- */
    /* region context */
    /* -------------- */
    if (!region.empty()) {
        std::unique_lock<std::mutex> lock(_mutex_opened_regions);
        _opened_regions[region] -= 1;

        // notify creation of new branch
        _cond_opened_regions.notify_all();
    }
    return true;
}

bool Request::trackBranch(const std::string& service, const utils::ProtoVec& regions, int num, 
    const std::string& parent_name, metadata::Branch * branch) {
        
    _num_opened_branches.fetch_add(num);
    /* --------------- */
    /* service context */
    /* --------------- */
    if (!service.empty()) {
        // check if service does not exist yet and add node
        if (_service_nodes.count(service) == 0) {
            _service_nodes[service].name = service;
        }

        // validate tag
        std::unique_lock<std::mutex> lock_services(_mutex_service_nodes);
        if (branch->hasTag()) {
            // ABORT - unique tag already exists
            if (_service_nodes[service].tagged_branches.count(branch->getTag()) != 0) {
                _num_opened_branches.fetch_add(-num);
                return false;
            }
            // track on SERVICE TAG
            _service_nodes[service].tagged_branches[branch->getTag()] = branch;
        }

        // add parent node and add current node to the parent's children list
        ServiceNodeStruct * parent_node = &_service_nodes[parent_name];
        _service_nodes[service].parent = parent_node;
        parent_node->children.emplace_back(&_service_nodes[service]);
        // increment opened branches in all parents
        while (parent_node != nullptr) {
            parent_node->num_opened_branches++;
            parent_node = parent_node->parent;
        }

        // track on SERVICE
        _service_nodes[service].num_opened_branches += num;

        // track on REGIONS of SERVICE
        if (regions.size() > 0) {
            std::unique_lock<std::mutex> lock_regions(_mutex_opened_regions);
            for (const auto& region : regions) {
                if (!region.empty()) {
                    _service_nodes[service].opened_regions[region] += 1;
                    _opened_regions[region] += 1;
                }
            }
        }
        // notify upon creation (due to async waits)
        _cond_new_service_nodes.notify_all();
    }

    /* -------------- */
    /* region context */
    /* -------------- */
    else {
        std::unique_lock<std::mutex> lock(_mutex_opened_regions);
        for (const auto& region : regions) {
            if (!region.empty()) {
                _opened_regions[region] += 1;
            }
        }
        // notify upon creation (due to async waits)
        _cond_new_opened_regions.notify_all();
    }
    return true;
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

int Request::waitService(const std::string& service, const std::string& tag, bool async, int timeout) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);

    // --------------
    // CONTEXT CHECKS
    //---------------
    // branch is expected to be asynchronously opened so we need to wait for the branch context
    if (async) {
        while (_service_nodes.count(service) == 0) {
            _cond_new_service_nodes.wait_for(lock, remaining_timeout);
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    // context error checking
    else {
        // no current branch for this service
        if (_service_nodes.count(service) == 0) {
            return -2;
        }
        // no current branch for this service (non async) tag
        if (!tag.empty() && _service_nodes[service].tagged_branches.count(tag) == 0) {
            return -2;
        }
    }

    // ----------
    // WAIT LOGIC
    //-----------
    // tag-specific
    if (!tag.empty()) {
        metadata::Branch * branch = _service_nodes[service].tagged_branches[tag];
        while (!branch->isClosed()) {
            inconsistency = 1;
            _cond_service_nodes.wait_for(lock, remaining_timeout);
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    // overall service
    else {
        int * num_branches_ptr = &(_service_nodes[service].num_opened_branches);
        while (*num_branches_ptr != 0) {
            _cond_service_nodes.wait_for(lock, remaining_timeout);
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    return inconsistency;
}

int Request::waitRegion(const std::string& region, bool async, int timeout) {
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

int Request::waitServiceRegion(const std::string& service, const std::string& region, 
const::std::string& tag, bool async, int timeout) {

    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);

    // --------------
    // CONTEXT CHECKS
    //---------------
    // wait for branch context (service and region)
    if (async) {
        auto service_it = _service_nodes.find(service);
        while (service_it == _service_nodes.end()) {
            _cond_new_service_nodes.wait_for(lock, remaining_timeout);
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
            service_it = _service_nodes.find(service);
        }
        auto opened_regions_it = service_it->second.opened_regions;
        auto region_it = opened_regions_it.find(region);
        while (region_it == opened_regions_it.end()) {
            _cond_new_service_nodes.wait_for(lock, remaining_timeout);
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) return -1;

            region_it = opened_regions_it.find(region);
        }
    }
    // context not found
    else if (_service_nodes.count(service) == 0 || _service_nodes[service].opened_regions.count(region) == 0) {
        return -2;
    }

    // ----------
    // WAIT LOGIC
    //-----------
    // tag-specific
    if (!tag.empty()) {
        metadata::Branch * branch = _service_nodes[service].tagged_branches[tag];
        while (!branch->isClosed(region)) {
            inconsistency = 1;
            _cond_service_nodes.wait_for(lock, remaining_timeout);
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    // overall service
    else {
        int * num_branches_ptr = &_service_nodes[service].opened_regions[region];
        while (*num_branches_ptr != 0) {
            _cond_service_nodes.wait_for(lock, remaining_timeout);
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    return inconsistency;
}

utils::Status Request::getStatus() {
    if (_num_opened_branches.load() == 0) {
        return {CLOSED};
    }
    return {OPENED};
}

utils::Status Request::getStatusRegion(const std::string& region) {
    std::unique_lock<std::mutex> lock(_mutex_opened_regions);
    if (!_opened_regions.count(region)) {
        return {UNKNOWN};
    }
    if (_opened_regions[region] == 0) {
        return {CLOSED};
    }
    return {OPENED};
}

utils::Status Request::getStatusService(const std::string& service, bool detailed) {
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);
    int status;

    // find out if service context exists
    if (!_service_nodes.count(service)) {
        return {UNKNOWN};
    }
    
    // get overall status of request
    if (_service_nodes[service].num_opened_branches == 0) {
        status = CLOSED;
    } else {
        status = OPENED;
    }

    // return if client only wants basic information (status)
    if (!detailed) {
        return {status};
    }

    // otherwise, return detailed information
    utils::Status res;
    res.status = status;
    // get tagged branches within the same service
    for (const auto& branch_it: _service_nodes[service].tagged_branches) {
        res.tagged[branch_it.first] = branch_it.second->getStatus();
    }
    // get children nodes of current service
    for (auto node_it = _service_nodes[service].children.begin(); node_it != _service_nodes[service].children.end(); ++node_it) {
        int status = (*node_it)->num_opened_branches == 0 ? CLOSED : OPENED;
        res.children[(*node_it)->name] = status;
    }
    // get all regions status
    for (const auto& region_it: _service_nodes[service].opened_regions) {
        res.regions[region_it.first] = region_it.second == 0 ? CLOSED : OPENED;
    }
    return res;
}

utils::Status Request::getStatusServiceRegion(const std::string& service, const std::string& region, bool detailed) {
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);
    int status;

    // find out if service context exists
    auto service_it = _service_nodes.find(service);
    if (service_it == _service_nodes.end()) {
        return {UNKNOWN};
    }
    auto region_it = service_it->second.opened_regions.find(region);
    if (region_it == service_it->second.opened_regions.end()) {
        return {UNKNOWN};
    }
    
    // get overall status of request
    if (_service_nodes[service].opened_regions[region] == 0) {
        status = CLOSED;
    } else {
        status = OPENED;
    }

    // return if client only wants basic information (status)
    if (!detailed) {
        return {status};
    }

    // otherwise, return detailed information
    utils::Status res;
    res.status = status;
    for (const auto& branch_it: _service_nodes[service].tagged_branches) {
        res.tagged[branch_it.first] = branch_it.second->getStatus(region);
    }
    // get children nodes of current service
    for (auto node_it = _service_nodes[service].children.begin(); node_it != _service_nodes[service].children.end(); ++node_it) {
        // by default, if region is not found then status is set to UNKNOWN
        int status = UNKNOWN;
        if ((*node_it)->opened_regions.count(region) > 0) {
            status = (*node_it)->opened_regions[region] == 0 ? CLOSED : OPENED;
        }
        res.children[(*node_it)->name] = status;
    }
    return res;
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
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);
    std::map<std::string, int> result = std::map<std::string, int>();
    auto service_it = _service_nodes.find(service);

    // no status found for a given service
    if (service_it == _service_nodes.end()) {
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