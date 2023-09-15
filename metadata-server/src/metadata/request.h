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

using namespace utils;

namespace metadata {

    class Request {
        private:
            /* track all branching information of a service */
            typedef struct ServiceNodeStruct {
                std::string name;
                int num_opened_branches;
                std::unordered_map<std::string, int> opened_regions;
                std::unordered_map<std::string, metadata::Branch*> tagged_branches;
                struct ServiceNodeStruct * parent;
                std::list<struct ServiceNodeStruct*> children;
            } ServiceNode;

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
            // <bid, branch ptr>
            std::unordered_map<std::string, metadata::Branch*> _branches;
            // <service, service branching ptr>
            std::unordered_map<std::string, ServiceNode*> _service_nodes;

            /* ------------------- */
            /* concurrency control */
            /* ------------------- */
            std::mutex _mutex_branches;
            std::mutex _mutex_service_nodes;
            std::condition_variable _cond_branches;
            std::condition_variable _cond_service_nodes;

            // for wait with async option
            std::condition_variable _cond_new_service_nodes;

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
            metadata::Branch * registerBranch(const std::string& bid, const std::string& service, 
                const std::string& tag, const utils::ProtoVec& regions, const std::string& prev_service);

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
             * Untrack (remove) branch according to its context (service, region or none) in the corresponding maps
             *
             * @param service The service context
             * @param region The region context
             * 
             * @return true if successful and false otherwise
             */
            bool untrackBranch(const std::string& service, const std::string& region, bool fully_closed);

            /**
             * Track a set of branches (add) according to their context (service, region or none) in the corresponding maps
             *
             * @param service The service context
             * @param regions The regions for each branch
             * @param num The number of new branches
             * @param branch If we want to specify the tag for the service we need to provided the branch
             * 
             * @return true if successful and false otherwise
             */
            bool trackBranch(const std::string& service, const utils::ProtoVec& regions, int num, const std::string& parent,
                metadata::Branch * branch = nullptr);

            /**
             * Wait until request is closed
             * 
             * @param prev_service Previously registered service
             * @param timeout Timeout in seconds
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-3) if prev_service is invalid
             */
            int wait(std::string prev_service, int timeout);

            /**
             * Wait until request is closed for a given context (region)
             *
             * @param region The name of the region that defines the waiting context
             * @param prev_service Previously registered service
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context was not found
             * - (-3) if prev_service is invalid
             */
            int waitRegion(const std::string& region, std::string prev_service, bool async, int timeout);

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
            int waitService(const std::string& service, const std::string& tag, bool async, int timeout);


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
            int waitServiceRegion(const std::string& service, const std::string& region, 
                const std::string& tag, bool async, int timeout);

            /**
             * Check status of request
             * @param detailed Detailed description of status for all tagged branches
             * @param prev_service Previously registered service
             * @param detailed Enable detailed information (status for tagged branches and dependencies)
             * 
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             */
            Status checkStatus(std::string prev_service = "", bool detailed = false);

            /**
             * Check status of request for a given context (region)
             *
             * @param region The name of the region that defines the waiting context
             * @param detailed Detailed description of status for all tagged branches
             * @param prev_service Previously registered service
             * @param detailed Enable detailed information (status for tagged branches and dependencies)
             *
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if request is UNKNOWN
             */
            Status checkStatusRegion(const std::string& region, std::string prev_service = "", bool detailed = false);

            /**
             * Check status of request for a given context (service)
             *
             * @param service The name of the service that defines the waiting context
             * @param detailed Detailed description of status for all tagged branches
             *
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if request is UNKNOWN
             */
            Status checkStatusService(const std::string& service, bool detailed = false);

            /**
             * Check status of request for a given context (service and region)
             *
             * @param service The name of the service that defines the waiting context
             * @param region The name of the region that defines the waiting context
             * @param detailed Detailed description of status for all tagged branches
             *
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if request is UNKNOWN
             */
            Status checkStatusServiceRegion(const std::string& service, const std::string& region, bool detailed = false);
        };
    
}

#endif