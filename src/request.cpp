#include "request.h"

using namespace metadata;

Request::Request(std::string rid)
    : rid(rid), nextId(0), numBranches(0) {
    
    /* Branches */
    // nested map: <service, <region, vector with branch pointers>>
    using Branches = std::vector<metadata::Branch*>;
    branches = std::unordered_map<std::string, std::unordered_map<std::string, Branches>>();

    /* Opened Branches */
    numBranchesPerRegion = std::unordered_map<std::string, int>();
    numBranchesPerService = std::unordered_map<std::string, int>();
}

Request::~Request() {
    for (const auto& service_it : branches)
        for (const auto& region_it : service_it.second)
            for (auto& branch : region_it.second)
                delete branch;
}

std::string Request::getRid() {
    return rid;
}

long Request::computeNextId() {
    return nextId.fetch_add(1);
}

void Request::addBranch(metadata::Branch * branch) {
    mutex_branches.lock();
    branches[branch->getService()][branch->getRegion()].push_back(branch);
    trackBranchOnContext(branch, 1);
    mutex_branches.unlock();
    
}

int Request::closeBranch(const std::string& service, const std::string& region, const std::string& bid) {
    mutex_branches.lock();

    auto service_it = branches.find(service);
    if (service_it == branches.end()) {
        mutex_branches.unlock();
        return -2;
    }

    auto region_it = service_it->second.find(region);
    if (region_it == service_it->second.end()) {
        mutex_branches.unlock();
        return -3;
    }

    int found = -4;
    for (const auto& branch : region_it->second) {
        if (branch->getBid() == bid) {
            found = 0;

            // branch was already closed
            if (branch->isClosed()) {
                cond_branches.notify_all();
                mutex_branches.unlock();
                return 0;
            }

            branch->close();
            trackBranchOnContext(branch, -1);
            break;
        }
    }

    cond_branches.notify_all();
    mutex_branches.unlock();

    return found;
}

void Request::trackBranchOnContext(metadata::Branch * branch, long value) {
    std::string service = branch->getService();
    std::string region = branch->getRegion();

    numBranches.fetch_add(value);
    if (!service.empty()) {
        mutex_numBranchesPerService.lock();

        numBranchesPerService[service] += value;
        if (value == -1) 
            cond_numBranchesPerService.notify_all();

        mutex_numBranchesPerService.unlock();
    }
    if (!region.empty()) {
        mutex_numBranchesPerRegion.lock();

        numBranchesPerRegion[region] += value;
        if (value == -1) 
            cond_numBranchesPerRegion.notify_all();
            
        mutex_numBranchesPerRegion.unlock();
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

    std::unique_lock<std::mutex> lock(mutex_numBranchesPerService);

    if (numBranchesPerService.count(service) == 0) { // not found
        return -2;
    }

    int * valuePtr = &numBranchesPerService[service];
    while (*valuePtr != 0) {
        cond_numBranchesPerService.wait(lock);
        inconsistency = 1;
    }
    
    return inconsistency;
}

int Request::waitOnRegion(const std::string& region) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(mutex_numBranchesPerRegion);

    if (numBranchesPerRegion.count(region) == 0) { // not found
        return -3;
    }

    int * valuePtr = &numBranchesPerRegion[region];
    while (*valuePtr != 0) {
        cond_numBranchesPerRegion.wait(lock);
        inconsistency = 1;
    }

    return inconsistency;
}

bool Request::hasOpenedBranches(std::vector<metadata::Branch*> branches) {
    for (const auto& branch : branches) {
        if (!branch->isClosed()) {
            return true;
        }
    }
    return false;
}

int Request::waitOnServiceAndRegion(const std::string& service, const std::string& region) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(mutex_branches);

    auto service_it = branches.find(service);
    if (service_it == branches.end()) {
        return -2;
    }

    auto region_it = service_it->second.find(region);
    if (region_it == service_it->second.end()) {
        return -3;
    }

    while (hasOpenedBranches(region_it->second)) {
        cond_branches.wait(lock);
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
    mutex_numBranchesPerService.lock();

    if (!numBranchesPerService.count(service)) {
        mutex_numBranchesPerService.unlock();
        return -2;
    }

    if (numBranchesPerService[service] == 0) {
        mutex_numBranchesPerService.unlock();
        return CLOSED;
    }

    mutex_numBranchesPerService.unlock();
    return OPENED;
}

int Request::getStatusOnRegion(const std::string& region) {
    mutex_numBranchesPerRegion.lock();

    if (!numBranchesPerRegion.count(region)) {
        mutex_numBranchesPerRegion.unlock();
        return -3;
    }

    if (numBranchesPerRegion[region] == 0) {
        mutex_numBranchesPerRegion.unlock();
        return CLOSED;
    }

    mutex_numBranchesPerRegion.unlock();
    return OPENED;
}

int Request::getStatusOnServiceAndRegion(const std::string& service, const std::string& region) {
    mutex_branches.lock();

    auto service_it = branches.find(service);
    if (service_it == branches.end()) {
        mutex_branches.unlock();
        return -2;
    }

    auto region_it = service_it->second.find(region);
    if (region_it == service_it->second.end()) {
        mutex_branches.unlock();
        return -3;
    }

    if (hasOpenedBranches(region_it->second)) {
        mutex_branches.unlock();
        return OPENED;
    }

    mutex_branches.unlock();
    return CLOSED;
}

std::map<std::string, int> Request::getStatusByRegions() {
    std::map<std::string, int> result = std::map<std::string, int>();

    mutex_numBranchesPerRegion.lock();
    for (const auto& it : numBranchesPerRegion) {
        result[it.first] = it.second == 0 ? CLOSED : OPENED;
    }
    mutex_numBranchesPerRegion.unlock();
    
    return result;
}

std::map<std::string, int> Request::getStatusByRegionsOnService(const std::string& service, int * status) {
    std::map<std::string, int> result = std::map<std::string, int>();
    *status = 0;

    mutex_branches.lock();
    auto service_it = branches.find(service);

    // no status found for a given service
    if (service_it == branches.end()) {
        *status = -2;
        mutex_branches.unlock();
        return result;
    }

    // nested map: <service, <region, vector of branch pointers>>
    for (const auto& region_it : service_it->second) {
        // skip branches with no region
        // ideally this should not be necessary
        if (!region_it.first.empty()) {
            result[region_it.first] = hasOpenedBranches(region_it.second) ? OPENED : CLOSED;
        }
    }
    
    mutex_branches.unlock();
    
    return result;
}