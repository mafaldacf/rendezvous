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

namespace metadata {

    class Request {

        typedef struct ServiceBranchingStruct {
            int num_opened_branches;
            bool global;
            std::unordered_map<std::string, int> opened_regions;
            std::unordered_map<std::string, metadata::Branch*> tagged_branches;
        } ServiceBranching;

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
            std::chrono::time_point<std::chrono::system_clock> _last_ts;
            
            /* ------------------- */
            /* replicas versioning */
            /* ------------------- */
            replicas::VersionRegistry * _versions_registry;

            /* -------------------- */
            /* branching management */
            /* -------------------- */
            // number of opened branches globally
            std::atomic<int> _num_opened_branches;
            std::unordered_map<std::string, metadata::Branch*> _branches; // <bid, branch ptr>
            std::unordered_map<std::string, ServiceBranching> _service_branching;
            std::unordered_map<std::string, int> _opened_regions;

            /* ------------------- */
            /* concurrency control */
            /* ------------------- */
            std::mutex _mutex_branches;
            std::mutex _mutex_service_branching;
            std::mutex _mutex_opened_regions;
            std::condition_variable _cond_branches;
            std::condition_variable _cond_service_branching;
            std::condition_variable _cond_opened_regions;

            // for wait with async option
            std::condition_variable _cond_new_service_branching;
            std::condition_variable _cond_new_opened_regions;

            /**
             * Compute remaining timeout based on the original timeout value and the elapsed time
             * 
             * @param timeout Timeout, in seconds, provided by the user
             * @param start_time
            */
            std::chrono::seconds _computeRemainingTimeout(int timeout, const std::chrono::steady_clock::time_point& start_time);

        public:

            Request(std::string rid, replicas::VersionRegistry * versions_registry);
            ~Request();

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
             * Register a set of branches in the request
             * 
             * @param bid The identifier of the set of branches
             * @param service The service where the branches are being registered
             * @param tag The service tag
             * @param region The regions for each branch
             * 
             * @param return branch if successfully registered and nullptr otherwise (if branches already exists)
             */
            metadata::Branch * registerBranch(const std::string& bid, const std::string& service, const std::string& tag, const utils::ProtoVec& regions);

            /**
             * Remove a branch from the request
             * 
             * @param bid The identifier of the set of branches where the current branch was registered
             * @param region The region where the branch was registered
             * 
             * @return 1 if branch was closed, 0 if branch was not found and -1 if regions does not exist
             */
            int closeBranch(const std::string& bid, const std::string& region);

            /**
             * Track branch (add or remove) according to its context (service, region or none) in the corresponding maps
             *
             * @param service The service context
             * @param region The region context
             * @param value The value (-1 if we are removing or 1 if we are adding) to be added to the current value in the map
             * @param branch If we want to specify the tag for the service we need to provided the branch
             */
            void untrackBranch(const std::string& service, const std::string& region, long value, metadata::Branch * branch = nullptr);

            /**
             * Track a set of branches (add) according to their context (service, region or none) in the corresponding maps
             *
             * @param service The service context
             * @param regions The regions for each branch
             * @param num The number of new branches
             * @param branch If we want to specify the tag for the service we need to provided the branch
             */
            void trackBranch(const std::string& service, const utils::ProtoVec& regions, int num, metadata::Branch * branch = nullptr);

            /**
             * Wait until request is closed
             * 
             * @param timeout Timeout in seconds
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             */
            int wait(int timeout);

            /**
             * Wait until request is closed for a given context (service)
             *
             * @param service The name of the service that defines the waiting context
             * @param tag Tag that specifies the service operation the client is waiting for (empty not specified in the request)
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context was not found
             */
            int waitOnService(const std::string& service, const std::string& tag, bool async, int timeout);

            /**
             * Wait until request is closed for a given context (region)
             *
             * @param region The name of the region that defines the waiting context
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context was not found
             */
            int waitOnRegion(const std::string& region, bool async, int timeout);

            /**
             * Wait until request is closed for a given context (service and region)
             *
             * @param service The name of the service that defines the waiting context
             * @param region The name of the region that defines the waiting context
             * @param tag Tag that specifies the service operation the client is waiting for (empty not specified in the request)
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context was not found
             */
            int waitOnServiceAndRegion(const std::string& service, const std::string& region, const std::string& tag, bool async, int timeout);

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