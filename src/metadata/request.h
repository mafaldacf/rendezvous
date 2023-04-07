#ifndef REQUEST_H
#define REQUEST_H

#include "branch.h"
#include "../replicas/version_registry.h"
#include "../utils.h"
#include <iostream>
#include <map>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <vector>
#include <string>
#include <stdint.h>

namespace metadata {

    class Request {

        /* request status */
        static const int OPENED = 0;
        static const int CLOSED = 1;

        /* branch tracking */
        const int REGISTER = 1;
        const int REMOVE = -1;

        private:
            const std::string rid;
            std::atomic<long> nextId;

            // replica version control
            replicas::VersionRegistry * versionsRegistry;

            // <bid, branch>
            std::unordered_map<std::string, metadata::Branch*> branches;


            /* Opened Branches */
            std::atomic<uint64_t> numBranches;
            std::unordered_map<std::string, uint64_t> numBranchesService;
            std::unordered_map<std::string, uint64_t> numBranchesRegion;
            std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> numBranchesServiceRegion;

            /*  Concurrency Control */
            std::mutex mutex_branches;
            std::mutex mutex_numBranchesService;
            std::mutex mutex_numBranchesRegion;
            std::mutex mutex_numBranchesServiceRegion;
            
            std::condition_variable cond_branches;
            std::condition_variable cond_numBranchesService;
            std::condition_variable cond_numBranchesRegion;
            std::condition_variable cond_numBranchesServiceRegion;

        public:

            Request(std::string rid, replicas::VersionRegistry * versionsRegistry);
            ~Request();

            /**
             * Get the identifier (rid) of the object
             * 
             * @return rid
             */
            std::string getRid();

            /**
             * Get versions registry
             * 
             * @return registry's pointer
             */
            replicas::VersionRegistry * getVersionsRegistry();

            /**
             * Generate a new identifier
             * 
             * @return new id
             */
            std::string genId();

            /**
             * Register a branch in the request
             * 
             * @param bid The identifier of the set of branches where the current branch is going to be registered
             * @param service The service where the branch is being registered
             * @param region The region where the branch is being registered
             */
            void registerBranch(const std::string& bid, const std::string& service, const std::string& region);

            /**
             * Register a set of branches in the request
             * 
             * @param bid The identifier of the set of branches
             * @param service The service where the branches are being registered
             * @param region The regions for each branch
             */
            void registerBranches(const std::string& bid, const std::string& service, const utils::ProtoVec& regions);

            /**
             * Remove a branch from the request
             * 
             * @param service The service where the branch was registered
             * @param region The region where the branch was registered
             * @param bid The identifier of the set of branches where the current branch was registered
             * 
             * @return true if branch was found and closed and false if no branch was found and a new closed one was created
             */
            bool closeBranch(const std::string& service, const std::string& region, const std::string& bid);

            /**
             * Track branch (add or remove) according to its context (service, region or none) in the corresponding maps
             *
             * @param service The service context
             * @param region The region context
             * @param value The value (-1 if we are removing or 1 if we are adding) to be added to the current value in the map
             */
            void trackBranchOnContext(const std::string& service, const std::string& region, const long& value);

            /**
             * Track a set of branches (add or remove) according to their context (service, region or none) in the corresponding maps
             *
             * @param service The service context
             * @param regions The regions for each branch
             * @param value The value (-1 if we are removing or 1 if we are adding) to be added to the current value in the map
             * @param num The number of new branches
             */
            void trackBranchesOnContext(const std::string& service, const utils::ProtoVec& regions, const long& value, const int& num);

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