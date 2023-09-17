#include "request.h"

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versions_registry)
    : _rid(rid), _next_id(0), _num_opened_branches(0), _versions_registry(versions_registry) {
    _last_ts = std::chrono::system_clock::now();
    // <bid, branch>
    _branches = std::unordered_map<std::string, metadata::Branch*>();
    _service_nodes = std::unordered_map<std::string, ServiceNode*>();

    // add root node
    _service_nodes[""] = new ServiceNode({""});
}

Request::~Request() {
    for (const auto& branch_it : _branches) {
        delete branch_it.second;
    }
    for (const auto& service_node_it : _service_nodes) {
        delete service_node_it.second;
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
    const std::string& tag, const utils::ProtoVec& regions, const std::string& prev_service) {

    std::unique_lock<std::mutex> lock(_mutex_branches);
    auto branch_it = _branches.find(bid);

    // branches already exist
    if (branch_it != _branches.end()) {
        return nullptr;
    }

    metadata::Branch * branch;
    int num = regions.size();

    // branch with specified regions
    if (num > 0) {
        branch = new metadata::Branch(service, tag, regions);
    }

    // no region specified - global region
    else {
        num = 1;
        branch = new metadata::Branch(service, tag);
    }

    // error tracking branch (tag already exists!)
    if (!trackBranch(service, regions, num, prev_service, branch)) {
        spdlog::error("Error creating branch for service = {} and #{} regions", service, num);
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
    bool globally_closed = branch->isClosed();
    if (r == 1) {
        untrackBranch(service, region, globally_closed);
        _cond_branches.notify_all();
    }
    else if (r == -1) {
        spdlog::error("Error closing branch for service = {} on region = {}", service, region);
    }
    return r;
}

bool Request::untrackBranch(const std::string& service, const std::string& region, bool globally_closed) {
    _num_opened_branches.fetch_add(-1);

    std::unique_lock<std::mutex> lock(_mutex_service_nodes);

    // decrease opened branches in all direct parents (start with the current one)
    if (region.empty()) {
        for (ServiceNode * parent_node = _service_nodes[service]; parent_node != nullptr; parent_node = parent_node->parent) {
            // by default, empty region means global region => we can decrement the entire branch
            parent_node->num_opened_branches--;
            parent_node->global_region--;

        }
    }
    else {
        for (ServiceNode * parent_node = _service_nodes[service]; parent_node != nullptr; parent_node = parent_node->parent) {
            if (globally_closed) {
                parent_node->num_opened_branches--;
            }
            parent_node->opened_regions[region]--;

        }
    }

    // notify creation of new branch
    _cond_service_nodes.notify_all();

    return true;
}

bool Request::trackBranch(const std::string& service, const utils::ProtoVec& regions, int num, 
    const std::string& prev_service, metadata::Branch * branch) {

    _num_opened_branches.fetch_add(num);

    std::unique_lock<std::mutex> lock_services(_mutex_service_nodes);
    
    // ABORT: prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
        _num_opened_branches.fetch_add(-num);
        return false;
    }
    // create new node if service does not exist yet
    if (_service_nodes.count(service) == 0) {
        _service_nodes[service] = new ServiceNode({service});
    }

    // validate tag
    if (branch->hasTag()) {
        // ABORT - unique tag already exists
        if (_service_nodes[service]->tagged_branches.count(branch->getTag()) != 0) {
            _num_opened_branches.fetch_add(-num);
            return false;
        }
        // track on SERVICE TAG
        _service_nodes[service]->tagged_branches[branch->getTag()] = branch;
    }

    // ----------------------
    // dependencies tracking
    // ----------------------
    ServiceNode * parent_node;
    // current node is the first child
    if (_service_nodes[service]->parent == nullptr) {
        parent_node = _service_nodes[prev_service];
        _service_nodes[service]->parent = parent_node;
        parent_node->children.emplace_back(_service_nodes[service]);
    }

    // track on REGIONS of SERVICE
    if (regions.size() > 0) {
        // increment REGIONS in all direct parents
        for (parent_node = _service_nodes[service]; parent_node != nullptr; parent_node = parent_node->parent) {
            parent_node->num_opened_branches++;
            for (const auto& region: regions) {
                parent_node->opened_regions[region]++;
            }
        }
    }
    // track on GLOBAL REGION of SERVICE
    else {
        // increment GLOBAL REGION in all direct parents
        for (parent_node = _service_nodes[service]; parent_node != nullptr; parent_node = parent_node->parent) {
            parent_node->num_opened_branches++;
            parent_node->global_region++;
        }
    }

    // notify upon creation (due to async waits)
    _cond_new_service_nodes.notify_all();

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

int Request::wait(std::string prev_service, int timeout) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);

    // prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
        return -3;
    }

    // transverse parents algorithm:
    // - only wait for top/left neighboors
    // - discard direct parents
    ServiceNode * stop = nullptr;
    ServiceNode * parent = _service_nodes[prev_service];
    do { 
        for (auto it = parent->children.begin(); it != parent->children.end(); it++) {
            // break if we reach previous checkpoint
            if (*it == stop) {
                break;
            }
            // wait for branch
            while ((*it)->num_opened_branches != 0) {
                _cond_service_nodes.wait_for(lock, std::chrono::seconds(remaining_timeout));
                inconsistency = 1;
                remaining_timeout = _computeRemainingTimeout(timeout, start_time);
                if (remaining_timeout <= std::chrono::seconds(0)) {
                    return -1;
                }
            }
        }
        stop = parent;
        parent = parent->parent;
    } while (parent != nullptr);
    
    return inconsistency;
}

int Request::waitRegion(const std::string& region, std::string prev_service, bool async, int timeout) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);

    // prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
        return -3;
    }

    // transverse parents algorithm (similar to the global wait):
    // - only wait for top/left neighboors
    // - discard direct parents
    ServiceNode * stop = nullptr;
    ServiceNode * parent = _service_nodes[prev_service];
    do {
        for (auto it = parent->children.begin(); it != parent->children.end(); it++) {
            // break if we reach previous checkpoint
            if (*it == stop) {
                break;
            }
            // check if region exists
            if ((*it)->opened_regions.count(region) != 0) {
                int * global_region_ptr = &((*it)->global_region);
                int * num_opened_regions_ptr = &((*it)->opened_regions[region]);
                // wait logic (need to check global branch that encompasses all regions!)
                while (*global_region_ptr != 0 || *num_opened_regions_ptr != 0) {
                    _cond_service_nodes.wait_for(lock, std::chrono::seconds(remaining_timeout));
                    inconsistency = 1;
                    remaining_timeout = _computeRemainingTimeout(timeout, start_time);
                    if (remaining_timeout <= std::chrono::seconds(0)) {
                        return -1;
                    }
                }
            }
            // if region does not exist, we still need to check global branch that encompasses all regions!
            else {
                int * global_region_ptr = &((*it)->global_region);
                while (*global_region_ptr != 0) {
                    _cond_service_nodes.wait_for(lock, std::chrono::seconds(remaining_timeout));
                    inconsistency = 1;
                    remaining_timeout = _computeRemainingTimeout(timeout, start_time);
                    if (remaining_timeout <= std::chrono::seconds(0)) {
                        return -1;
                    }
                }
            }
        }
        stop = parent;
        parent = parent->parent;
    } while (parent != nullptr);

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
        if (!tag.empty() && _service_nodes[service]->tagged_branches.count(tag) == 0) {
            return -2;
        }
    }

    // ----------
    // WAIT LOGIC
    //-----------
    // tag-specific
    if (!tag.empty()) {
        metadata::Branch * branch = _service_nodes[service]->tagged_branches[tag];
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
        int * num_branches_ptr = &(_service_nodes[service]->num_opened_branches);
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
        auto opened_regions_it = service_it->second->opened_regions;
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
    else if (_service_nodes.count(service) == 0 || _service_nodes[service]->opened_regions.count(region) == 0) {
        return -2;
    }
    // tag not found
    else if (!tag.empty() && _service_nodes[service]->tagged_branches.count(tag) == 0) {
        return -4;
    }

    // ----------
    // WAIT LOGIC
    //-----------
    // tag-specific
    if (!tag.empty()) {
        metadata::Branch * branch = _service_nodes[service]->tagged_branches[tag];
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
        int * num_branches_ptr = &_service_nodes[service]->opened_regions[region];
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

utils::Status Request::checkStatus(const std::string& prev_service) {
    utils::Status res;
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);

    // prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
        res.status = INVALID_CONTEXT;
        return res;
    }

    res.status = CLOSED;

    // transverse parents algorithm:
    // - only check top/left neighboors
    // - discard direct parents
    ServiceNode * stop = nullptr;
    ServiceNode * parent = _service_nodes[prev_service];
    do {
        for (auto it = parent->children.begin(); it != parent->children.end(); it++) {
            // we reached last checkpoint
            if (*it == stop) {
                break;
            }
            // ------------------
            // CHECK STATUS LOGIC
            // - return OPENED if at least one branch is opened
            // - otherwise, we keep iterating to make sure every branch is CLOSED
            // ------------------
            if ((*it)->num_opened_branches != 0) {
                res.status = OPENED;
                return res;
            }
        }
        stop = parent;
        parent = parent->parent;
    } while (parent != nullptr);

    return res;
}

utils::Status Request::checkStatusRegion(const std::string& region, const std::string& prev_service) {
    utils::Status res;
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);

    // prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
        res.status = INVALID_CONTEXT;
        return res;
    }
    res.status = UNKNOWN;

    // transverse parents algorithm:
    // - only check top/left neighboors
    // - discard direct parents

    ServiceNode * stop = nullptr;
    ServiceNode * parent = _service_nodes[prev_service];
    do {
        for (auto it = parent->children.begin(); it != parent->children.end(); it++) {
            // we reached last checkpoint
            if (*it == stop) {
                break;
            }

            // ------------------
            // CHECK STATUS LOGIC
            // - return OPENED if at least one branch is opened
            // - otherwise, we keep iterating to make sure every branch is CLOSED
            // ------------------

            // check if region exists (still need to check global branch that encompasses all regions!)
            if ((*it)->opened_regions.count(region) != 0) {
                if ((*it)->opened_regions[region] != 0 || (*it)->global_region != 0) {
                    res.status = OPENED;
                    return res;
                }
                // we force to CLOSED everytime since default value is UNKNOWN
                res.status = CLOSED;
            }
            // if region does not exist, we still need to check global branch that encompasses all regions!
            else {
                // if global region is not open it can also mean that there
                // is no global region => we mantain the UNKNOWN res.status (do not set to CLOSED)
                if ((*it)->global_region != 0) {
                    res.status = OPENED;
                    return res;
                }
            }
        }
        stop = parent;
        parent = parent->parent;
    } while (parent != nullptr);

    return res;
}

utils::Status Request::checkStatusService(const std::string& service, bool detailed) {
    utils::Status res;
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);

    // find out if service context exists
    if (_service_nodes.count(service) == 0) {
        res.status = UNKNOWN;
        return res;
    }
    
    // get overall status of request
    if (_service_nodes[service]->num_opened_branches == 0) {
        res.status = CLOSED;
    } else {
        res.status = OPENED;
    }

    // return if client only wants basic information (status)
    if (!detailed) {
        return res;
    }

    // otherwise, return detailed information
    // get tagged branches within the same service
    for (const auto& branch_it: _service_nodes[service]->tagged_branches) {
        res.tagged[branch_it.first] = branch_it.second->getStatus();
    }
    // get all regions status
    for (const auto& region_it: _service_nodes[service]->opened_regions) {
        res.regions[region_it.first] = region_it.second == 0 ? CLOSED : OPENED;
    }
    return res;
}

utils::Status Request::checkStatusServiceRegion(const std::string& service, const std::string& region, bool detailed) {
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);
    utils::Status res;

    // find out if service context exists
    auto service_it = _service_nodes.find(service);
    if (service_it == _service_nodes.end()) {
        res.status = UNKNOWN;
        return res;
    }
    auto region_it = service_it->second->opened_regions.find(region);
    if (region_it == service_it->second->opened_regions.end()) {
        res.status = UNKNOWN;
        return res;
    }
    
    // get overall status of request
    if (_service_nodes[service]->opened_regions[region] == 0) {
        res.status = CLOSED;
    } else {
        res.status = OPENED;
    }

    // return if client only wants basic information (status)
    if (!detailed) {
        return res;
    }

    // otherwise, return detailed information
    for (const auto& branch_it: _service_nodes[service]->tagged_branches) {
        res.tagged[branch_it.first] = branch_it.second->getStatus(region);
    }
    return res;
}

utils::Dependencies Request::fetchDependencies(const std::string& prev_service) {
    utils::Dependencies result {0};
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);
    // find out if context is valid
    if (_service_nodes.count(prev_service) == 0) {
        result.res = INVALID_CONTEXT;
        return result;
    }
    // get children nodes of current service
    for (auto it = _service_nodes[prev_service]->children.begin(); it != _service_nodes[prev_service]->children.end(); ++it) {
        result.deps.emplace_back((*it)->name);
    }
    return result;
}

utils::Dependencies Request::fetchDependenciesService(const std::string& service) {
    utils::Dependencies result {0};
    std::unique_lock<std::mutex> lock(_mutex_service_nodes);

    if (_service_nodes.count(service) == 0) {
        result.res = INVALID_SERVICE;
        return result;
    }

    // get children nodes of current service
    for (auto it = _service_nodes[service]->children.begin(); it != _service_nodes[service]->children.end(); ++it) {
        result.deps.emplace_back((*it)->name);
    }
    return result;
}