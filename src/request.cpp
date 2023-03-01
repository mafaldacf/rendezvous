#include "request.h"

using namespace request;

Request::Request(std::string rid)
    : rid(rid), nextRID(0) {

    branches = std::map<long, branch::Branch*>();
    branchesPerRegion = std::unordered_map<std::string, int>();
    branchesPerService = std::unordered_map<std::string, int>();
    branchesPerServiceAndRegion = std::map<std::pair<std::string, std::string>, int>();
}

Request::~Request() {
    auto last = branches.end();
    for (auto pair = branches.begin(); pair != last; pair++) {
        branch::Branch * branch = pair->second;
        delete branch;
    }
}

std::string Request::getRid() {
    return rid;
}

long Request::computeNextBID() {
        return nextRID.fetch_add(1);
}

void Request::addBranch(branch::Branch * branch) {
    mutex_branches.lock();

    branches.insert({ branch->getBid(), branch });
    trackBranchOnContext(branch, 1);

    mutex_branches.unlock();
    
}

int Request::removeBranch(long bid) {
    mutex_branches.lock();

    auto pair = branches.find(bid);

    if (pair == branches.end()) {
        mutex_branches.unlock();
        return -1;
    }

    branch::Branch * branch = pair->second;
    branches.erase(bid);
    trackBranchOnContext(branch, -1);
    delete branch;

    cond_branches.notify_all();
    mutex_branches.unlock();

    return 0;
}

void Request::trackBranchOnContext(branch::Branch * branch, long value) {
    std::string service = branch->getService();
    std::string region = branch->getRegion();

    if (!service.empty() && !region.empty()) {
        mutex_branchesPerServiceAndRegion.lock();

        branchesPerServiceAndRegion[std::make_pair(service, region)] += value;
        if (value == -1)
            cond_branchesPerServiceAndRegion.notify_all();

        mutex_branchesPerServiceAndRegion.unlock();
    }

    if (!service.empty()) {
        mutex_branchesPerService.lock();

        branchesPerService[service] += value;
        if (value == -1) 
            cond_branchesPerService.notify_all();

        mutex_branchesPerService.unlock();
    }

    if (!region.empty()) {
        mutex_branchesPerRegion.lock();

        branchesPerRegion[region] += value;
        if (value == -1) 
            cond_branchesPerRegion.notify_all();
            
        mutex_branchesPerRegion.unlock();
    }
}

int Request::wait() {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(mutex_branches);
    while (branches.size() != 0) {
        cond_branches.wait(lock);
        inconsistency = 1;
    }

    return inconsistency;
}

int Request::waitOnService(std::string service) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(mutex_branchesPerService);

    if (branchesPerService.count(service) == 0) { // not found
        return -1;
    }

    int * valuePtr = &branchesPerService[service];
    while (*valuePtr != 0) {
        cond_branchesPerService.wait(lock);
        inconsistency = 1;
    }
    
    return inconsistency;
}

int Request::waitOnRegion(std::string region) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(mutex_branchesPerRegion);

    if (branchesPerRegion.count(region) == 0) { // not found
        return -1;
    }

    int * valuePtr = &branchesPerRegion[region];
    while (*valuePtr != 0) {
        cond_branchesPerRegion.wait(lock);
        inconsistency = 1;
    }

    return inconsistency;
}

int Request::waitOnServiceAndRegion(std::string service, std::string region) {
    int inconsistency = 0;

    std::unique_lock<std::mutex> lock(mutex_branchesPerServiceAndRegion);

    auto it = branchesPerServiceAndRegion.find(std::make_pair(service, region));
    if (it == branchesPerServiceAndRegion.end()) { // not found
        return -1;
    }

    int * valuePtr = &it->second;
    while (*valuePtr != 0) {
        cond_branchesPerServiceAndRegion.wait(lock);
        inconsistency = 1;
    }
    return inconsistency;
}

int Request::getStatus() {
    mutex_branches.lock();

    if (branches.size() == 0) {
        mutex_branches.unlock();
        return CLOSED;
    }

    mutex_branches.unlock();
    return OPENED;
}

int Request::getStatusOnService(std::string service) {
    mutex_branchesPerService.lock();

    if (!branchesPerService.count(service)) {
        mutex_branchesPerService.unlock();
        return -1;
    }

    if (branchesPerService[service] == 0) {
        mutex_branchesPerService.unlock();
        return CLOSED;
    }

    mutex_branchesPerService.unlock();
    return OPENED;
}

int Request::getStatusOnRegion(std::string region) {
    mutex_branchesPerRegion.lock();

    if (!branchesPerRegion.count(region)) {
        mutex_branchesPerRegion.unlock();
        return -1;
    }

    if (branchesPerRegion[region] == 0) {
        mutex_branchesPerRegion.unlock();
        return CLOSED;
    }

    mutex_branchesPerRegion.unlock();
    return OPENED;
}

int Request::getStatusOnServiceAndRegion(std::string service, std::string region) {
    mutex_branchesPerServiceAndRegion.lock();

    auto it = branchesPerServiceAndRegion.find(std::make_pair(service, region));

    if (it == branchesPerServiceAndRegion.end()) { // not found
        mutex_branchesPerServiceAndRegion.unlock();
        return -1;
    }

    if (it->second == 0) {
        mutex_branchesPerServiceAndRegion.unlock();
        return CLOSED;
    }

    mutex_branchesPerServiceAndRegion.unlock();
    return OPENED;
}

std::map<std::string, int> Request::getStatusByRegions(int * status) {
    std::map<std::string, int> result = std::map<std::string, int>();
    *status = 0;

    mutex_branchesPerRegion.lock();

    if(branchesPerRegion.begin() == branchesPerRegion.end()) {
        *status = -2;
        mutex_branchesPerRegion.unlock();
        return result;
    }

    auto last = branchesPerRegion.end();
    for (auto pair = branchesPerRegion.begin(); pair != last; pair++)
        result[pair->first] = pair->second == 0 ? CLOSED : OPENED;

    mutex_branchesPerRegion.unlock();
    
    return result;
}

std::map<std::string, int> Request::getStatusByRegionsOnService(std::string service, int * status) {
    std::map<std::string, int> result = std::map<std::string, int>();
    *status = 0;

    mutex_branchesPerServiceAndRegion.lock();

    if(branchesPerServiceAndRegion.begin() == branchesPerServiceAndRegion.end()) { // no regions
        *status = -2;
        mutex_branchesPerServiceAndRegion.unlock();
        return result;
    }

    // pair = (service, region)
    for(const auto& it : branchesPerServiceAndRegion) {
        const std::pair<std::string, std::string>& pair = it.first;
        int value = it.second;

        if (pair.first == service) { // found service
            result[pair.second] = value == 0 ? CLOSED : OPENED;
        }
    }

    mutex_branchesPerServiceAndRegion.unlock();

    if (result.empty()) { // no services with regions
        *status = -1;
    }
    
    return result;
}