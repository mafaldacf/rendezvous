#include "request.h"
#include <mutex>
#include <shared_mutex>

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versions_registry)
    : _rid(rid), _next_bid_index(0), _num_opened_branches(0), _opened_global_region(0), 
    _next_sub_rid_index(1), _versions_registry(versions_registry) {

    _last_ts = std::chrono::system_clock::now();
    // <bid, branch>
    _branches = std::unordered_map<std::string, metadata::Branch*>();
    _service_nodes = std::unordered_map<std::string, ServiceNode*>();
    _sub_requests = oneapi::tbb::concurrent_hash_map<std::string, SubRequest*>();
    _wait_logs = std::map<std::string, int>();

    // add root node
    _service_nodes[""] = new ServiceNode({""});

    // insert the subrequest of the root
    tbb::concurrent_hash_map<std::string, SubRequest*>::accessor write_accessor;
    _sub_requests.insert(write_accessor, "0");
    write_accessor->second = new SubRequest{};
}

Request::~Request() {
    for (const auto& it : _sub_requests) {
        delete it.second;
    }
    for (const auto& it : _branches) {
        delete it.second;
    }
    for (const auto& it : _service_nodes) {
        delete it.second;
    }
    delete _versions_registry;
}

// -----------
// Identifiers
//------------

std::string Request::getRid() {
    return _rid;
}

std::string Request::genId() {
  return std::to_string(_next_bid_index.fetch_add(1));
}

// -----------
// Helpers
//------------

std::chrono::time_point<std::chrono::system_clock> Request::getLastTs() {
    return _last_ts;
}

replicas::VersionRegistry * Request::getVersionsRegistry() {
    return _versions_registry;
}

std::string Request::addNextSubRequest(const std::string& sub_rid) {
    int next_sub_rid_index;
    std::string next_sub_rid;

    // if we are currently in a sub_rid we fetch its next sub_rid
    if (!sub_rid.empty()) {
        tbb::concurrent_hash_map<std::string, SubRequest*>::accessor write_accessor;
        bool found = _sub_requests.find(write_accessor, sub_rid);
        // current sub_rid does not exist
        if (!found) {
            return "";
        }

        // get next sub_rid
        next_sub_rid_index = write_accessor->second->next_sub_rid_index++;
        // parse the index to full string of sub_rid
        next_sub_rid = sub_rid + utils::FULL_ID_DELIMITER + std::to_string(next_sub_rid_index);
    }
    // otherwise we obtain the next sub_rid from current request
    else {
        next_sub_rid_index = _next_sub_rid_index.fetch_add(1);
        // parse the index to full string of sub_rid
        next_sub_rid = std::to_string(next_sub_rid_index);
    }

    // insert the next subrequest and return its id
    tbb::concurrent_hash_map<std::string, SubRequest*>::accessor write_accessor;
    _sub_requests.insert(write_accessor, next_sub_rid);
    write_accessor->second = new SubRequest{};
    return next_sub_rid;
}

// ---------------------
// Core Rendezvous Logic
//----------------------

metadata::Branch * Request::registerBranch(const std::string& sub_rid, const std::string& bid, const std::string& service,  
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
    _branches[bid] = branch;
    lock.unlock();

    // error tracking branch (tag already exists!)
    if (!trackBranch(sub_rid, service, regions, num, prev_service, branch)) {
        lock.lock();
        _branches.erase(bid);
        delete branch;
        lock.unlock();
        return nullptr;
    }

    return branch;
}

int Request::closeBranch(const std::string& sub_rid, const std::string& bid, const std::string& region) {
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
    lock.unlock();
    if (r == 1) {
        if (!untrackBranch(sub_rid, service, region, globally_closed)) {
            r = -1;
        }
    }
    return r;
}

bool Request::untrackBranch(const std::string& sub_rid, const std::string& service, 
    const std::string& region, bool globally_closed) {

    // ---------------------------
    // SERVICE NODE & DEPENDENCIES
    // ---------------------------
    std::unique_lock<std::mutex> lock_services(_mutex_service_nodes);
    // decrease opened branches in all direct parents (start with the current one)
    if (region.empty()) {
        for (ServiceNode * parent_node = _service_nodes[service]; parent_node != nullptr; parent_node = parent_node->parent) {
            // by default, empty region means global region => we can decrement the entire branch
            parent_node->num_opened_branches--;
            parent_node->opened_global_region--;

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
    lock_services.unlock();

    // ----------
    // SUBREQUEST
    // ----------
    // althought we are modifying subrequests, this shared lock is used for
    // controlling concurrency with wait calls (they use unique_lock over this mutex)
    // hence, the following code is blocked until the wait stops reading
    // this is to ensure that both region trackers and subrequest region trackers
    // are observed at the same time in the wait calls, while ensuring fine-grained lock with tbb lib
    // when using a shared_lock here

    std::shared_lock<std::shared_mutex> lock_subrequests(_mutex_subrequests);

    if (!sub_rid.empty()) {
        tbb::concurrent_hash_map<std::string, SubRequest*>::const_accessor read_accessor;
        bool found = _sub_requests.find(read_accessor, sub_rid);

        // sanity check, should never happen
        if (!found) return false;

        SubRequest * subrequest = read_accessor->second;
        read_accessor.release();

        if (globally_closed) {
            subrequest->opened_branches.fetch_add(-1);
        }

        if (region.empty()) {
            subrequest->opened_global_region.fetch_add(-1);
        }
        else {
            tbb::concurrent_hash_map<std::string, int>::accessor write_accessor;
            bool found = subrequest->opened_regions.find(write_accessor, region);
            // sanity check (must always be found)
            if (found) {
                write_accessor->second--;
            }
        }
    }

    // ------------
    // REGIONS ONLY (REMINDER: needs to be placed after tracking subrequests to notify all threads)
    // ------------
    if (!region.empty()) {
        std::unique_lock<std::mutex> lock_regions(_mutex_regions);
        _opened_regions[region]--;
    }
    else {
        _opened_global_region.fetch_add(-1);
    }
    if (globally_closed) {
        _num_opened_branches.fetch_add(-1);
    }
    _cond_subrequests.notify_all();

    return true;
}

bool Request::trackBranch(const std::string& sub_rid, const std::string& service, 
    const utils::ProtoVec& regions, int num, const std::string& prev_service, metadata::Branch * branch) {

    // ---------------------------
    // SERVICE NODE & DEPENDENCIES
    // ---------------------------
    std::unique_lock<std::mutex> lock_services(_mutex_service_nodes);
    
    // ABORT: prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
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
            return false;
        }
        // track on SERVICE TAG
        _service_nodes[service]->tagged_branches[branch->getTag()] = branch;
    }
    
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
            parent_node->opened_global_region++;
        }
    }

    // notify upon creation (due to async waits)
    _cond_new_service_nodes.notify_all();
    lock_services.unlock();

    // ----------
    // SUBREQUEST
    // ----------
    if (!sub_rid.empty()) {
        tbb::concurrent_hash_map<std::string, SubRequest*>::const_accessor read_accessor;
        bool found = _sub_requests.find(read_accessor, sub_rid);

        // sanity check >> should never happen
        if (!found) {
            return false;
        }

        SubRequest * subrequest = read_accessor->second;
        read_accessor.release();
        subrequest->opened_branches.fetch_add(1);

        tbb::concurrent_hash_map<std::string, int>::accessor write_accessor;
        for (const auto& region: regions) {
            bool new_key = subrequest->opened_regions.insert(write_accessor, region);
            if (new_key) {
                write_accessor->second = 1;
            }
            else {
                write_accessor->second++;
            }
        }
        write_accessor.release();
        // if no regions are provided we also increment globally
        if (regions.size() == 0) {
            subrequest->opened_global_region.fetch_add(1);
        }
    }

    // ------------
    // REGIONS ONLY (REMINDER: needs to be placed after tracking subrequests to notify all threads)
    // ------------
    std::unique_lock<std::mutex> lock(_mutex_branches);
    std::unique_lock<std::mutex> lock_regions(_mutex_regions);
    for (const auto& region: regions) {
        _opened_regions[region]++;
    }
    if (regions.size() == 0) {
        _opened_global_region.fetch_add(1);
    }
    _num_opened_branches.fetch_add(1);
    lock.unlock();
    lock_regions.unlock();

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

void Request::_addToWaitLogs(const std::string& sub_rid) {
    // REMINDER: the function that calls this method already acquires lock on _mutex_subrequests

    std::unique_lock<std::shared_mutex> lock(_mutex_wait_logs);
    _wait_logs[sub_rid]++;

    // notify regarding new wait logs to cover an edge case:
    // subrid (a) registered before subrid (b), but (b) does wait call before (a) and both with branches opened
    // (b) needs to be signaled and update its preceeding list to discard (a) from the wait call
    _cond_subrequests.notify_all();
}

void Request::_removeFromWaitLogs(const std::string& sub_rid) {
    // REMINDER: the function that calls this method already acquires lock on _mutex_subrequests

    std::unique_lock<std::shared_mutex> lock(_mutex_wait_logs);
    // remove key from map since this is the last wait entry
    if (_wait_logs[sub_rid] == 1) {
        _wait_logs.erase(sub_rid);
    }
    else {
        _wait_logs[sub_rid]--;
    }
}

std::vector<std::string> Request::_getPrecedingWaitLogsEntries(const std::string& sub_rid) {
    // REMINDER: the function that calls this method already acquires lock on _mutex_subrequests

    std::vector<std::string> entries;
    std::shared_lock<std::shared_mutex> lock(_mutex_wait_logs);
    for (auto it = _wait_logs.begin(); it != _wait_logs.end(); it++) {
        // stop since we are dealing with an ordered map
        if (it->first >= sub_rid) {
            break;
        }
        entries.emplace_back(it->first);
    }
    return entries;
}

int Request::_openedBranchesPrecedingSubRids(const std::vector<std::string>& sub_rids) {
    // REMINDER: the function that calls this method already acquires lock on _mutex_subrequests

    int num = 0;
    tbb::concurrent_hash_map<std::string, SubRequest*>::const_accessor read_accessor;
    for (const auto& sub_rid: sub_rids) {
        bool found = _sub_requests.find(read_accessor, sub_rid);
        // sanity check
        if (!found) continue;
        SubRequest * subrequest = read_accessor->second;
        num += subrequest->opened_branches.load();

    }
    read_accessor.release();
    return num;
}

std::pair<int, int> Request::_openedBranchesRegionPrecedingSubRids(
    const std::vector<std::string>& sub_rids, const std::string& region) {
    // REMINDER: the function that calls this method already acquires lock!

    // <global region counter, current region counter>
    std::pair<int, int> num = {0, 0};
    tbb::concurrent_hash_map<std::string, SubRequest*>::const_accessor read_accessor_sr;
    tbb::concurrent_hash_map<std::string, int>::const_accessor read_accessor_num;
    for (const auto& sub_rid: sub_rids) {
        bool found = _sub_requests.find(read_accessor_sr, sub_rid);

        // sanity check
        if (!found) continue;
        SubRequest * subrequest = read_accessor_sr->second;

        // get number of opened branches globally, in terms of regions
        num.second += subrequest->opened_global_region.load();

        // get number of opened branches for this region
        found = subrequest->opened_regions.find(read_accessor_num, region);
        if (!found) continue;
        num.second += read_accessor_num->second;
    }
    return num;
}

int Request::wait(const std::string& sub_rid, std::string prev_service, int timeout) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);

    // -----------------------------------
    //           VALIDATIONS
    // -----------------------------------

    // prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
        return -3;
    }

    tbb::concurrent_hash_map<std::string, SubRequest*>::const_accessor read_accessor;
    bool found = _sub_requests.find(read_accessor, sub_rid);
    // subrequest does not exist - error propagating rid by client
    if (!found) {
        return -4;
    }

    SubRequest * subrequest = read_accessor->second;
    read_accessor.release();


    // -----------------------------------
    //           CORE WAIT LOGIC
    // -----------------------------------
    std::unique_lock<std::shared_mutex> lock(_mutex_subrequests);
    _addToWaitLogs(sub_rid);
    while (true) {
        // get number of branches to ignore from preceding subrids in the wait logs
        const auto& preceding_subrids = _getPrecedingWaitLogsEntries(sub_rid);
        int offset = _openedBranchesPrecedingSubRids(preceding_subrids);

        if (_num_opened_branches.load() - subrequest->opened_branches.load() - offset != 0) {
            _cond_subrequests.wait_for(lock, std::chrono::seconds(remaining_timeout));
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                _removeFromWaitLogs(sub_rid);
                return -1;
            }
        }
        else {
            break;
        }
    }
    _removeFromWaitLogs(sub_rid);

    // -----------------------------------
    // TRANSVERSE DEPENDENCIES ALGORITHM:
    // - only wait for top/left neighboors
    // - discard direct parents
    // -----------------------------------
    /* std::unique_lock<std::mutex> lock(_mutex_service_nodes);
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
    } while (parent != nullptr); */
    
    return inconsistency;
}

int Request::waitRegion(const std::string& sub_rid, const std::string& region, 
    std::string prev_service, bool async, int timeout) {
        
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_timeout = _computeRemainingTimeout(timeout, start_time);

    // -----------------------------------
    //           VALIDATIONS
    // -----------------------------------

    // prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
        return -3;
    }

    std::unique_lock<std::shared_mutex> lock(_mutex_subrequests);

    // wait for creation
    if (async) {
        while (_opened_regions.count(region) == 0) {
            _cond_subrequests.wait_for(lock, std::chrono::seconds(remaining_timeout));
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    else {
        if (_opened_regions.count(region) == 0) {
            return 0;
        }
    }

    tbb::concurrent_hash_map<std::string, SubRequest*>::const_accessor read_accessor_sr;
    bool found = _sub_requests.find(read_accessor_sr, sub_rid);
    // subrequest does not exist - error propagating rid by client
    if (!found) {
        return -4;
    }

    SubRequest * subrequest = read_accessor_sr->second;
    read_accessor_sr.release();

    // -----------------------------------
    //           CORE WAIT LOGIC
    // -----------------------------------
    tbb::concurrent_hash_map<std::string, int>::const_accessor read_accessor_num;
    _addToWaitLogs(sub_rid);
    while(true) {
        // get counters (region and globally, in terms of region) for current sub request
        int opened_sub_request_global_region = subrequest->opened_global_region.load();
        bool found = subrequest->opened_regions.find(read_accessor_num, region);
        int opened_sub_request_region = found ? read_accessor_num->second : 0;

        // get number of branches to ignore from preceding subrids in the wait logs
        const auto& preceding_subrids = _getPrecedingWaitLogsEntries(sub_rid);
        std::pair<int, int> offset = _openedBranchesRegionPrecedingSubRids(preceding_subrids, region);

        if (_opened_global_region.load() - opened_sub_request_global_region - offset.first != 0 
            || _opened_regions[region] - opened_sub_request_region - offset.second != 0) {

            read_accessor_num.release();
            _cond_subrequests.wait_for(lock, std::chrono::seconds(remaining_timeout));
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                _removeFromWaitLogs(sub_rid);
                return -1;
            }
        }
        else {
            break;
        }
    }
    _removeFromWaitLogs(sub_rid);

    // -----------------------------------
    // TRANSVERSE DEPENDENCIES ALGORITHM:
    // - only wait for top/left neighboors
    // - discard direct parents
    // -----------------------------------
    /* ServiceNode * stop = nullptr;
    ServiceNode * parent = _service_nodes[prev_service];
    do {
        for (auto it = parent->children.begin(); it != parent->children.end(); it++) {
            // break if we reach previous checkpoint
            if (*it == stop) {
                break;
            }
            // check if region exists
            if ((*it)->opened_regions.count(region) != 0) {
                int * opened_global_region_ptr = &((*it)->opened_global_region);
                int * num_opened_regions_ptr = &((*it)->opened_regions[region]);
                // wait logic (need to check global branch that encompasses all regions!)
                while (*opened_global_region_ptr != 0 || *num_opened_regions_ptr != 0) {
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
                int * opened_global_region_ptr = &((*it)->opened_global_region);
                while (*opened_global_region_ptr != 0) {
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
    } while (parent != nullptr); */

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

utils::Status Request::checkStatus(const std::string& sub_rid, const std::string& prev_service) {
    utils::Status res {UNKNOWN};

    // prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
        res.status = INVALID_CONTEXT;
        return res;
    }

    tbb::concurrent_hash_map<std::string, SubRequest*>::const_accessor read_accessor;
    bool found = _sub_requests.find(read_accessor, sub_rid);
    // subrequest does not exist - error propagating rid by client
    if (!found) {
        res.status = INVALID_CONTEXT;
        return res;
    }

    SubRequest * subrequest = read_accessor->second;
    read_accessor.release();

    std::unique_lock<std::mutex> lock(_mutex_branches);
    if (_num_opened_branches.load() > subrequest->opened_branches.load()) {
        res.status = OPENED;
    }
    else {
        res.status = CLOSED;
    }

    // -----------------------------------
    // TRANSVERSE DEPENDENCIES ALGORITHM:
    // - only wait for top/left neighboors
    // - discard direct parents
    // -----------------------------------
    /* ServiceNode * stop = nullptr;
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
    } while (parent != nullptr); */

    return res;
}

utils::Status Request::checkStatusRegion(const std::string& sub_rid, const std::string& region, const std::string& prev_service) {
    utils::Status res {UNKNOWN};

    // prev service does not exist - error propagating context by client
    if (_service_nodes.count(prev_service) == 0) {
        res.status = INVALID_CONTEXT;
        return res;
    }

    if (_opened_regions.count(region) == 0) {
        return res;
    }

    tbb::concurrent_hash_map<std::string, SubRequest*>::const_accessor read_accessor_sr;
    bool found = _sub_requests.find(read_accessor_sr, sub_rid);
    // subrequest does not exist - error propagating rid by client
    if (!found) {
        res.status = INVALID_CONTEXT;
        return res;
    }

    SubRequest * subrequest = read_accessor_sr->second;
    read_accessor_sr.release();

    tbb::concurrent_hash_map<std::string, int>::const_accessor read_accessor_num;
    std::unique_lock<std::mutex> lock(_mutex_regions);

    // get counters (region and globally) for current sub request
    int sr_global_region = subrequest->opened_global_region.load();
    found = subrequest->opened_regions.find(read_accessor_num, region);
    int sr_region = found ? read_accessor_num->second : 0;

    if (_opened_regions[region] != sr_region || _opened_global_region.load() != sr_global_region) {
        res.status = OPENED;
    }
    else {
        res.status = CLOSED;
    }

    // -----------------------------------
    // TRANSVERSE DEPENDENCIES ALGORITHM:
    // - only wait for top/left neighboors
    // - discard direct parents
    // -----------------------------------

    /* ServiceNode * stop = nullptr;
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
                if ((*it)->opened_regions[region] != 0 || (*it)->opened_global_region != 0) {
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
                if ((*it)->opened_global_region != 0) {
                    res.status = OPENED;
                    return res;
                }
            }
        }
        stop = parent;
        parent = parent->parent;
    } while (parent != nullptr); */

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