#include "request.h"

using namespace metadata;

Request::Request(std::string rid, replicas::VersionRegistry * versionsRegistry)
    : rid(rid), nextId(0), numBranches(0), versionsRegistry(versionsRegistry) {
    
    // <bid, branch>
    branches = std::unordered_map<std::string, metadata::Branch*>();

    /* Opened Branches */
    numBranchesRegion = std::unordered_map<std::string, uint64_t>();
    numBranchesService = std::unordered_map<std::string, uint64_t>();
    numBranchesServiceRegion = std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>>();
}

Request::~Request() {
    for (const auto& branch_it : branches) {
        delete branch_it.second;
    }
    delete versionsRegistry;
}

std::string Request::getRid() {
    return rid;
}

replicas::VersionRegistry * Request::getVersionsRegistry() {
    return versionsRegistry;
}

std::string Request::genId() {
  return std::to_string(nextId.fetch_add(1));
}

int Request::registerBranch(const std::string& bid, const std::string& service, const std::string& region) {
    mutex_branches.lock();

    auto branch_it = branches.find(bid);

    // branches already exist
    if (branch_it != branches.end()) {
        mutex_branches.unlock();
        return -1;
    }

    // branch not found
    if (branch_it == branches.end()) {
        metadata::Branch * branch = new metadata::Branch(bid, service, region);
        branches[bid] = branch;
        trackBranchOnContext(service, region, REGISTER);
    }

    cond_new_branches.notify_all();
    mutex_branches.unlock();
    return 0;
}

int Request::registerBranches(const std::string& bid, const std::string& service, const utils::ProtoVec& regions) {
    mutex_branches.lock();

    auto branch_it = branches.find(bid);

    // branches already exist
    if (branch_it != branches.end()) {
        mutex_branches.unlock();
        return -1;
    }

    metadata::Branch * branch = new metadata::Branch(bid, service, regions);
    branches[bid] = branch;
    int num = regions.size();
    trackBranchesOnContext(service, regions, REGISTER, num);

    cond_new_branches.notify_all();
    mutex_branches.unlock();
    return 0;
}

bool Request::closeBranch(const std::string& bid, const std::string& region) {
    std::unique_lock<std::mutex> lock(mutex_branches);

    bool region_found = false;
    auto branch_it = branches.find(bid);

    // branch not found
    while (branch_it == branches.end()) {
        cond_new_branches.wait(lock);
        branch_it = branches.find(bid);
    }

    metadata::Branch * branch = branch_it->second;
    if (branch->close(region)) {
        trackBranchOnContext(branch->getService(), region, REMOVE);
        region_found = true;
    }

    cond_branches.notify_all();
    return region_found;
}

void Request::trackBranchOnContext(const std::string& service, const std::string& region, const long& value) {
    numBranches.fetch_add(value);

    if (!service.empty()) {
        mutex_numBranchesService.lock();

        numBranchesService[service] += value;
        if (value == REMOVE) 
            cond_numBranchesService.notify_all();

        mutex_numBranchesService.unlock();
    }
    if (!region.empty()) {
        mutex_numBranchesRegion.lock();

        numBranchesRegion[region] += value;
        if (value == REMOVE) 
            cond_numBranchesRegion.notify_all();
            
        mutex_numBranchesRegion.unlock();
    }
    if (!service.empty() && !region.empty()) {
        mutex_numBranchesServiceRegion.lock();

        numBranchesServiceRegion[service][region] += value;
        if (value == REMOVE) 
            cond_numBranchesServiceRegion.notify_all();
            
        mutex_numBranchesServiceRegion.unlock();
    }
}

void Request::trackBranchesOnContext(const std::string& service, const utils::ProtoVec& regions, const long& value, const int& num) {
    numBranches.fetch_add(value*num);

    if (!service.empty()) {
        mutex_numBranchesService.lock();

        numBranchesService[service] += value*num;
        if (value == REMOVE) 
            cond_numBranchesService.notify_all();

        mutex_numBranchesService.unlock();
    }

    if (regions.size() > 0) {
        mutex_numBranchesRegion.lock();
        mutex_numBranchesServiceRegion.lock();

        for (const auto& region : regions) {
            // sanity check - region can never be empty when registering a set of branches
            if (!region.empty()) {

                numBranchesRegion[region] += value;
                if (value == REMOVE)
                    cond_numBranchesRegion.notify_all();

                if (!service.empty()) {
                    numBranchesServiceRegion[service][region] += value;
                    if (value == REMOVE)
                        cond_numBranchesServiceRegion.notify_all();
                }
            }
        }

        mutex_numBranchesRegion.unlock();
        mutex_numBranchesServiceRegion.unlock();
    }

}

int Request::wait() {
    int inconsistency = 0;
    std::unique_lock<std::mutex> lock(mutex_branches);
    while (numBranches.load() != 0) {
        cond_branches.wait(lock);
        inconsistency = 1;
    }
    return inconsistency;
}

int Request::waitOnService(const std::string& service) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(mutex_numBranchesService);

    if (numBranchesService.count(service) == 0) { // not found
        return -2;
    }

    uint64_t * valuePtr = &numBranchesService[service];
    while (*valuePtr != 0) {
        cond_numBranchesService.wait(lock);
        inconsistency = 1;
    }
    
    return inconsistency;
}

int Request::waitOnRegion(const std::string& region) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(mutex_numBranchesRegion);

    if (numBranchesRegion.count(region) == 0) { // not found
        return -3;
    }

    uint64_t * valuePtr = &numBranchesRegion[region];
    while (*valuePtr != 0) {
        cond_numBranchesRegion.wait(lock);
        inconsistency = 1;
    }

    return inconsistency;
}

int Request::waitOnServiceAndRegion(const std::string& service, const std::string& region) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(mutex_numBranchesServiceRegion);

    auto service_it = numBranchesServiceRegion.find(service);
    if (service_it == numBranchesServiceRegion.end()) {
        return -2;
    }

    auto region_it = service_it->second.find(region);
    if (region_it == service_it->second.end()) {
        return -3;
    }

    uint64_t * valuePtr = &numBranchesServiceRegion[service][region];

    while (*valuePtr != 0) {
        cond_numBranchesServiceRegion.wait(lock);
        inconsistency = 1;
    }
    return inconsistency;
}

int Request::getStatus() {
    if (numBranches.load() == 0) {
        return CLOSED;
    }
    return OPENED;
}

int Request::getStatusOnService(const std::string& service) {
    mutex_numBranchesService.lock();

    if (!numBranchesService.count(service)) {
        mutex_numBranchesService.unlock();
        return -2;
    }

    if (numBranchesService[service] == 0) {
        mutex_numBranchesService.unlock();
        return CLOSED;
    }

    mutex_numBranchesService.unlock();
    return OPENED;
}

int Request::getStatusOnRegion(const std::string& region) {
    mutex_numBranchesRegion.lock();

    if (!numBranchesRegion.count(region)) {
        mutex_numBranchesRegion.unlock();
        return -3;
    }

    if (numBranchesRegion[region] == 0) {
        mutex_numBranchesRegion.unlock();
        return CLOSED;
    }

    mutex_numBranchesRegion.unlock();
    return OPENED;
}

int Request::getStatusOnServiceAndRegion(const std::string& service, const std::string& region) {
    mutex_numBranchesServiceRegion.lock();

    auto service_it = numBranchesServiceRegion.find(service);
    if (service_it == numBranchesServiceRegion.end()) {
        mutex_numBranchesServiceRegion.unlock();
        return -2;
    }

    auto region_it = service_it->second.find(region);
    if (region_it == service_it->second.end()) {
        mutex_numBranchesServiceRegion.unlock();
        return -3;
    }

    if (numBranchesServiceRegion[service][region] == 0) {
        mutex_numBranchesServiceRegion.unlock();
        return CLOSED;
    }

    mutex_numBranchesServiceRegion.unlock();
    return OPENED;
}

std::map<std::string, int> Request::getStatusByRegions() {
    std::map<std::string, int> result = std::map<std::string, int>();

    mutex_numBranchesRegion.lock();
    for (const auto& it : numBranchesRegion) {
        result[it.first] = it.second == 0 ? CLOSED : OPENED;
    }
    mutex_numBranchesRegion.unlock();
    
    return result;
}

std::map<std::string, int> Request::getStatusByRegionsOnService(const std::string& service, int * status) {
    std::map<std::string, int> result = std::map<std::string, int>();
    *status = 0;

    mutex_numBranchesServiceRegion.lock();
    auto service_it = numBranchesServiceRegion.find(service);

    // no status found for a given service
    if (service_it == numBranchesServiceRegion.end()) {
        *status = -2;
        mutex_numBranchesServiceRegion.unlock();
        return result;
    }

    // <service, <region, number of opened branches>>
    for (const auto& region_it : service_it->second) {
        // sanity check: skip branches with no region
        if (!region_it.first.empty()) {
            result[region_it.first] = region_it.second == 0 ? CLOSED : OPENED;
        }
    }
    
    mutex_numBranchesServiceRegion.unlock();
    
    return result;
}