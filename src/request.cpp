#include "request.h"

using namespace request;

Request::Request(long rid)
    : rid(rid), nextRID(0) {

    branches = std::map<long, branch::Branch*>();
    branchesPerRegion = std::map<std::string, long>();
    branchesPerService = std::map<std::string, long>();
    branchesPerServiceAndRegion = std::map<std::string, std::map<std::string, long>>();
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

    cond_branches.notify_all();
    mutex_branches.unlock();

    delete branch;

    return 0;
}

void Request::trackBranchOnContext(branch::Branch * branch, long value) {
    std::string service = branch->getService();
    std::string region = branch->getRegion();

    if (!service.empty() && !region.empty()) {
        mutex_branchesPerServiceAndRegion.lock();

        branchesPerServiceAndRegion[service][region] += value;
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

void Request::waitRequest(std::string service, std::string region) {
    if (!service.empty() && !region.empty()) {
        std::unique_lock<std::mutex> lock(mutex_branchesPerServiceAndRegion);
        while (branchesPerServiceAndRegion[service][region] != 0)
            cond_branchesPerServiceAndRegion.wait(lock);
    }   

    else if (!service.empty()) {
        std::unique_lock<std::mutex> lock(mutex_branchesPerService);
        while (branchesPerService[service] != 0)
            cond_branchesPerService.wait(lock);
    }

    else if (!region.empty()) {
        std::unique_lock<std::mutex> lock(mutex_branchesPerRegion);
        while (branchesPerRegion[region] != 0)
            cond_branchesPerRegion.wait(lock);
    }

    else {
        std::unique_lock<std::mutex> lock(mutex_branches);
        while (branches.size() != 0) {
            cond_branches.wait(lock);
        }
    }
}

int Request::checkRequest(std::string service, std::string region) {
    int status = OPENED;

    if (!service.empty() && !region.empty()) {
        mutex_branchesPerServiceAndRegion.lock();

        if (branchesPerServiceAndRegion[service][region] == 0)
            status = CLOSED;
        
        mutex_branchesPerServiceAndRegion.unlock();
    }  

    else if (!service.empty()) {
        mutex_branchesPerService.lock();

        if (branchesPerService[service] == 0)
            status = CLOSED;
        
        mutex_branchesPerService.unlock();
    }

    else if (!region.empty()) {
        mutex_branchesPerRegion.lock();

        if (branchesPerRegion[region] == 0)
            status = CLOSED;
        
        mutex_branchesPerRegion.unlock();
    }

    else {
        mutex_branches.lock();

        if (branches.size() == 0)
            status = CLOSED;

        mutex_branches.unlock();
    }

    return status;
}

std::map<std::string, int> Request::checkRequestByRegions(std::string service) {
    std::map<std::string, int> result = std::map<std::string, int>();

    if (!service.empty()) {
        mutex_branchesPerServiceAndRegion.lock();

        auto last = branchesPerServiceAndRegion[service].end();
        for (auto pair = branchesPerServiceAndRegion[service].begin(); pair != last; pair++)
            result[pair->first] = pair->second == 0 ? CLOSED : OPENED;

        mutex_branchesPerServiceAndRegion.unlock();
    }

    else {
        mutex_branchesPerRegion.lock();

        auto last = branchesPerRegion.end();
        for (auto pair = branchesPerRegion.begin(); pair != last; pair++)
            result[pair->first] = pair->second == 0 ? CLOSED : OPENED;

        mutex_branchesPerRegion.unlock();
    }
    
    return result;
}