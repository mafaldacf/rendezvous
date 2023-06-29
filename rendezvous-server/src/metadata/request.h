#ifndef REQUEST_H
#define REQUEST_H

#include "branch.h"
#include "../replicas/version_registry.h"
#include "../utils.h"
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <cstring>
#include <vector>
#include <string>
#include <stdint.h>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <thread>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

using json = nlohmann::json;

namespace metadata {

    class Request {

        /* request status */
        static const int OPENED = 0;
        static const int CLOSED = 1;
        static const int UNKNOWN = 2;

        /* branch tracking */
        static const int REGISTER = 1;
        static const int REMOVE = -1;

        private:

            const std::string _rid;
            std::atomic<long> _next_id;
            std::chrono::time_point<std::chrono::system_clock> _init_ts;
            std::chrono::time_point<std::chrono::system_clock> _last_ts;

            /* replicas versioning */
            replicas::VersionRegistry * _versions_registry;

            /* branching management */

            // <bid, branch>
            std::unordered_map<std::string, metadata::Branch*> _branches;

            // number of opened branches
            std::atomic<uint64_t> _num_branches;
            std::unordered_map<std::string, uint64_t> _num_branches_service;
            std::unordered_map<std::string, uint64_t> _num_branches_region;
            std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> _num_branches_service_region;

            // concurrency control
            std::mutex _mutex_branches;
            std::mutex _mutex_num_branches_service;
            std::mutex _mutex_num_branches_region;
            std::mutex _mutex_num_branches_service_region;
            
            std::condition_variable _cond_new_branches;
            std::condition_variable _cond_branches;
            std::condition_variable _cond_num_branches_service;
            std::condition_variable _cond_num_branches_region;
            std::condition_variable _cond_num_branches_service_region;

        public:

            Request(std::string rid, replicas::VersionRegistry * versions_registry);
            ~Request();

            /**
             * Stores request info in json format
             * 
             * @return json 
             */
            json toJson() const;

            /**
             * Return timestamp of last modification
             */
            std::chrono::time_point<std::chrono::system_clock> getLastTs();

            /**
             * Refresh timestamp of last modification to now
             * 
             */
            void refreshLastTs();

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
             * @param tag The service tag
             * @param region The region where the branch is being registered
             * 
             * @param return branch if successfully registered and nullptr otherwise (if branch already exists)
             */
            metadata::Branch * registerBranch(const std::string& bid, const std::string& service, const std::string& tag, const std::string& region);

            /**
             * Register a set of branches in the request
             * 
             * @param bid The identifier of the set of branches
             * @param service The service where the branches are being registered
             * @param tag The service tag
             * @param region The regions for each branch
             * 
             * @param return branch if successfully registered and nullptr otherwise (if branches already exists)
             */
            metadata::Branch * registerBranches(const std::string& bid, const std::string& service, const std::string& tag, const utils::ProtoVec& regions);

            /**
             * Remove a branch from the request
             * 
             * @param bid The identifier of the set of branches where the current branch was registered
             * @param region The region where the branch was registered
             * @param service to be modified
             * @param tag to be modified
             * 
             * @return 1 if branch was closed, 0 if branch was not found and -1 if regions does not exist
             */
            int closeBranch(const std::string& bid, const std::string& region, std::string& service, std::string& tag);

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
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - 2 if context was not found
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
             * - 2 if context was not found
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
             * - 2 if context was not found
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
             * - 2 if context was not found
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
             * - 2 if context was not found
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
             * - 2 if context was not found
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
             * - 2 if context was not found
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
             *
             * @return Map of status of the request (OPENED or CLOSED) for each region
             */
            std::map<std::string, int> getStatusByRegionsOnService(const std::string& service);

        };
    
}

#endif