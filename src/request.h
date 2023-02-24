#ifndef REQUEST_H
#define REQUEST_H

#include <iostream>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "utils.h"

#include "branch.h"

using namespace utils;

namespace request {

    class Request {

    private:
        std::atomic<long> nextRID;
        long rid;

        // Open Branches
        std::map<long, branch::Branch*> branches;
        std::map<std::string, long> branchesPerRegion;
        std::map<std::string, long> branchesPerService;
        std::map<std::string, std::map<std::string, long>> branchesPerServiceAndRegion;

        // Concurrency Control
        std::mutex mutex_branches;
        std::mutex mutex_branchesPerRegion;
        std::mutex mutex_branchesPerService;
        std::mutex mutex_branchesPerServiceAndRegion;
        
        std::condition_variable cond_branches;
        std::condition_variable cond_branchesPerRegion;
        std::condition_variable cond_branchesPerService;
        std::condition_variable cond_branchesPerServiceAndRegion;

    public:

        Request(long rid);

        long computeNextBID();

        void addBranch(branch::Branch*);
        int removeBranch(long bid);

        void trackBranchOnContext(branch::Branch*, long);

        void waitRequest(std::string, std::string);
        int checkRequest(std::string, std::string);
        std::map<std::string, int> checkRequestByRegions(std::string);
    };
    
}

#endif