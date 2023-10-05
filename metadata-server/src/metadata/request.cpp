#include "request.h"
#include <mutex>
#include <shared_mutex>

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versions_registry)
    : _rid(rid), _next_bid_index(0), _num_opened_branches(0), _opened_global_region(0), 
    _next_sub_rid_index(1), sub_requests_i(1), _versions_registry(versions_registry) {

    _last_ts = std::chrono::system_clock::now();
    // <bid, branch>
    _branches = std::unordered_map<std::string, metadata::Branch*>();
    _service_nodes = std::unordered_map<std::string, ServiceNode*>();
    _sub_requests = oneapi::tbb::concurrent_hash_map<std::string, SubRequest*>();
    _wait_logs = std::unordered_set<SubRequest*>();

    // add root node
    _service_nodes[""] = new ServiceNode({""});

    // insert the subrequest of the root
    tbb::concurrent_hash_map<std::string, SubRequest*>::accessor write_accessor;
    _sub_requests.insert(write_accessor, utils::ROOT_ASYNC_ZONE_ID);
    write_accessor->second = new SubRequest{0, utils::ROOT_ASYNC_ZONE_ID};
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

std::string Request::addNextSubRequest(const std::string& sid, const std::string& sub_rid, bool gen_id) {
    int next_sub_rid_index;
    std::string next_sub_rid;

    // if we are currently in a sub_rid we fetch its next sub_rid
    if (gen_id) {
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
            next_sub_rid = sub_rid;
        }
        // otherwise we obtain the next sub_rid from current request
        else {
            next_sub_rid_index = _next_sub_rid_index.fetch_add(1);
            // parse the index to full string of sub_rid
            next_sub_rid = "root";
        }
        next_sub_rid += utils::FULL_ID_DELIMITER + sid + std::to_string(next_sub_rid_index);
    }
    // skip the id generation step
    else {
        next_sub_rid = sub_rid;
    }

    // sanity check
    if (next_sub_rid != utils::ROOT_ASYNC_ZONE_ID) {
        // insert the next subrequest and return its id
        tbb::concurrent_hash_map<std::string, SubRequest*>::accessor write_accessor;
        _sub_requests.insert(write_accessor, next_sub_rid);
        write_accessor->second = new SubRequest{sub_requests_i.fetch_add(1), next_sub_rid};
    }

    return next_sub_rid;
}

metadata::Request::SubRequest * Request::_validateSubRid(const std::string& sub_rid) {
    tbb::concurrent_hash_map<std::string, SubRequest*>::const_accessor read_accessor;
    bool found = _sub_requests.find(read_accessor, sub_rid);
    if (found) return read_accessor->second;
    return nullptr;
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
        branch = new metadata::Branch(service, tag, sub_rid, regions);
    }

    // no region specified - global region
    else {
        num = 1;
        branch = new metadata::Branch(service, tag, sub_rid);
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

int Request::closeBranch(const std::string& bid, const std::string& region) {
    std::unique_lock<std::mutex> lock(_mutex_branches);
    bool region_found = true;
    auto branch_it = _branches.find(bid);

    // branch not found
    if (branch_it == _branches.end()) {
        return -1;
    }
    metadata::Branch * branch = branch_it->second;

    const std::string& sub_rid = branch->getSubRid();

    int r = branch->close(region);
    bool globally_closed = branch->isClosed();
    lock.unlock();
    if (r == 1) {
        const std::string& service = branch->getService();
        const std::string& tag = branch->getTag();

        // error in sub_requests tbb map
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
    std::unique_lock<std::shared_mutex> lock_services(_mutex_service_nodes);
    // decrease opened branches in all direct parents (start with the current one)
    if (region.empty()) {
        for (ServiceNode * parent_node = _service_nodes[service]; parent_node != nullptr; parent_node = parent_node->parent) {
            // by default, empty region means global region => we can decrement the entire branch
            parent_node->opened_branches--;
            parent_node->opened_global_region--;

        }
    }
    else {
        for (ServiceNode * parent_node = _service_nodes[service]; parent_node != nullptr; parent_node = parent_node->parent) {
            if (globally_closed) {
                parent_node->opened_branches--;
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
        SubRequest * subrequest = _validateSubRid(sub_rid);
        if (subrequest == nullptr) return false;

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
    std::unique_lock<std::shared_mutex> lock_services(_mutex_service_nodes);
    
    if (_service_nodes.count(prev_service) == 0) return false;

    // create new node if service does not exist yet
    // SERVICES CANNOT HAVE EMPTY NAME otherwise we create a loop when adding parent
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
            parent_node->opened_branches++;
            for (const auto& region: regions) {
                parent_node->opened_regions[region]++;
            }
        }
    }
    // track on GLOBAL REGION of SERVICE
    else {
        // increment GLOBAL REGION in all direct parents
        for (parent_node = _service_nodes[service]; parent_node != nullptr; parent_node = parent_node->parent) {
            parent_node->opened_branches++;
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
        SubRequest * subrequest = _validateSubRid(sub_rid);
        if (subrequest == nullptr) return false;
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

void Request::_addToWaitLogs(SubRequest* subrequest) {
    // REMINDER: the function that calls this method already acquires lock on _mutex_subrequests

    // try to insert if not yet done
    _wait_logs.insert(subrequest);
    subrequest->num_current_waits++;

    // notify regarding new wait logs to cover an >>> EDGE CASE <<<:
    // subrid (a) registered before subrid (b), but (b) does wait call before (a) and both with branches opened
    // (b) needs to be signaled and update its preceeding list to discard (a) from the wait call
    _cond_subrequests.notify_all();
}

void Request::_removeFromWaitLogs(SubRequest* subrequest) {
    // REMINDER: the function that calls this method already acquires lock on _mutex_subrequests

    int n = --subrequest->num_current_waits;
    if (n == 0) {
        _wait_logs.erase(subrequest);
    }
}

// Examples of IDs: 
// eu0:eu0 vs eu0:eu1
// eu0:us0 vs eu0:ap0
// eu0:eu0 vs eu0:eu0:eu0

bool Request::_isPrecedingAsyncZone(SubRequest* subrequest_1, SubRequest* subrequest_2) {
    std::vector<std::string> sub_zone(2), sid(2);
    std::vector<size_t> local_pos(2), global_pos(2);
    std::vector<std::string> id {subrequest_1->sub_rid, subrequest_2->sub_rid};
    std::vector<int> idx {subrequest_1->i, subrequest_2->i};

    // remove 'root' id
    for (int i = 0; i < 2; i++) {
        local_pos[i] = id[i].find(utils::FULL_ID_DELIMITER);
        if (local_pos[i] != std::string::npos) {
            id[i] = id[i].substr(local_pos[i] + 1);
        }
    }
    
    while (true) {
        // reached the same async zone
        // return false since branches within the same async zone are later ignored by default
        if (local_pos[0] == std::string::npos && local_pos[1] == std::string::npos) return false;
        // reached end of first zone
        // first zone is the parent, so we want to ignore the first in the wait logic
        else if (local_pos[0] == std::string::npos && local_pos[1] != std::string::npos) return true;
        // reached end of second zone
        // first zone is the child, so we want to include the first n the wait logic
        else if (local_pos[0] != std::string::npos && local_pos[1] == std::string::npos) return false;

        // parse id: <sid>:<sub_zone>:<remaining of next id>
        // otherwise, we are at the end
        for (int i = 0; i < 2; i++) {
            local_pos[i] = id[i].find(utils::FULL_ID_DELIMITER, global_pos[i]);
            sid[i] = id[i].substr(global_pos[i], utils::SIZE_SIDS);
            if (local_pos[i] != std::string::npos) {
                sub_zone[i] = id[i].substr(global_pos[i] + utils::SIZE_SIDS, local_pos[i] - utils::SIZE_SIDS);
                global_pos[i] += local_pos[i] + 1;
            }
            else {
                sub_zone[i] = id[i].substr(global_pos[i] + utils::SIZE_SIDS);
            }
        }

        // if SIDs are different, compare by index of insertion of the DIRECT PARENT
        if (sid[0] != sid[1]) {
            std::string async_zone_parent_1 = id[0].substr(0, global_pos[0]);
            std::string async_zone_parent_2 = id[1].substr(0, global_pos[1]);
            SubRequest * sub_request_parent_1 = _validateSubRid(async_zone_parent_1);
            SubRequest * sub_request_parent_2 = _validateSubRid(async_zone_parent_2);

            // sanity check and return true to avoid any unwanted cycles
            if (sub_request_parent_1 == nullptr || sub_request_parent_2 == nullptr) {
                return true;
            }
            return sub_request_parent_1->i < sub_request_parent_2->i;
        }

        // compare by async zone id if they are different and SIDs are equal, otherwise we keep iterating for next zones
        if (sub_zone[0] != sub_zone[1]) {
            return sub_zone[0] < sub_zone[1];
        }

    }
}

std::vector<std::string> Request::_getPrecedingAsyncZones(SubRequest* subrequest) {
    // REMINDER: the function that calls this method already acquires lock on _mutex_subrequests

    std::vector<std::string> entries;
    for (const auto& entry: _wait_logs) {
        if (_isPrecedingAsyncZone(entry, subrequest)) {
            entries.emplace_back(entry->sub_rid);
        }
    }
    return entries;
}

int Request::_numOpenedBranchesAsyncZones(const std::vector<std::string>& sub_rids) {
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

std::pair<int, int> Request::_numOpenedRegionsAsyncZones(
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

    if (_service_nodes.count(prev_service) == 0) return -3;

    SubRequest * subrequest = _validateSubRid(sub_rid);
    if (subrequest == nullptr) return -4;

    // -----------------------------------
    //           CORE WAIT LOGIC
    // -----------------------------------
    std::unique_lock<std::shared_mutex> lock(_mutex_subrequests);
    _addToWaitLogs(subrequest);
    while (true) {
        // get number of branches to ignore from preceding subrids in the wait logs
        const auto& preceding_subrids = _getPrecedingAsyncZones(subrequest);
        int offset = _numOpenedBranchesAsyncZones(preceding_subrids);

        if (_num_opened_branches.load() - subrequest->opened_branches.load() - offset != 0) {
            _cond_subrequests.wait_for(lock, std::chrono::seconds(remaining_timeout));
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                _removeFromWaitLogs(subrequest);
                return -1;
            }
        }
        else {
            break;
        }
    }
    _removeFromWaitLogs(subrequest);

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
            while ((*it)->opened_branches != 0) {
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

    if (_service_nodes.count(prev_service) == 0) return -3;

    SubRequest * subrequest = _validateSubRid(sub_rid);
    if (subrequest == nullptr) return -4;

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

    // -----------------------------------
    //           CORE WAIT LOGIC
    // -----------------------------------
    tbb::concurrent_hash_map<std::string, int>::const_accessor read_accessor_num;
    _addToWaitLogs(subrequest);
    while(true) {
        // get counters (region and globally, in terms of region) for current sub request
        int opened_sub_request_global_region = subrequest->opened_global_region.load();
        bool found = subrequest->opened_regions.find(read_accessor_num, region);
        int opened_sub_request_region = found ? read_accessor_num->second : 0;

        // get number of branches to ignore from preceding subrids in the wait logs
        const auto& preceding_subrids = _getPrecedingAsyncZones(subrequest);
        std::pair<int, int> offset = _numOpenedRegionsAsyncZones(preceding_subrids, region);

        if (_opened_global_region.load() - opened_sub_request_global_region - offset.first != 0 
            || _opened_regions[region] - opened_sub_request_region - offset.second != 0) {

            read_accessor_num.release();
            _cond_subrequests.wait_for(lock, std::chrono::seconds(remaining_timeout));
            inconsistency = 1;
            remaining_timeout = _computeRemainingTimeout(timeout, start_time);
            if (remaining_timeout <= std::chrono::seconds(0)) {
                _removeFromWaitLogs(subrequest);
                return -1;
            }
        }
        else {
            break;
        }
    }
    _removeFromWaitLogs(subrequest);

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
    std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);

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
        int * num_branches_ptr = &(_service_nodes[service]->opened_branches);
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
    std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);

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
    if (_service_nodes.count(prev_service) == 0) return utils::Status {UNKNOWN};

    SubRequest * subrequest = _validateSubRid(sub_rid);
    if (subrequest == nullptr) return utils::Status {INVALID_CONTEXT};

    std::unique_lock<std::mutex> lock(_mutex_branches);
    if (_num_opened_branches.load() > subrequest->opened_branches.load()) {
        return utils::Status {OPENED};
    }
    return utils::Status {CLOSED};

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
            if ((*it)->opened_branches != 0) {
                res.status = OPENED;
                return res;
            }
        }
        stop = parent;
        parent = parent->parent;
    } while (parent != nullptr); */
}

utils::Status Request::checkStatusRegion(const std::string& sub_rid, const std::string& region, const std::string& prev_service) {
    utils::Status res {UNKNOWN};

    if (_service_nodes.count(prev_service) == 0) return utils::Status {INVALID_CONTEXT};
    if (_opened_regions.count(region) == 0) return utils::Status {UNKNOWN};

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
    std::shared_lock<std::shared_mutex>lock(_mutex_service_nodes);

    // find out if service context exists
    if (_service_nodes.count(service) == 0) {
        res.status = UNKNOWN;
        return res;
    }
    
    // get overall status of request
    if (_service_nodes[service]->opened_branches == 0) {
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
    std::shared_lock<std::shared_mutex> lock(_mutex_service_nodes);
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
    utils::Dependencies result {OK};
    std::shared_lock<std::shared_mutex> lock(_mutex_service_nodes);
    /* // find out if context is valid
    if (_service_nodes.count(prev_service) == 0) {
        result.res = INVALID_CONTEXT;
        return result;
    }
    // get children nodes of current service
    for (auto it = _service_nodes[prev_service]->children.begin(); it != _service_nodes[prev_service]->children.end(); ++it) {
        result.deps.emplace_back((*it)->name);
    } */

    for (auto it = _service_nodes.begin(); it != _service_nodes.end(); it++) {
        if (it->second->opened_branches > 0) {
            result.deps.insert(it->first);
        }
    }

    return result;
}

utils::Dependencies Request::fetchDependenciesService(const std::string& service) {
    std::shared_lock<std::shared_mutex> lock(_mutex_service_nodes);

    if (_service_nodes.count(service) == 0) return utils::Dependencies {INVALID_SERVICE};

    utils::Dependencies result {0};
    std::stack<std::string> lookup_deps;
    // get direct children nodes of current service
    for (auto it = _service_nodes[service]->children.begin(); it != _service_nodes[service]->children.end(); ++it) {
        const std::string& curr_service = (*it)->name;
        result.deps.insert(curr_service);

        // store indirect (deph 1) dependencies
        for (auto children_it = _service_nodes[curr_service]->children.begin(); children_it != _service_nodes[curr_service]->children.end(); children_it++) {
            lookup_deps.push((*children_it)->name);
        }
    }

    // get all indirect nodes
    while (lookup_deps.size() != 0) {
        const std::string& curr_service = lookup_deps.top();
        lookup_deps.pop();

        // first children lookup for this service
        if (result.indirect_deps.count(curr_service) == 0) {
            for (auto children_it = _service_nodes[curr_service]->children.begin(); children_it != _service_nodes[curr_service]->children.end(); children_it++) {
                lookup_deps.push((*children_it)->name);
            }
        }
        result.indirect_deps.insert(curr_service);
    }

    return result;
}