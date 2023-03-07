#ifndef REQUEST_H
#define REQUEST_H

#include <iostream>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <vector>
#include "branch.h"

namespace metadata {

    class Request {

        /* request status */
        const int OPENED = 0;
        const int CLOSED = 1;

        private:
            std::atomic<long> nextId;
            std::string rid;

            /* Branches */
            // nested map: <service, <region, vector with branch pointers>>
            using Branches = std::vector<metadata::Branch*>;
            std::unordered_map<std::string, std::unordered_map<std::string, Branches>> branches;


            /* Opened Branches */
            std::atomic<int> numBranches;
            std::unordered_map<std::string, int> numBranchesPerRegion;
            std::unordered_map<std::string, int> numBranchesPerService;

            /*  Concurrency Control */
            std::mutex mutex_branches;
            std::mutex mutex_numBranchesPerRegion;
            std::mutex mutex_numBranchesPerService;
            
            std::condition_variable cond_branches;
            std::condition_variable cond_numBranchesPerRegion;
            std::condition_variable cond_numBranchesPerService;

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
             * Atomically get and incremented next identifier to be part of a new bid
             * 
             * @return new identifier
             */
            long computeNextId();

            void addBranch(metadata::Branch * branch);

            /**
             * Remove a branch from the request
             * 
             * @param service The service where the branch was registered
             * @param region The region where the branch was registered
             * @param bid The identifier of the set of branches where the current branch was registered
             * 
             * @return Possible return values:
             * - 0 if successfully removed
             * - (-2) if no branch was found for a given service
             * - (-3) if no branch was found for a given region
             * - (-4) if no branch was found with a given bid
             */
            int closeBranch(const std::string& service, const std::string& region, const std::string& bid);

            /**
             * Track branch (add or remove) according to its context (service, region or none) in the corresponding maps
             *
             * @param value The value (-1 if we are removing or 1 if we are adding) to be added to the current value in the map
             */
            void trackBranchOnContext(metadata::Branch * branch, long value);

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
             * - (-2) if no status was found for a given service
             */
            int waitOnService(const std::string& service);

            /**
             * Wait until request is closed for a given context (region)
             *
             * @param region The name of the region that defines the waiting context
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-3) if no status was found for a given region
             */
            int waitOnRegion(const std::string& region);

            /**
             * Wait until request is closed for a given context (service and region)
             *
             * @param service The name of the service that defines the waiting context
             * @param region The name of the region that defines the waiting context
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-2) if no status was found for a given service
             * - (-3) if no status was found for a given region
             */
            int waitOnServiceAndRegion(const std::string& service, const std::string& region);

            /**
             * Check if branches are opened for a given service and region
             * 
             * @param branches The vector containing branches for a given context
             * @return true if at least one branch is opened and false otherwise 
             */
            bool hasOpenedBranches(std::vector<metadata::Branch*> branches);

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
             * - (-3) if no status was found for a given region
             */
            int getStatusOnRegion(const std::string& region);

            /**
             * Check status of request for a given context (service)
             *
             * @param service The name of the service that defines the waiting context
             *
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - (-2) if no status was found for a given service
             */
            int getStatusOnService(const std::string& service);

            /**
             * Check status of request for a given context (service and region)
             *
             * @param service The name of the service that defines the waiting context
             * @param region The name of the region that defines the waiting context
             *
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - (-2) if no status was found for a given service
             * - (-3) if no status was found for a given region
             */
            int getStatusOnServiceAndRegion(const std::string& service, const std::string& region);

            /**
             * Check status of request for each available region and for a given contex (service)
             *
             * @return Map of status of the request (OPENED or CLOSED) for each region
             */
            std::map<std::string, int> getStatusByRegions();

            /**
             * Check status of request for each available region and for a given contex (service)
             *
             * @param service The name of the service that defines the waiting context
             * @param status Sets value depending on the outcome:
             * - (-2) if no status was found for a given service
             *
             * @return Map of status of the request (OPENED or CLOSED) for each region
             */
            std::map<std::string, int> getStatusByRegionsOnService(const std::string& service, int * status);

        };
    
}

#endif