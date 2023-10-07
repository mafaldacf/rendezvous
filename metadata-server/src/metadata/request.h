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
        private:
            /* track all branching information of a service */
            typedef struct ServiceNodeStruct {
                std::string name;
                int opened_global_region;
                int opened_branches;
                std::unordered_map<std::string, int> opened_regions;
                std::unordered_map<std::string, metadata::Branch*> tagged_branches;
                struct ServiceNodeStruct * parent;
                std::list<struct ServiceNodeStruct*> children;
            } ServiceNode;

        // public for testing purposes
        public:
            typedef struct SubRequestStruct {
                int i;
                std::string sub_rid;
                // protected by sub_requests mutex
                int num_current_waits;

                std::atomic<int> next_sub_rid_index;
                std::atomic<int> opened_branches;
                std::atomic<int> opened_global_region;
                oneapi::tbb::concurrent_hash_map<std::string, int> opened_regions;

            } SubRequest;

            /* ----------- */
            /* async zones */
            /* ----------- */
            // index for sub_requests
            std::atomic<int> sub_requests_i;
            // <sub_rid, sub_request_ptr>
            oneapi::tbb::concurrent_hash_map<std::string, SubRequest*> _sub_requests;
            // <sub_rid, num_current_waits>
            std::unordered_set<SubRequest*> _wait_logs;

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
            std::mutex _mutex_branches;
            std::mutex _mutex_regions;
            // service nodes
            std::shared_mutex _mutex_service_nodes;
            std::condition_variable_any _cond_service_nodes;
            // service nodes -- wait with async option
            std::condition_variable_any _cond_new_service_nodes;
            // sub requests
            std::shared_mutex _mutex_subrequests;
            std::condition_variable_any _cond_subrequests;


            /**
             * Compute remaining timeout based on the original timeout value and the elapsed time
             * 
             * @param timeout Timeout, in seconds, provided by the user
             * @param start_time
            */
            std::chrono::seconds _computeRemainingTimeout(int timeout, const std::chrono::steady_clock::time_point& start_time);
            
        // public for testing purposes
        public:
            /**
            * Verify that given sub_rid exists
            * 
            * @param sub_rid The sub request identifier
            * @return return pointer to SubRequest if found and nullptr otherwise
            */
            SubRequest *  _validateSubRid(const std::string& sub_rid);

            /**
             * Add current sub request to wait logs
             * 
             * @param subrequest The current sub request
            */
            void _addToWaitLogs(SubRequest* subrequest);

            /**
             * Remove current sub request from wait logs
             * 
             * @param subrequest The current sub request
            */
            void _removeFromWaitLogs(SubRequest* subrequest);

            /**
             * Check if first async zone precedes second async zone
             * 
             * @param subrequest_1 Subrequest ptr for the first async zone
             * @param subrequest_2 Subrequest ptr for the second async zone
             * @return true if first async zone precedes second and false otherwise
            */
            bool _isPrecedingAsyncZone(SubRequest* subrequest_1, SubRequest* subrequest_2);

            /**
             * Get all preceding entires from wait logs, i.e., smaller sub_rids than the current one
             * (used to be ignored in the wait call)
             * 
             * @param subrequest The current sub request
             * @return vector of all preceding sub rids
            */
            std::vector<std::string> _getPrecedingAsyncZones(SubRequest* subrequest);

            /**
             * Get number of opened branches for all preceding sub requests
             * 
             * @param sub_rids Vector of all preceding sub requests identifiers
             * @return number of opened branches
            */
            int _numOpenedBranchesAsyncZones(const std::vector<std::string>& sub_rids);

            /**
             * Get number of opened branches for all preceding sub requests in current region and global region
             * 
             * @param sub_rids Vector of all preceding sub requests identifiers
             * @param region Targeted region
             * @return pair for number of opened branches with format: <global region, targeted region>
            */
            std::pair<int, int> _numOpenedRegionsAsyncZones(
                const std::vector<std::string>& sub_rids, const std::string& region);

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
             * Register a new sub request originating from an async branch within the current subrequest with
             * the folllowing format: <sub_rid>:<sid>-<new_sub_rid>
             * 
             * @param sid The current server (replica) id
             * @param sub_rid Current subrequest
             * @param gen_id If disabled, the next sub_rid is not generated
             * @return new subrequest
            */
            std::string addNextSubRequest(const std::string& sid, const std::string& sub_rid, bool gen_id);

            /**
             * Register a set of branches in the request
             * 
             * @param sub_rid Current subrequest
             * @param bid The identifier of the set of branches
             * @param service The service where the branches are being registered
             * @param tag The service tag
             * @param region The regions for each branch
             * 
             * @param return branch if successfully registered and nullptr otherwise (if branches already exists)
             */
            metadata::Branch * registerBranch(const std::string& sub_rid, const std::string& bid, const std::string& service, 
                const std::string& tag, const utils::ProtoVec& regions, const std::string& prev_service);

            /**
             * Remove a branch from the request
             * 
             * @param bid The identifier of the set of branches where the current branch was registered
             * @param region The region where the branch was registered

             * @return one of three values:
             * - 1 if branch was closed
             * - 0 if branch was already closed before
             * - (-1) if encountered error from either (i) wrong bid, wrong region, or error in sub_requests tbb map
             */
            int closeBranch(const std::string& bid, const std::string& region);

            /**
             * Untrack (remove) branch according to its context (service, region or none) in the corresponding maps
             *
             * @param sub_rid Current subrequest
             * @param service The service context
             * @param region The region context
             * @param globally_closed Indicates if all regions are closed
             * 
             * @return true if successful and false otherwise
             */
            bool untrackBranch(const std::string& sub_rid, const std::string& service, 
                const std::string& region, bool globally_closed);

            /**
             * Track a set of branches (add) according to their context (service, region or none) in the corresponding maps
             *
             * @param sub_rid Current subrequest
             * @param service The service context
             * @param regions The regions for each branch
             * @param num The number of new branches
             * @param branch If we want to specify the tag for the service we need to provided the branch
             * 
             * @return true if successful and false otherwise
             */
            bool trackBranch(const std::string& sub_rid, const std::string& service, const utils::ProtoVec& regions, 
                int num, const std::string& parent, metadata::Branch * branch = nullptr);

            /**
             * Wait until request is closed
             * 
             * @param sub_rid Current sub request
             * @param prev_service Previously registered service
             * @param timeout Timeout in seconds
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-3) if prev_service is invalid
             * - (-4) if sub_rid does not exist
             */
            int wait(const std::string& sub_rid, std::string prev_service, int timeout);

            /**
             * Wait until request is closed for a given context (region)
             *
             * @param sub_rid Current subrequest
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
             * - (-4) if sub_rid does not exist
             */
            int waitRegion(const std::string& sub_rid, const std::string& region, std::string prev_service, bool async, int timeout);

            /**
             * Wait until request is closed for a given context (service)
             *
             * @param service The name of the service that defines the waiting context
             * @param tag Tag that specifies the service operation the client is waiting for (empty not specified in the request)
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             * @param wait_deps If enabled, it waits for all dependencies of the service
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context was not found
             */
            int waitService(const std::string& service, const std::string& tag, bool async, int timeout, bool wait_deps);


            /**
             * Wait until request is closed for a given context (service and region)
             *
             * @param service The name of the service that defines the waiting context
             * @param region The name of the region that defines the waiting context
             * @param tag Tag that specifies the service operation the client is waiting for (empty not specified in the request)
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             * @param wait_deps If enabled, it waits for all dependencies of the service
             *
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context (service/region) was not found
             * - (-3) if context (prev service) was not found
             * - (-4) if tag was not found
             */
            int waitServiceRegion(const std::string& service, const std::string& region, 
                const std::string& tag, bool async, int timeout, bool wait_deps);

            /**
             * Check status of request
             * @param sub_rid Current sub request
             * @param prev_service Previously registered service
             * 
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             */
            Status checkStatus(const std::string& sub_rid, const std::string& prev_service);

            /**
             * Check status of request for a given context (region)
             *
             * @param sub_rid Current sub request
             * @param region The name of the region that defines the waiting context
             * @param prev_service Previously registered service
             *
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if request is UNKNOWN
             */
            Status checkStatusRegion(const std::string& sub_rid, const std::string& region, const std::string& prev_service);

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

            /**
             * Fetch dependencies in the call graph
             * 
             * @param request Request where the branch is registered
             * @param prev_service Previously registered service
             * @return Possible return values of Dependencies.res:
             * - 0 if OK
             * - (-2) if service was not found
             * - (-3) if context was not found
             */
            utils::Dependencies fetchDependencies(const std::string& prev_service);

            /**
             * Fetch dependencies in the call graph
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @return Possible return values of Dependencies.res:
             * - 0 if OK
             * - (-2) if service was not found
             * - (-3) if context was not found
             */
            utils::Dependencies fetchDependenciesService(const std::string& service);
        };
    
}

#endif