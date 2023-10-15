#ifndef REQUEST_H
#define REQUEST_H

#include "branch.h"
#include "../replicas/version_registry.h"
#include "../utils/grpc_service.h"
#include "../utils/metadata.h"
#include "../utils/settings.h"
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
#include <unordered_set>
#include <stack>

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_vector.h"

using namespace utils;

namespace metadata {

    class Request {

        // public for testing purposes
        public:
            /* track all branching information of a service */
            typedef struct ServiceNodeStruct {
                std::string name;
                std::unordered_map<std::string, int> async_zone_ids_opened_branches;
                int opened_global_region; // FIXME: CONVERT TO ATOMIC DUE TO ThE WAIT LOGS
                int opened_branches;
                int num_current_waits;
                std::unordered_map<std::string, int> opened_regions;
                std::unordered_map<std::string, std::vector<metadata::Branch*>> tagged_branches;
                std::vector<struct ServiceNodeStruct*> children;

                // concurrency control
                std::mutex mutex;

            } ServiceNode;

            typedef struct AsyncZoneStruct {
                std::string async_zone_id;
                int i;
                // protected by async_zones mutex
                int num_current_waits;

                std::atomic<int> next_async_zone_index;
                std::atomic<int> opened_branches;
                std::atomic<int> opened_global_region;
                oneapi::tbb::concurrent_hash_map<std::string, int> opened_regions;

            } AsyncZone;

        private:
            /* ----------- */
            /* async zones */
            /* ----------- */
            // index for async_zones
            std::atomic<int> async_zones_i;
            // <async_zone_id, async_zone_ptr>
            oneapi::tbb::concurrent_hash_map<std::string, AsyncZone*> _async_zones;
            // <async_zone_id, num_current_waits>
            std::set<AsyncZone*> _wait_logs;
            std::unordered_map<std::string, std::set<ServiceNode*>> _service_wait_logs;

            /* ------- */
            /* helpers */
            /* ------- */
            const std::string _rid;
            std::atomic<long> _next_bid_index;
            std::atomic<int> _next_sub_rid_index;
            std::chrono::time_point<std::chrono::system_clock> _last_ts;
            
            /* ------------------- */
            /* replicas versioning */
            /* ------------------- */
            replicas::VersionRegistry * _versions_registry;

            /* -------------------- */
            /* branching management */
            /* -------------------- */
            std::atomic<int> _num_opened_branches;
            std::atomic<int> _opened_global_region;
            // <region, num opened branches>
            std::unordered_map<std::string, int> _opened_regions; // FIXME: use tbb
            // <bid, branch_ptr>
            std::unordered_map<std::string, metadata::Branch*> _branches; //FIXME: use tbb
            // <service name, service_node_ptr>
            std::unordered_map<std::string, ServiceNode*> _service_nodes;

            /* ------------------- */
            /* concurrency control */
            /* ------------------- */
            // logs
            std::shared_mutex _mutex_service_wait_logs;
            // branches
            std::mutex _mutex_branches;
            std::condition_variable _cond_new_branch;
            // regions
            std::mutex _mutex_regions;
            // service nodes
            std::shared_mutex _mutex_service_nodes;
            std::condition_variable_any _cond_service_nodes;
            // service nodes -- wait with async option
            std::condition_variable_any _cond_new_service_nodes;
            // async zones
            std::shared_mutex _mutex_async_zones;
            std::condition_variable_any _cond_async_zones;

            /**
             * Wait for the branch's registration
             * 
             * @param bid The branch identifier
             * @return pointer to the branch if found and nullptr otherwise
            */
            metadata::Branch * _waitBranchRegistration(const std::string& bid);

            /**
             * Get all the following dependencies for the current service node
             * 
             * @param service_node Pointer for the current service node
             * @param async_zone_id Current async zone id
             * @return return vector of all following dependencies
            */
            std::vector<ServiceNode*> _getAllFollowingDependencies(ServiceNode * service_node, 
                const std::string& async_zone_id);

            /**
             * Helper for wait logic in service
             * 
             * @param service_node Pointer for the current service node
             * @param async_zone_id Current async zone id
             * @param timeout Timeout set by client
             * @param start_time Start time
             * @param remaining_timeout Remaining timeout
             * @return if inconsistency was prevented or not
            */
            int _doWaitService(ServiceNode * service_node, const std::string& async_zone_id,
                int timeout, const std::chrono::steady_clock::time_point& start_time, 
                std::chrono::seconds remaining_timeout);

            /**
             * Helper for wait logic in service and region
             * 
             * @param service_node Pointer for the current service node
             * @param region Region to be waited for
             * @param async_zone_id Current async zone id
             * @param timeout Timeout set by client
             * @param start_time Start time
             * @param remaining_timeout Remaining timeout
             * @return if inconsistency was prevented or not
            */
            int _doWaitServiceRegion(ServiceNode * service_node, const std::string& region, const std::string& async_zone_id,
                int timeout, const std::chrono::steady_clock::time_point& start_time, 
                std::chrono::seconds remaining_timeout);

            /**
             * Helper for wait logic in tag for service and service and region
             * 
             * @param service_node Pointer for the current service node
             * @param tag Tag to be waited for
             * @param region Region to be waited for (can be empty)
             * @param async_zone_id Current async zone id
             * @param timeout Timeout set by client
             * @param start_time Start time
             * @param remaining_timeout Remaining timeout
             * @return if inconsistency was prevented or not
            */
            int _doWaitTag(ServiceNode * service_node, const std::string& tag, const std::string& region, 
                int timeout, const std::chrono::steady_clock::time_point& start_time, 
                std::chrono::seconds remaining_timeout);
        
        public:


            /**
             * Compute remaining timeout based on the original timeout value and the elapsed time
             * 
             * @param timeout Timeout, in seconds, provided by the user
             * @param start_time The starting time of the call
            */
            std::chrono::seconds _computeRemainingTimeout(int timeout, const std::chrono::steady_clock::time_point& start_time);
            
            /**
             * Validates the service node
             * 
             * @param service The service name
             * @return The pointer to the service node if valid and nullptr otherwise
            */
            ServiceNode * validateServiceNode(const std::string& service);

            /**
             * Add current service node to wait logs
             * 
             * @param service_node The current service node
             * @param target_service The target service to be waited for
            */
            void _addToServiceWaitLogs(ServiceNode* service_node, const std::string& target_service);

            /**
             * Remove current service node from wait logs
             * 
             * @param service_node The current service node
             * @param target_service The target service to be waited for
            */
            void _removeFromServiceWaitLogs(ServiceNode* service_node, const std::string& target_service);

            /**
             * Gets number of opened branches for services waiting on current service
             * 
             * @param current_service The current service node
             * @return pair for number of opened branches
            */
            int _numOpenedBranchesServiceLogs(const std::string& current_service);

            /**
             * Gets number of opened branches for services waiting on current service
             * 
             * @param current_service The current service node
             * @param service The current service
             * @return pair for number of opened branches with format: <global region, targeted region>
            */
            std::pair<int, int> _numOpenedRegionsServiceLogs(const std::string& current_service, const std::string& region);

        // public for testing purposes
        public:
            /**
            * Verify that given async_zone_id exists
            * 
            * @param async_zone_id The async zone id identifier
            * @return return pointer to AsyncZone if found and nullptr otherwise
            */
            AsyncZone * _validateAsyncZone(const std::string& async_zone_id);

            /**
             * Add current async zone to wait logs
             * 
             * @param async_zone The current async zone
            */
            void _addToWaitLogs(AsyncZone* async_zone);

            /**
             * Remove current async zone from wait logs
             * 
             * @param async_zone The current async zone
            */
            void _removeFromWaitLogs(AsyncZone* subrequest);

            /**
             * Check if first async zone precedes second async zone
             * 
             * @param subrequest_1 Subrequest ptr for the first async zone
             * @param subrequest_2 Subrequest ptr for the second async zone
             * @return true if first async zone precedes second and false otherwise
            */
            bool _isPrecedingAsyncZone(AsyncZone* subrequest_1, AsyncZone* subrequest_2);

            /**
             * Get all preceding entires from wait logs, i.e., smaller sub_rids than the current one
             * (used to be ignored in the wait call)
             * 
             * @param subrequest The current async zone id
             * @return vector of all preceding sub rids
            */
            std::vector<std::string> _getPrecedingAsyncZones(AsyncZone* subrequest);

            /**
             * Get number of opened branches for all preceding async zone ids
             * 
             * @param sub_rids Vector of all preceding async zone ids identifiers
             * @return number of opened branches
            */
            int _numOpenedBranchesAsyncZones(const std::vector<std::string>& sub_rids);

            /**
             * Get number of opened branches for all preceding async zone ids in current region and global region
             * 
             * @param sub_rids Vector of all preceding async zone ids identifiers
             * @param region Targeted region
             * @return pair for number of opened branches with format: <global region, targeted region>
            */
            std::pair<int, int> _numOpenedRegionsAsyncZones(
                const std::vector<std::string>& sub_rids, const std::string& region);

            /**
             * Wait until the first branch is registered
             * @param timeout Timeout, in seconds, provided by the user
             * @param start_time The starting time of the call
             * 
             */
            bool _waitFirstBranch(const std::chrono::steady_clock::time_point& start_time, int timeout);

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
             * Register a new async zone id originating from an async branch within the current subrequest with
             * the folllowing format: <async_zone_id>:<sid>-<new_sub_rid>
             * 
             * @param sid The current server (replica) id
             * @param async_zone_id Current subrequest
             * @param gen_id If disabled, the next async_zone_id is not generated
             * @return new subrequest
            */
            std::string addNextAsyncZone(const std::string& sid, const std::string& async_zone_id, bool gen_id);

            /**
             * Inserts new async zone if it does not exist yet
             * 
             * @param async_zone_id The identifier for th current async zone
            */
            void insertAsyncZone(const std::string& async_zone_id);

            /**
             * Register a set of branches in the request
             * 
             * @param async_zone_id Current subrequest
             * @param bid The identifier of the set of branches
             * @param service The service where the branches are being registered
             * @param tag The service tag
             * @param regions The regions for each branch
             * @param current_service The parent service
             * 
             * @param return branch if successfully registered and nullptr otherwise (if branches already exists)
             */
            metadata::Branch * registerBranch(const std::string& async_zone_id, const std::string& bid, const std::string& service, 
                const std::string& tag, const utils::ProtoVec& regions, const std::string& current_service);

            /**
             * Remove a branch from the request
             * 
             * @param bid The identifier of the set of branches where the current branch was registered
             * @param region The region where the branch was registered

             * @return one of three values:
             * - 1 if branch was closed
             * - 0 if branch was already closed before
             * - (-1) if encountered error from either (i) wrong bid, wrong region, or error in async_zones tbb map
             */
            int closeBranch(const std::string& bid, const std::string& region);

            /**
             * Untrack (remove) branch according to its context (service, region or none) in the corresponding maps
             *
             * @param async_zone_id Current subrequest
             * @param service The service context
             * @param region The region context
             * @param globally_closed Indicates if all regions are closed
             * 
             * @return true if successful and false otherwise
             */
            bool untrackBranch(const std::string& async_zone_id, const std::string& service, 
                const std::string& region, bool globally_closed);

            /**
             * Track a set of branches (add) according to their context (service, region or none) in the corresponding maps
             *
             * @param async_zone_id Current subrequest
             * @param service The service context
             * @param regions The regions for each branch
             * @param num The number of new branches
             * @param num Parent service
             * @param branch If we want to specify the tag for the service we need to provided the branch
             * 
             * @return true if successful and false otherwise
             */
            bool trackBranch(const std::string& async_zone_id, const std::string& service, const utils::ProtoVec& regions, 
                int num, const std::string& parent, metadata::Branch * branch = nullptr);

            /**
             * Wait until request is closed
             * 
             * @param async_zone_id Current async zone id
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             * @param async_zone_id Current asynchronous zone
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-3) current_service not found
             * - (-4) if async_zone_id does not exist
             */
            int wait(const std::string& async_zone_id, bool async, int timeout, const std::string& current_service);

            /**
             * Wait until request is closed for a given context (region)
             *
             * @param async_zone_id Current subrequest
             * @param region The name of the region that defines the waiting context
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             * @param async_zone_id Current asynchronous zone
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context was not found
             * - (-3) current_service not found
             * - (-4) if async_zone_id does not exist
             */
            int waitRegion(const std::string& async_zone_id, const std::string& region, bool async, int timeout, const std::string& current_service);

            /**
             * Wait until request is closed for a given context (service)
             *
             * @param async_zone_id Current asynchronous zone
             * @param service The name of the service that defines the waiting context
             * @param tag Tag that specifies the service operation the client is waiting for (empty not specified in the request)
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             * @param current_service The current service name
             * @param wait_deps If enabled, it waits for all dependencies of the service
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-3) current_service not found
             * - (-2) if context was not found
             */
            int waitService(const std::string& async_zone_id, 
                const std::string& service, const std::string& tag, bool async, 
                int timeout, const std::string& current_service, bool wait_deps);


            /**
             * Wait until request is closed for a given context (service and region)
             *
             * @param async_zone_id Current asynchronous zone
             * @param service The name of the service that defines the waiting context
             * @param region The name of the region that defines the waiting context
             * @param tag Tag that specifies the service operation the client is waiting for (empty not specified in the request)
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             * @param current_service The current service name
             * @param wait_deps If enabled, it waits for all dependencies of the service
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context (service/region) was not found
             * - (-3) current_service not found
             * - (-4) if tag was not found
             */
            int waitServiceRegion(const std::string& async_zone_id, 
                const std::string& service, const std::string& region, 
                const std::string& tag, bool async, int timeout,
                const std::string& current_service, bool wait_deps);

            /**
             * Check status of request
             * @param async_zone_id Current async zone id
             * 
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - (-3) if async_zone_id does not exist
             */
            Status checkStatus(const std::string& async_zone_id);

            /**
             * Check status of request for a given context (region)
             *
             * @param async_zone_id Current async zone id
             * @param region The name of the region that defines the waiting context
             *
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if request is UNKNOWN
             * - (-3) if async_zone_id does not exist
             */
            Status checkStatusRegion(const std::string& async_zone_id, const std::string& region);

            /**
             * Check status of request for a given context (service)
             *
             * @param async_zone_id Current async zone id
             * @param service The name of the service that defines the waiting context
             * @param detailed Detailed description of status for all tagged branches
             *
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if request is UNKNOWN
             */
            Status checkStatusService(const std::string& async_zone_id, 
                const std::string& service, bool detailed = false);

            /**
             * Check status of request for a given context (service and region)
             *
             * @param async_zone_id Current async zone id
             * @param service The name of the service that defines the waiting context
             * @param region The name of the region that defines the waiting context
             * @param detailed Detailed description of status for all tagged branches
             *
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if request is UNKNOWN
             */
            Status checkStatusServiceRegion(const std::string& async_zone_id, 
                const std::string& service, const std::string& region, bool detailed = false);

            /**
             * Fetch dependencies in the call graph
             * 
             * @param async_zone_id Current async zone id
             * @param service The service context
             * @return Possible return values of Dependencies.res:
             * - 0 if OK
             * - (-2) if service was not found
             * - (-3) if async_zone_id does not exist
             */
            utils::Dependencies fetchDependencies(const std::string& service, const std::string& async_zone_id);
        };
    
}

#endif