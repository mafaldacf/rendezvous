#include "request.h"
#include "branch.h"
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <spdlog/spdlog.h>

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versions_registry)
    : _rid(rid), _next_bid_index(0), _num_opened_branches(0), _opened_global_region(0), 
    _next_sub_rid_index(1), async_zones_i(1), _versions_registry(versions_registry), _closed(false) {

    _last_ts = std::chrono::system_clock::now();
    // <bid, branch>
    _branches = std::unordered_map<std::string, metadata::Branch*>();
    _service_nodes = std::unordered_map<std::string, ServiceNode*>();
    _async_zones = oneapi::tbb::concurrent_hash_map<std::string, AsyncZone*>();
    _wait_logs = std::set<std::string>();
    _service_wait_logs = std::unordered_map<std::string, std::unordered_set<ServiceNode*>>();

    // add root node
    _service_nodes[utils::ROOT_SERVICE_NODE_ID] = new ServiceNode{utils::ROOT_SERVICE_NODE_ID};
    _service_nodes[utils::ROOT_SERVICE_NODE_ID]->async_zone_opened_branches[utils::ROOT_ASYNC_ZONE_ID] = 0;

    // insert the async_zone of the root
    tbb::concurrent_hash_map<std::string, AsyncZone*>::accessor write_accessor;
    _async_zones.insert(write_accessor, utils::ROOT_ASYNC_ZONE_ID);
    write_accessor->second = new AsyncZone{utils::ROOT_ASYNC_ZONE_ID, 0};
}

Request::~Request() {
    // if the request is closed then these structures were previously deleted
    if (!_closed) {
        for (const auto& it : _async_zones) {
            delete it.second;
        }
        for (const auto& it : _branches) {
            delete it.second;
        }
    }
    for (const auto& it : _service_nodes) {
        delete it.second;
    }
    delete _versions_registry;
}

void Request::setClosed() {
    _closed = true;
}

bool Request::isClosed() {
    return _closed;
}


void Request::partialDelete() {
    for (const auto& it : _async_zones) {
        delete it.second;
    }
    for (const auto& it : _branches) {
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

void Request::setBranchReplicationReady(metadata::Branch * branch) {
    if (utils::ASYNC_REPLICATION) {
        std::unique_lock<std::mutex> lock(_mutex_replicated_bid);
        branch->replicated.store(true);
        _cond_replicated_bid.notify_all();
    }
}

metadata::Branch * Request::_waitBranchRegistration(const std::string& bid) {
    std::unique_lock<std::mutex> lock(_mutex_branches);
    auto branch_it = _branches.find(bid);

    // if we are dealing with async replication we wait until branch is registered
    if (branch_it == _branches.end()) { 
        if (utils::ASYNC_REPLICATION) {
            auto start_time = std::chrono::steady_clock::now();
            auto remaining_time = _computeRemainingTime(utils::WAIT_REPLICA_TIMEOUT_S, start_time);
            while (branch_it == _branches.end()) {
                _cond_new_branch.wait_for(lock, std::chrono::seconds(remaining_time));
                remaining_time = _computeRemainingTime(utils::WAIT_REPLICA_TIMEOUT_S, start_time);
                if (remaining_time <= std::chrono::seconds(0)) {
                    // try one more time!
                    branch_it = _branches.find(bid);
                    if (branch_it != _branches.end()) {
                        return branch_it->second;
                    }
                    return nullptr;
                }
                branch_it = _branches.find(bid);
            }
            return branch_it->second;
        }
        else {
            return nullptr;
        }
    }
    return branch_it->second;
}

metadata::Branch * Request::_waitBranchReplicationReady(const std::string& bid) {
    // wait for creation
    metadata::Branch * branch = _waitBranchRegistration(bid);
    if (branch == nullptr) {
        return nullptr;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_time = _computeRemainingTime(utils::WAIT_REPLICA_TIMEOUT_S, start_time);
    std::unique_lock<std::mutex> lock(_mutex_replicated_bid);
    
    // wait for actuall replication
    while (branch->replicated.load() == false) {
        _cond_replicated_bid.wait_for(lock, std::chrono::seconds(remaining_time));
        if (remaining_time <= std::chrono::seconds(0)) {
            return nullptr;
        }
        remaining_time = _computeRemainingTime(utils::WAIT_REPLICA_TIMEOUT_S, start_time);
    }
    return branch;
}

bool Request::waitBranchesReplicationReady(std::vector<std::string> visible_bids) {
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_time = _computeRemainingTime(utils::WAIT_REPLICA_TIMEOUT_S, start_time);

    for (const auto& bid: visible_bids) {
        std::unique_lock<std::mutex> lock(_mutex_replicated_bid);
        auto branch_it = _branches.find(bid);
        
        // branches already exist
        while (branch_it == _branches.end() || branch_it->second->replicated.load() == false) {
            _cond_replicated_bid.wait_for(lock, std::chrono::seconds(remaining_time));
            if (remaining_time <= std::chrono::seconds(0)) {
                return false;
            }
            remaining_time = _computeRemainingTime(utils::WAIT_REPLICA_TIMEOUT_S, start_time);
            branch_it = _branches.find(bid);
        }
    }
    return true;
}

std::chrono::time_point<std::chrono::system_clock> Request::getLastTs() {
    return _last_ts;
}

replicas::VersionRegistry * Request::getVersionsRegistry() {
    return _versions_registry;
}

std::string Request::addNextAsyncZone(const std::string& sid, const std::string& async_zone_id, bool gen_id) {
    int next_async_zone_index;
    std::string next_sub_rid;

    // if we are currently in a async_zone_id we fetch its next async_zone_id
    if (gen_id) {
        if (!async_zone_id.empty()) {
            tbb::concurrent_hash_map<std::string, AsyncZone*>::accessor write_accessor;
            bool found = _async_zones.find(write_accessor, async_zone_id);
            // current async_zone_id does not exist
            if (!found) {
                return "";
            }

            // get next async_zone_id
            next_async_zone_index = write_accessor->second->next_async_zone_index++;
            // parse the index to full string of async_zone_id
            next_sub_rid = async_zone_id;
        }
        // otherwise we obtain the next async_zone_id from current request
        else {
            next_async_zone_index = _next_sub_rid_index.fetch_add(1);
            // parse the index to full string of async_zone_id
            next_sub_rid = "root";
        }
        next_sub_rid += utils::FULL_ID_DELIMITER + sid + std::to_string(next_async_zone_index);
    }
    // skip the id generation step
    else {
        next_sub_rid = async_zone_id;
    }

    // sanity check
    if (next_sub_rid != utils::ROOT_ASYNC_ZONE_ID) {
        // insert the next async_zone and return its id
        tbb::concurrent_hash_map<std::string, AsyncZone*>::accessor write_accessor;
        _async_zones.insert(write_accessor, next_sub_rid);
        write_accessor->second = new AsyncZone{next_sub_rid, async_zones_i.fetch_add(1)};
    }

    return next_sub_rid;
}

void Request::insertAsyncZone(const std::string& async_zone_id) {
    // sanity check
    if (async_zone_id != utils::ROOT_ASYNC_ZONE_ID) {
        // insert the next async_zone and return its id
        tbb::concurrent_hash_map<std::string, AsyncZone*>::accessor write_accessor;
        bool new_zone = _async_zones.insert(write_accessor, async_zone_id);
        if (new_zone) {
            write_accessor->second = new AsyncZone{async_zone_id};
        }
    }
}

metadata::Request::AsyncZone * Request::_validateAsyncZone(const std::string& async_zone_id) {
    // insert the next async_zone and return its id
    tbb::concurrent_hash_map<std::string, AsyncZone*>::accessor write_accessor;
    bool new_zone = _async_zones.insert(write_accessor, async_zone_id);
    if (new_zone) {
        AsyncZone * zone = new AsyncZone{async_zone_id};
        write_accessor->second = zone;
        return zone;
    }
    return write_accessor->second;
}

// ---------------------
// Core Rendezvous Logic
//----------------------

metadata::Branch * Request::registerBranch(const std::string& async_zone_id, const std::string& bid, const std::string& service,  
    const std::string& tag, const utils::ProtoVec& regions, const std::string& current_service_bid, bool replicated) {

    //spdlog::debug("> register branch for {}:{} @ {}", service, tag, async_zone_id);

    std::string current_service = "";

    if (!current_service_bid.empty()) {
        //spdlog::debug("> wait branch replication ready {}:{} @ {}", service, tag, async_zone_id);
        metadata::Branch * current_service_branch = _waitBranchReplicationReady(current_service_bid);
        if (current_service_branch == nullptr) {
            return nullptr;
        }
        //spdlog::debug("< wait branch replication ready {}:{} @ {}", service, tag, async_zone_id);
        current_service = current_service_branch->getService();
    }

    std::unique_lock<std::mutex> lock(_mutex_branches);
    auto branch_it = _branches.find(bid);

    // branches already exist
    if (branch_it != _branches.end()) {
        spdlog::error("Branch with core bid '{}' already exists", bid);
        return nullptr;
    }
    lock.unlock();

    metadata::Branch * branch;
    int num = regions.size();

    // branch with specified regions
    if (num > 0) {
        branch = new metadata::Branch(service, tag, async_zone_id, regions, replicated);
    }

    // no region specified - global region
    else {
        num = 1;
        branch = new metadata::Branch(service, tag, async_zone_id, replicated);
    }

    //spdlog::debug("> insert async zone {}:{} @ {}", service, tag, async_zone_id);
    insertAsyncZone(async_zone_id);
    //spdlog::debug("< insert async zone {}:{} @ {}", service, tag, async_zone_id);

    // error tracking branch (tag already exists!)
    //spdlog::debug("> track {}:{} @ {}", service, tag, async_zone_id);
    if (!trackBranch(async_zone_id, service, regions, num, current_service, branch)) {
        spdlog::error("Error tracking branch with core bid '{}'", bid);
        delete branch;
        //spdlog::debug("< track {}:{} @ {}", service, tag, async_zone_id);
        return nullptr;
    }
    //spdlog::debug("< track {}:{} @ {}", service, tag, async_zone_id);

    lock.lock();
    _branches[bid] = branch;
    
    // notify threads waiting for replication ready
    if (replicated) {
        _cond_replicated_bid.notify_all();
    }
    _cond_new_branch.notify_all();
    lock.unlock();

    //spdlog::debug("< registered branch for {}:{} @ {}", service, tag, async_zone_id);

    return branch;
}

int Request::closeBranch(const std::string& bid, const std::string& region) {
    //spdlog::debug("> close branch for {} @ {}", bid, region);

    metadata::Branch * branch = _waitBranchRegistration(bid);
    if (branch == nullptr) {
        spdlog::error("branch '{}' not found", bid);
        return -1;
    }
    int closed = branch->close(region);
    if (closed == 1) {
        const std::string& service = branch->getService(); 
        const std::string& async_zone_id = branch->getAsyncZoneId();
        bool globally_closed = branch->isGloballyClosed();

        // abort: error in async_zones tbb map
        if (!untrackBranch(async_zone_id, service, region, globally_closed)) {
            branch->open(region);
            spdlog::error("branch '{}' error untracking in async zone", bid);
            return -1;
        }

        // if this branch is globally closed than we are closer to have all the request closed
        // so we check if all branches are closed
        if (globally_closed) {
            if (_num_opened_branches.load() == 0) {
                return 2;
            }
        }
    }
    else {
        spdlog::error("Could not close branch with core bid '{}'", bid);
    }
    //spdlog::debug("< close branch for {} @ {}", bid, region);
    return closed;
}

bool Request::untrackBranch(const std::string& async_zone_id, const std::string& service, 
    const std::string& region, bool globally_closed) {

    // ---------------------------
    // SERVICE NODE & DEPENDENCIES
    // ---------------------------
    std::unique_lock<std::shared_mutex> lock_services(_mutex_service_nodes);

    // sanity check
    auto it = _service_nodes.find(service);
    if (it == _service_nodes.end()) return false;
    ServiceNode * service_node = it->second;
    
    if (globally_closed) {
        service_node->opened_branches--;
        service_node->async_zone_opened_branches[async_zone_id]--;
    }
    if (region.empty()) {
        service_node->opened_global_region--;
    }
    else {
        service_node->opened_regions[region]--;
    }

    // notify creation of new branch
    _cond_service_nodes.notify_all();
    lock_services.unlock();

    // ----------
    // ASYNC ZONE
    // ----------
    // althought we are modifying async_zones, this shared lock is used for
    // controlling concurrency with wait calls (they use unique_lock over this mutex)
    // hence, the following code is blocked until the wait stops reading
    // this is to ensure that both region trackers and async_zone region trackers
    // are observed at the same time in the wait calls, while ensuring fine-grained lock with tbb lib
    // when using a shared_lock here

    std::shared_lock<std::shared_mutex> lock_async_zones(_mutex_async_zones);

    if (!async_zone_id.empty()) {
        AsyncZone * async_zone = _validateAsyncZone(async_zone_id);
        if (async_zone == nullptr) return false;

        if (globally_closed) {
            async_zone->opened_branches.fetch_add(-1);
        }

        if (region.empty()) {
            async_zone->opened_global_region.fetch_add(-1);
        }
        else {
            tbb::concurrent_hash_map<std::string, int>::accessor write_accessor;
            bool found = async_zone->opened_regions.find(write_accessor, region);
            // sanity check (must always be found)
            if (found) {
                write_accessor->second--;
            }
        }
    }

    // ------------
    // REGIONS ONLY (REMINDER: needs to be placed after tracking async_zones to notify all threads)
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
    _cond_async_zones.notify_all();
    return true;
}

bool Request::trackBranch(const std::string& async_zone_id, const std::string& service, 
    const utils::ProtoVec& regions, int num, const std::string& current_service, metadata::Branch * branch) {
        
    // ---------------------------
    // SERVICE NODE & DEPENDENCIES
    // ---------------------------
    ServiceNode * parent_node;
    ServiceNode * service_node;
    std::unique_lock<std::shared_mutex> lock_services(_mutex_service_nodes);

    auto it = _service_nodes.find(current_service);
    // if parent does not exist we wait for it
    if (service != current_service && it == _service_nodes.end()) {
        lock_services.unlock();

        std::unique_lock<std::mutex> lock(_mutex_branches);
        auto start_time = std::chrono::steady_clock::now();
        auto remaining_time = _computeRemainingTime(utils::WAIT_REPLICA_TIMEOUT_S, start_time);
        while (it == _service_nodes.end()) {
            _cond_new_branch.wait_for(lock, std::chrono::seconds(remaining_time));
            remaining_time = _computeRemainingTime(utils::WAIT_REPLICA_TIMEOUT_S, start_time);
            if (remaining_time <= std::chrono::seconds(0)) {
                spdlog::error("current service node '{}' not found (timed out)", current_service);
                return false;
            }
            it = _service_nodes.find(current_service);
        }
        parent_node = it->second;
        lock_services.lock();
    }
    else {
        parent_node = it->second;
    }
    
    // sanity check
    auto service_node_it = _service_nodes.find(service);
    if (service_node_it == _service_nodes.end()) {
        std::vector<std::string> async_zone_ids = std::vector<std::string>();
        async_zone_ids.emplace_back(async_zone_id);
        service_node = new ServiceNode{service};
        service_node->opened_regions = std::unordered_map<std::string, int>();
        _service_nodes[service] = service_node;
    }
    else {
        service_node = service_node_it->second;
    }

    // needs to be placed before lock_service_nodes to prevent deadlocks (e.g. same service nodes)
    if (service != current_service) {
        std::unique_lock<std::shared_mutex> lock_parent_node(service_node->mutex);
        parent_node->children.emplace_back(service_node);
        lock_parent_node.unlock();
    }

    std::unique_lock<std::shared_mutex> lock_service_node(service_node->mutex);
    service_node->async_zone_opened_branches[async_zone_id] += 1;

    // validate tag
    if (branch->hasTag()) {
        service_node->tagged_branches[branch->getTag()].push_back(branch);
    }

    service_node->opened_branches++;
    for (const auto& region: regions) {
        service_node->opened_regions[region]++;
    }
    if (regions.empty()) {
        service_node->opened_global_region++;
    }

    // notify upon creation (due to async waits)
    _cond_new_service_nodes.notify_all();
    lock_service_node.unlock();
    lock_services.unlock();

    // ----------
    // ASYNC ZONE
    // ----------
    std::shared_lock<std::shared_mutex> lock_async_zones(_mutex_async_zones);
    if (!async_zone_id.empty()) {
        AsyncZone * async_zone = _validateAsyncZone(async_zone_id);
        if (async_zone == nullptr) return false;
        async_zone->opened_branches.fetch_add(1);

        tbb::concurrent_hash_map<std::string, int>::accessor write_accessor;
        for (const auto& region: regions) {
            bool new_key = async_zone->opened_regions.insert(write_accessor, region);
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
            async_zone->opened_global_region.fetch_add(1);
        }
    }

    // ------------
    // REGIONS ONLY 
    // REMINDER: this MUST be placed after tracking of async zones!!!
    // ------------
    // mantain order of these locks
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

std::chrono::seconds Request::_computeRemainingTime(int timeout, const std::chrono::steady_clock::time_point& start_time) {
    if (timeout != 0) {
        auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        auto remaining_time = std::chrono::seconds(timeout) - std::chrono::duration_cast<std::chrono::seconds>(elapsed_time);
        return remaining_time;
    }
    return std::chrono::seconds(60);
}

metadata::Request::ServiceNode * Request::validateServiceNode(const std::string& service) {
    std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);
    auto it = _service_nodes.find(service);
    if (it == _service_nodes.end()) return nullptr;
    return it->second;
}

void Request::_addToServiceWaitLogs(ServiceNode* curr_service_node, const std::string& target_service) {
    std::unique_lock<std::shared_mutex> lock(_mutex_service_wait_logs);

    // try to insert if not yet done
    _service_wait_logs[target_service].insert(curr_service_node);
    curr_service_node->num_current_waits++;

    // don't forget to notify for wait and waitRegion calls :)
    _cond_async_zones.notify_all();
}

void Request::_removeFromServiceWaitLogs(ServiceNode* curr_service_node, const std::string& target_service) {
    std::unique_lock<std::shared_mutex> lock(_mutex_service_wait_logs);

    int n = --curr_service_node->num_current_waits;
    if (n == 0) {
        _service_wait_logs[target_service].erase(curr_service_node);
    }
}

int Request::_numOpenedBranchesServiceLogs(const std::string& current_service) {
    int num = 0;

    std::shared_lock<std::shared_mutex> lock_logs(_mutex_service_wait_logs);
    std::unordered_set<ServiceNode*> service_logs = _service_wait_logs[current_service];
    lock_logs.unlock();

    std::unique_lock<std::shared_mutex> lock_services(_mutex_service_nodes);
    for (const auto& entry: service_logs) {
        num += entry->opened_branches;
    }
    lock_services.unlock();

    return num;
}

std::pair<int, int> Request::_numOpenedRegionsServiceLogs(const std::string& current_service, const std::string& region) {
    // <global region counter, current region counter>
    std::pair<int, int> num = {0, 0};

    std::unique_lock<std::shared_mutex> lock_logs(_mutex_service_wait_logs);
    std::unordered_set<ServiceNode*> service_logs = _service_wait_logs[current_service];
    lock_logs.unlock();

    std::unique_lock<std::shared_mutex> lock_services(_mutex_service_nodes);
    for (const auto& entry: service_logs) {
        auto it_region = entry->opened_regions.find(region);
        if (it_region != entry->opened_regions.end()) {
            num.second += it_region->second;
        }
        num.first += entry->opened_global_region;
    }
    lock_services.unlock();
    
    return num;
}

void Request::_addToWaitLogs(AsyncZone* async_zone) {
    // FRIENDLY REMINDER: caller of this function already acquires a lock on async_zones mutex

    // try to insert if not yet done
    _wait_logs.insert(async_zone->async_zone_id);
    async_zone->num_current_waits++;

    // notify regarding new wait logs to cover an >>> EDGE CASE <<<:
    // async_zone_id (a) registered before async_zone_id (b), but (b) does wait call before (a) and both with branches opened
    // (b) needs to be signaled and update its preceeding list to discard (a) from the wait call
    _cond_async_zones.notify_all();
}

void Request::_removeFromWaitLogs(AsyncZone* async_zone) {
    // FRIENDLY REMINDER: caller of this function already acquires a lock on async_zones mutex
    
    int n = --async_zone->num_current_waits;
    if (n == 0) {
        _wait_logs.erase(async_zone->async_zone_id);
    }
}

// Examples of IDs: 
// eu0:eu0 vs eu0:eu1
// eu0:us0 vs eu0:ap0
// eu0:eu0 vs eu0:eu0:eu0

bool Request::_isPrecedingAsyncZone(AsyncZone* async_zone_1, AsyncZone* async_zone_2) {
    std::vector<std::string> sub_async_zone(2), sid(2);
    std::vector<size_t> local_pos(2), global_pos(2);
    std::vector<std::string> id {async_zone_1->async_zone_id, async_zone_2->async_zone_id};
    std::vector<int> idx {async_zone_1->i, async_zone_2->i};

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

        // parse id: <sid>:<sub_async_zone>:<remaining of next id>
        // otherwise, we are at the end
        for (int i = 0; i < 2; i++) {
            local_pos[i] = id[i].find(utils::FULL_ID_DELIMITER, global_pos[i]);
            sid[i] = id[i].substr(global_pos[i], utils::SIZE_SIDS);
            if (local_pos[i] != std::string::npos) {
                sub_async_zone[i] = id[i].substr(global_pos[i] + utils::SIZE_SIDS, local_pos[i] - utils::SIZE_SIDS);
                global_pos[i] += local_pos[i] + 1;
            }
            else {
                sub_async_zone[i] = id[i].substr(global_pos[i] + utils::SIZE_SIDS);
            }
        }

        // if SIDs are different, compare by index of insertion of the DIRECT PARENT
        if (sid[0] != sid[1]) {
            std::string async_zone_id_parent_1 = id[0].substr(0, global_pos[0]);
            std::string async_zone_id_parent_2 = id[1].substr(0, global_pos[1]);
            AsyncZone * async_zone_parent_1 = _validateAsyncZone(async_zone_id_parent_1);
            AsyncZone * async_zone_parent_2 = _validateAsyncZone(async_zone_id_parent_2);

            // sanity check and return true to avoid any unwanted cycles
            if (async_zone_parent_1 == nullptr || async_zone_parent_2 == nullptr) {
                return true;
            }
            return async_zone_parent_1->i < async_zone_parent_2->i;
        }

        // compare by async zone id if they are different and SIDs are equal, otherwise we keep iterating for next zones
        if (sub_async_zone[0] != sub_async_zone[1]) {
            return sub_async_zone[0] < sub_async_zone[1];
        }

    }
}

std::vector<std::string> Request::_getHighestAsyncZones(AsyncZone* async_zone) {
    std::shared_lock<std::shared_mutex> lock(_mutex_service_wait_logs);
    std::vector<std::string> entries;

    // iterate in reverse order
    for (auto it = _wait_logs.rbegin(); it != _wait_logs.rend(); it++) {
        if ((*it) > async_zone->async_zone_id) {
            entries.emplace_back((*it));
        }
        else { 
            break;
        }
    }
    return entries;
}

int Request::_numOpenedBranchesAsyncZones(const std::vector<std::string>& sub_rids) {
    // REMINDER: the function that calls this method already acquires lock on _mutex_async_zones

    int num = 0;
    tbb::concurrent_hash_map<std::string, AsyncZone*>::const_accessor read_accessor;
    for (const auto& async_zone_id: sub_rids) {
        bool found = _async_zones.find(read_accessor, async_zone_id);
        // sanity check
        if (!found) continue;
        AsyncZone * async_zone = read_accessor->second;
        num += async_zone->opened_branches.load();

    }
    read_accessor.release();
    return num;
}

std::pair<int, int> Request::_numOpenedRegionsAsyncZones(
    const std::vector<std::string>& sub_rids, const std::string& region) {
    // REMINDER: the function that calls this method already acquires lock!

    // <global region counter, current region counter>
    std::pair<int, int> num = {0, 0};
    tbb::concurrent_hash_map<std::string, AsyncZone*>::const_accessor read_accessor_async_zone;
    tbb::concurrent_hash_map<std::string, int>::const_accessor read_accessor_num;
    for (const auto& async_zone_id: sub_rids) {
        bool found = _async_zones.find(read_accessor_async_zone, async_zone_id);

        // sanity check
        if (!found) continue;
        AsyncZone * async_zone = read_accessor_async_zone->second;

        // get number of opened branches globally, in terms of regions
        num.second += async_zone->opened_global_region.load();

        // get number of opened branches for this region
        found = async_zone->opened_regions.find(read_accessor_num, region);
        if (!found) continue;
        num.second += read_accessor_num->second;
    }
    return num;
}

bool Request::_waitFirstBranch(const std::chrono::steady_clock::time_point& start_time, int timeout) {
    std::unique_lock<std::mutex> lock(_mutex_branches);
    auto remaining_time = _computeRemainingTime(timeout, start_time);
    while (_branches.size() == 0) {
        _cond_new_branch.wait_for(lock, std::chrono::seconds(remaining_time));
        if (remaining_time <= std::chrono::seconds(0)) {
            return false;
        }
    }
    return true;
}

int Request::wait(const std::string& async_zone_id, bool async, int timeout, const std::string& current_service) {
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();

    // -----------------------------------
    //           VALIDATIONS
    // -----------------------------------

    AsyncZone * async_zone = _validateAsyncZone(async_zone_id);
    if (async_zone == nullptr) return -4;

    // -----------------------------------
    //           ASYNC CREATION
    // -----------------------------------

    if (async) {
        _waitFirstBranch(start_time, timeout);
    }
    auto remaining_time = _computeRemainingTime(timeout, start_time);

    // -----------------------------------
    //           CORE WAIT LOGIC
    // -----------------------------------
    std::unique_lock<std::shared_mutex> lock(_mutex_async_zones);
    _addToWaitLogs(async_zone);
    while (true) {
        // get number of branches to ignore from highest async zones in the wait logs
        const auto& highest_async_zones = _getHighestAsyncZones(async_zone);
        int offset_highest_async_zones = _numOpenedBranchesAsyncZones(highest_async_zones);
        // services waiting on the current one (we give them priority)
        int offset_services = _numOpenedBranchesServiceLogs(current_service);
        int offset = async_zone->opened_branches.load() + offset_highest_async_zones + offset_services;
        if (_num_opened_branches.load() - offset != 0) {
            _cond_async_zones.wait_for(lock, std::chrono::seconds(remaining_time));
            inconsistency = 1;
            remaining_time = _computeRemainingTime(timeout, start_time);
            if (remaining_time <= std::chrono::seconds(0)) {
                _removeFromWaitLogs(async_zone);
                return -1;
            }
        }
        else {
            break;
        }
    }
    _removeFromWaitLogs(async_zone);
    
    return inconsistency;
}

int Request::waitRegion(const std::string& async_zone_id, const std::string& region, bool async, int timeout, const std::string& current_service) {
        
    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    std::chrono::seconds remaining_time;

    // -----------------------------------
    //           VALIDATIONS
    // -----------------------------------

    AsyncZone * async_zone = _validateAsyncZone(async_zone_id);
    if (async_zone == nullptr) return -4;

    // -----------------------------------
    //           ASYNC CREATION
    // -----------------------------------

    // wait for creation
    if (async) {
        _waitFirstBranch(start_time, timeout);
        remaining_time = _computeRemainingTime(timeout, start_time);

        std::unique_lock<std::shared_mutex> lock(_mutex_async_zones);
        while (_opened_regions.count(region) == 0) {
            _cond_async_zones.wait_for(lock, std::chrono::seconds(remaining_time));
            inconsistency = 1;
            remaining_time = _computeRemainingTime(timeout, start_time);
            if (remaining_time <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    else {
        std::unique_lock<std::shared_mutex> lock(_mutex_async_zones);
        if (_opened_regions.count(region) == 0) {
            return 0;
        }
    }

    std::unique_lock<std::shared_mutex> lock(_mutex_async_zones);

    remaining_time = _computeRemainingTime(timeout, start_time);

    // -----------------------------------
    //           CORE WAIT LOGIC
    // -----------------------------------
    tbb::concurrent_hash_map<std::string, int>::const_accessor read_accessor_num;
    _addToWaitLogs(async_zone);
    while(true) {
        // get counters (region and globally, in terms of region) for current sub request
        int num_branches_async_zone_global_region = async_zone->opened_global_region.load();
        bool found = async_zone->opened_regions.find(read_accessor_num, region);
        int num_branches_async_zone_region = found ? read_accessor_num->second : 0;

        // get number of branches to ignore from preceding subrids in the wait logs
        const auto& highest_async_zones = _getHighestAsyncZones(async_zone);
        std::pair<int, int> offset_highest_async_zones = _numOpenedRegionsAsyncZones(highest_async_zones, region);
        std::pair<int, int> offset_services = _numOpenedRegionsServiceLogs(current_service, region);

        int offset_global_region = num_branches_async_zone_global_region + offset_highest_async_zones.first + offset_services.first;
        int offset_region = num_branches_async_zone_region + offset_highest_async_zones.second + offset_services.second;

        if (_opened_global_region.load() - offset_global_region != 0 || _opened_regions[region] - offset_region != 0) {

            read_accessor_num.release();
            _cond_async_zones.wait_for(lock, std::chrono::seconds(remaining_time));
            inconsistency = 1;
            remaining_time = _computeRemainingTime(timeout, start_time);
            if (remaining_time <= std::chrono::seconds(0)) {
                _removeFromWaitLogs(async_zone);
                return -1;
            }
        }
        else {
            break;
        }
    }
    _removeFromWaitLogs(async_zone);

    return inconsistency;
}

bool Request::_doWaitAsyncServiceNodeRegistration(const std::string& service, const std::string& tag, const std::string& region,
    int timeout, const std::chrono::steady_clock::time_point& start_time, std::chrono::seconds remaining_time) {
        
    std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);
    while (_service_nodes.count(service) == 0) {
        _cond_new_service_nodes.wait_for(lock, remaining_time);
        remaining_time = _computeRemainingTime(timeout, start_time);
        if (remaining_time <= std::chrono::seconds(0)) {
            return false;
        }
    }
    if (!tag.empty()) {
        while (_service_nodes[service]->tagged_branches.count(tag) == 0) {
            _cond_new_service_nodes.wait_for(lock, remaining_time);
            remaining_time = _computeRemainingTime(timeout, start_time);
            if (remaining_time <= std::chrono::seconds(0)) {
                return false;
            }
        }
    }
    if (!region.empty()) {
        auto opened_regions_it = _service_nodes[service]->opened_regions;
        auto region_it = opened_regions_it.find(region);
        while (region_it == opened_regions_it.end()) {
            _cond_new_service_nodes.wait_for(lock, remaining_time);
            remaining_time = _computeRemainingTime(timeout, start_time);
            if (remaining_time <= std::chrono::seconds(0)) return false;

            region_it = opened_regions_it.find(region);
        }
    }
    return true;
}

int Request::waitService(const std::string& async_zone_id, 
    const std::string& service, const std::string& tag, bool async, int timeout, 
    const std::string& current_service, bool wait_deps) {

    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_time = _computeRemainingTime(timeout, start_time);

    AsyncZone * async_zone = _validateAsyncZone(async_zone_id);
    if (async_zone == nullptr) return -4;

    // -------------------------
    // ASYNCHRONOUS REGISTRATION
    // -------------------------

    // branch is expected to be asynchronously opened so we need to wait for the branch context
    if (async) {
        bool found = _doWaitAsyncServiceNodeRegistration(service, tag, "", timeout, start_time, remaining_time);
        if (!found) return -1;
    }
    // context error checking
    else {
        std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);
        // no current branch for this service
        if (_service_nodes.count(service) == 0) return -2;
        // no current branch for this service (non async) tag
        if (!tag.empty() && _service_nodes[service]->tagged_branches.count(tag) == 0) return -2;
    }

    // -----------
    // VALIDATIONS
    //------------

    std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);
    auto it = _service_nodes.find(current_service);
    if (it == _service_nodes.end()) return -3;

    ServiceNode * current_service_node = it->second;
    ServiceNode * service_node = _service_nodes[service];
    lock.unlock();

    // ----------
    // WAIT LOGIC
    //-----------
    _addToServiceWaitLogs(current_service_node, service);
    // tag-specific
    if (!tag.empty()) {
        inconsistency = _doWaitTag(service_node, tag, "", timeout, start_time, remaining_time);
    }
    // overall service
    else {
        inconsistency = _doWaitService(service_node, async_zone_id, timeout, start_time, remaining_time);
        // wait for all dependencies
        if (inconsistency != -1 && wait_deps) {
            const auto& deps = _getAllFollowingDependencies(service_node, async_zone_id);
            int i = 0;
            int deps_size = deps.size();

            // iterate and wait all on all dependencies
            while (i < deps_size) {
                auto dep = deps.at(i++);
                inconsistency = _doWaitService(dep, async_zone_id, timeout, start_time, remaining_time);
                if (inconsistency == -1) {
                    break;
                }
            }
        }
    }

    _removeFromServiceWaitLogs(current_service_node, service);
    return inconsistency;
}

int Request::waitServiceRegion(const std::string& async_zone_id, const std::string& service, 
    const std::string& region, 
    const::std::string& tag, bool async, int timeout, 
    const std::string& current_service, bool wait_deps) {

    int inconsistency = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto remaining_time = _computeRemainingTime(timeout, start_time);
    
    // -------------------------
    // ASYNCHRONOUS REGISTRATION
    // -------------------------

    // wait for branch context (service and region)
    if (async) {
        bool found = _doWaitAsyncServiceNodeRegistration(service, tag, region, timeout, start_time, remaining_time);
        if (!found) return -1;
    }
    else {
        std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);
        // context not found
        if (_service_nodes.count(service) == 0 || _service_nodes[service]->opened_regions.count(region) == 0) {
            return -2;
        }
        // tag not found
        else if (!tag.empty() && _service_nodes[service]->tagged_branches.count(tag) == 0) {
            return -4;
        }
    }

    // -----------
    // VALIDATIONS
    // ------------

    AsyncZone * async_zone = _validateAsyncZone(async_zone_id);
    if (async_zone == nullptr) return -4;
    
    std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);
    auto it = _service_nodes.find(current_service);
    if (it == _service_nodes.end()) return -3;

    ServiceNode * service_node = _service_nodes[service];
    ServiceNode * current_service_node = it->second;

    lock.unlock();
    
    // ----------
    // WAIT LOGIC
    //-----------
    _addToServiceWaitLogs(current_service_node, service);

    // tag-specific
    if (!tag.empty()) {
        inconsistency = _doWaitTag(service_node, tag, region, timeout, start_time, remaining_time);
    }
    // overall service
    else {
        inconsistency = _doWaitServiceRegion(service_node, region, async_zone_id, timeout, start_time, remaining_time);
        // wait for all dependencies
        if (inconsistency != -1 && wait_deps) {
            const auto& deps = _getAllFollowingDependencies(service_node, async_zone_id);
            int i = 0;
            int deps_size = deps.size();

            // iterate and wait all on all dependencies
            while (i < deps_size) {
                auto dep = deps.at(i++);
                inconsistency = _doWaitServiceRegion(dep, region, async_zone_id, timeout, start_time, remaining_time);
                if (inconsistency == -1) {
                    break;
                }
            }
        }
    }
    _removeFromServiceWaitLogs(current_service_node, service);
    return inconsistency;
}

int Request::_doWaitTag(ServiceNode * service_node, const std::string& tag, const std::string& region, 
    int timeout, const std::chrono::steady_clock::time_point& start_time, std::chrono::seconds remaining_time) {
    
    std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);
    int inconsistency = 0;

    for (int i = 0; i < service_node->tagged_branches[tag].size(); i++) {
        metadata::Branch * branch = service_node->tagged_branches[tag].at(i);

        // this is an helper function for both wait on service and wait on service and region
        // so the region can be empty
        while (!branch->isGloballyClosed(region)) {
            inconsistency = 1;
            _cond_service_nodes.wait_for(lock, remaining_time);
            remaining_time = _computeRemainingTime(timeout, start_time);
            if (remaining_time <= std::chrono::seconds(0)) {
                return -1;
            }
        }
    }
    return inconsistency;
}

int Request::_doWaitService(ServiceNode * service_node, const std::string& async_zone_id,
    int timeout, const std::chrono::steady_clock::time_point& start_time, std::chrono::seconds remaining_time) {

    std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);
    int inconsistency = 0;

    int * num_branches_ptr = &(service_node->opened_branches);
    int * async_zone_id_num_branches_ptr = &(service_node->async_zone_opened_branches[async_zone_id]);
    
    // wait until branches are closed and only if there are more 
    // branches opened besides the one in the current async zone
    while (*num_branches_ptr > 0 && *num_branches_ptr > *async_zone_id_num_branches_ptr) {
        _cond_service_nodes.wait_for(lock, remaining_time);
        inconsistency = 1;
        remaining_time = _computeRemainingTime(timeout, start_time);
        if (remaining_time <= std::chrono::seconds(0)) {
            spdlog::error("[{}] service node {} not found and timed out", _rid, service_node->name);
            return -1;
        }
    }
    return inconsistency;
}

int Request::_doWaitServiceRegion(ServiceNode * service_node, const std::string& region, const std::string& async_zone_id,
    int timeout, const std::chrono::steady_clock::time_point& start_time, std::chrono::seconds remaining_time) {

    std::unique_lock<std::shared_mutex> lock(_mutex_service_nodes);

    int inconsistency = 0;

    int * num_branches_region_ptr = &service_node->opened_regions[region];
    int * num_global_region_ptr = &service_node->opened_global_region;
    int * num_branches_ptr = &service_node->opened_branches;
    int * async_zone_id_num_branches_ptr = &service_node->async_zone_opened_branches[async_zone_id];

    // WAIT FOR:
    // - current region
    // - global region that encompasses all regions
    // - BUT only if there are more opened branches besides the ones in the current zone (that we must ignore!)
    while ((*num_branches_region_ptr != 0 || *num_global_region_ptr != 0) && *num_branches_ptr > *async_zone_id_num_branches_ptr) {

        _cond_service_nodes.wait_for(lock, remaining_time);
        inconsistency = 1;
        remaining_time = _computeRemainingTime(timeout, start_time);
        if (remaining_time <= std::chrono::seconds(0)) {
            return -1;
        }
    }
    return inconsistency;
}

std::vector<metadata::Request::ServiceNode*> Request::_getAllFollowingDependencies(ServiceNode * service_node, 
    const std::string& async_zone_id) {

    std::vector<ServiceNode*> deps;
    int i = 0;
    std::unordered_set<ServiceNode*> visited {service_node};
    visited.insert(service_node);

    std::shared_lock<std::shared_mutex> lock_service_node(service_node->mutex);
    // get all depends for current service
    for (const auto& child: service_node->children) {
        deps.emplace_back(child);
    }
    lock_service_node.unlock();

    while (i < deps.size()) {
        auto dep = deps.at(i++);
        visited.insert(dep);
        std::shared_lock<std::shared_mutex> lock_service_node_dep(dep->mutex);
        // get all following dependencies for fetched service
        // - cannot be already visited
        // - cannot be in the same async zone
        for (const auto& child: dep->children) {
            if (visited.count(child) == 0) {
                auto async_zone_ids_map = child->async_zone_opened_branches;

                // if the service has only one async zone which is the current one we just ignore it
                if(async_zone_ids_map.size() == 1 && async_zone_ids_map.count(async_zone_id) == 1) {
                    deps.emplace_back(child);
                }
            }
        }
        lock_service_node_dep.unlock();
    }
    return deps;
}

utils::Status Request::checkStatus(const std::string& async_zone_id) {
    AsyncZone * async_zone = _validateAsyncZone(async_zone_id);
    if (async_zone == nullptr) return utils::Status {INVALID_CONTEXT};

    std::unique_lock<std::mutex> lock(_mutex_branches);
    // get number of all opened branches and ignore ones in the current async zone
    if (_num_opened_branches.load() - async_zone->opened_branches.load() != 0) {
        return utils::Status {OPENED};
    }
    return utils::Status {CLOSED};
}

utils::Status Request::checkStatusRegion(const std::string& async_zone_id, const std::string& region) {
    utils::Status res {UNKNOWN};

    if (_opened_regions.count(region) == 0) return utils::Status {UNKNOWN};

    AsyncZone * async_zone = _validateAsyncZone(async_zone_id);
    if (async_zone == nullptr) return utils::Status {INVALID_CONTEXT};

    tbb::concurrent_hash_map<std::string, int>::const_accessor read_accessor_num;
    std::unique_lock<std::mutex> lock(_mutex_regions);

    // get counters (region and globally) for current sub request
    int async_zone_global_region = async_zone->opened_global_region.load();
    bool found = async_zone->opened_regions.find(read_accessor_num, region);
    int async_zone_region = found ? read_accessor_num->second : 0;

    // by default, we are targeting a specific region
    // but if a global region is opened then all regions are opened aswell
    if (_opened_regions[region] - async_zone_region != 0 || _opened_global_region.load() - async_zone_global_region != 0) {
        res.status = OPENED;
    }
    else {
        res.status = CLOSED;
    }

    return res;
}

utils::Status Request::checkStatusService(const std::string& async_zone_id, const std::string& service, bool detailed) {
    utils::Status res;
    std::shared_lock<std::shared_mutex>lock(_mutex_service_nodes);

    auto it = _service_nodes.find(service);

    // find out if service context exists
    if (it == _service_nodes.end()) {
        res.status = UNKNOWN;
        return res;
    }

    ServiceNode * service_node = it->second;
    lock.unlock();

    std::shared_lock<std::shared_mutex> lock_service_node(service_node->mutex);
    
    // get overall status of request
    // BUT if there are more than 0 opened branches we ignore if they belong to the same region
    if (service_node->opened_branches == 0 
        || service_node->opened_branches == service_node->async_zone_opened_branches[async_zone_id]) {
        res.status = CLOSED;
    } else {
        res.status = OPENED;
    }

    // return detailed info if enabled by client
    if (detailed) {
        // get tagged branches within the same service
        for (const auto& tag_it: service_node->tagged_branches) {
            res.tagged[tag_it.first] = CLOSED;
            // set status to OPENED if AT LEAST one branch is opened for this tag
            for (const auto& branch_it: tag_it.second) {
                if (branch_it->getStatus() == OPENED) {
                    res.tagged[tag_it.first] = OPENED;
                    break;
                }
            }
        }
        // get all regions status
        for (const auto& region_it: service_node->opened_regions) {
            res.regions[region_it.first] = region_it.second == 0 ? CLOSED : OPENED;
        }

    }
    return res;
}

utils::Status Request::checkStatusServiceRegion(const std::string& async_zone_id, 
    const std::string& service, const std::string& region, bool detailed) {

    std::shared_lock<std::shared_mutex> lock(_mutex_service_nodes);
    utils::Status res;

    // find out if service context exists
    auto service_it = _service_nodes.find(service);
    if (service_it == _service_nodes.end()) {
        res.status = UNKNOWN;
        return res;
    }
    lock.unlock();
    
    ServiceNode * service_node = service_it->second;
    std::shared_lock<std::shared_mutex> lock_service_node(service_node->mutex);
    auto region_it = service_node->opened_regions.find(region);
    if (region_it == service_node->opened_regions.end()) {
        res.status = UNKNOWN;
        return res;
    }
    
    // get overall status of request
    // get tagged branches within the same service
    if (service_node->opened_regions[region] == 0
        || service_node->opened_branches == service_node->async_zone_opened_branches[async_zone_id]) {
            
        res.status = CLOSED;
    } else {
        res.status = OPENED;
    }

    // return if client only wants basic information (status)
    if (!detailed) {
        return res;
    }

    // otherwise, return detailed information
    for (const auto& tag_it: service_node->tagged_branches) {
        res.tagged[tag_it.first] = CLOSED;
        // set status to OPENED if at least one branch for this tag is opened
        for (const auto& branch_it: tag_it.second) {
            if (branch_it->getStatus(region) == OPENED) {
                res.tagged[tag_it.first] = OPENED;
                break;
            }
        }
    }
    return res;
}

utils::Dependencies Request::fetchDependencies(const std::string& async_zone_id, const std::string& service) {
    utils::Dependencies result {OK};
    std::shared_lock<std::shared_mutex> lock(_mutex_service_nodes);
    AsyncZone * async_zone = _validateAsyncZone(async_zone_id);
    if (async_zone == nullptr) return utils::Dependencies {INVALID_CONTEXT};

    // service can also be the root
    auto service_node_it = _service_nodes.find(service);
    if (service_node_it == _service_nodes.end()) {
        return utils::Dependencies {INVALID_SERVICE};
    }
    lock.unlock();

    ServiceNode * service_node = service_node_it->second;
    const auto& deps = _getAllFollowingDependencies(service_node, async_zone_id);
    for (auto & dep : deps) {
        std::shared_lock<std::shared_mutex> lock_service_node_dep(dep->mutex);
        if (dep->opened_branches > 0) {
            result.deps.insert(dep->name);
        }
    }
    return result;
}