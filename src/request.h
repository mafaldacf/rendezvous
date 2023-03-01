#ifndef REQUEST_H
#define REQUEST_H

#include <iostream>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "branch.h"
#include <cstring>

namespace request {

    /* request status */
    const int OPENED = 0;
    const int CLOSED = 1;

    class Request {

    private:
        std::atomic<long> nextRID;
        std::string rid;

        // Open Branches
        std::map<long, branch::Branch*> branches;
        std::unordered_map<std::string, int> branchesPerRegion;
        std::unordered_map<std::string, int> branchesPerService;
        std::map<std::pair<std::string, std::string>, int> branchesPerServiceAndRegion;

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

        Request(std::string rid);
        ~Request();

        /**
         * Get the identifier (rid) of the object
         * 
         * @return rid
         */
        std::string getRid();

        /**
         * Atomically get and incremented next branch identifier
         * 
         * @return new branch identifier
         */
        long computeNextBID();

        void addBranch(branch::Branch * branch);

        /**
         * Remove a branch from the request
         * 
         * @param bid Identifier of the branch to be removed
         * 
         * @return 0 if successfully removed, otherwise -1 if branch was not found by its identifier
         */
        int removeBranch(long bid);

        /**
         * Track branch (add or remove) according to its context (service, region or none) in the corresponding maps
         *
         * @param value The value (-1 if we are removing or 1 if we are adding) to be added to the current value in the map
         */
        void trackBranchOnContext(branch::Branch * branch, long value);

        /**
         * Wait until request is closed
         *
         * @return Possible return values:
         *  - 0 if call did not block, 
         *  - 1 if inconsistency was prevented
         */
        int wait();

        /**
         * Wait until request is closed for a given context (service)
         *
         * @param service The name of the service that defines the waiting context
         *
         * @return Possible return values:
         * - 0 if call did not block, 
         * - 1 if inconsistency was prevented
         * - (-1) if no status was found for a given service
         */
        int waitOnService(std::string service);

        /**
         * Wait until request is closed for a given context (region)
         *
         * @param region The name of the region that defines the waiting context
         *
         * @return Possible return values:
         * - 0 if call did not block, 
         * - 1 if inconsistency was prevented
         * - (-1) if no status was found for a given region
         */
        int waitOnRegion(std::string region);

        /**
         * Wait until request is closed for a given context (service and region)
         *
         * @param service The name of the service that defines the waiting context
         * @param region The name of the region that defines the waiting context
         *
         * @return Possible return values:
         * - 0 if call did not block, 
         * - 1 if inconsistency was prevented
         * - (-1) if no status was found for a given context (service or region)
         */
        int waitOnServiceAndRegion(std::string service, std::string region);

        /**
         * Check status of request
         *
         * @return Possible return values:
         * - 0 if request is OPENED 
         * - 1 if request is CLOSED
         */
        int getStatus();

        /**
         * Check status of request for a given context (region)
         *
         * @param region The name of the region that defines the waiting context
         *
         * @return Possible return values:
         * - 0 if request is OPENED 
         * - 1 if request is CLOSED
         * - (-1) if no status was found for the given region
         */
        int getStatusOnRegion(std::string region);

        /**
         * Check status of request for a given context (service)
         *
         * @param service The name of the service that defines the waiting context
         *
         * @return Possible return values:
         * - 0 if request is OPENED 
         * - 1 if request is CLOSED
         * - (-1) if no status was found for the given service
         */
        int getStatusOnService(std::string service);

        /**
         * Check status of request for a given context (service and region)
         *
         * @param service The name of the service that defines the waiting context
         * @param region The name of the region that defines the waiting context
         *
         * @return Possible return values:
         * - 0 if request is OPENED 
         * - 1 if request is CLOSED
         * - (-1) if no status was found for a given context (service or region)
         */
        int getStatusOnServiceAndRegion(std::string service, std::string region);

        /**
         * Check status of request for each available region and for a given contex (service)
         *
         * @param status Sets value depending on the outcome:
         * - (-1) if no status was found for a given service
         *
         * @return Map of status of the request (OPENED or CLOSED) for each region
         */
        std::map<std::string, int> getStatusByRegions(int * status);

        /**
         * Check status of request for each available region and for a given contex (service)
         *
         * @param service The name of the service that defines the waiting context
         * @param status Sets value depending on the outcome:
         * - (-1) if no status was found for a given service
         * - (-2) if no status was found for any region
         *
         * @return Map of status of the request (OPENED or CLOSED) for each region
         */
        std::map<std::string, int> getStatusByRegionsOnService(std::string service, int * status);

    };
    
}

#endif