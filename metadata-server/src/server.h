#ifndef SERVER_H
#define SERVER_H

#include "metadata/request.h"
#include "metadata/branch.h"
#include "metadata/subscriber.h"
#include "replicas/version_registry.h"
#include "replicas/replica_client.h"
#include "utils/grpc_service.h"
#include "utils/metadata.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <set>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace rendezvous {

    class Server {

        static const int REGISTER = 1;
        static const int REMOVE = -1;

        private:
            /* ------------- */
            /* config values */
            /* --------------*/
            const int _cleanup_requests_interval_m;
            const int _cleanup_requests_validity_m;
            const int _cleanup_subscribers_interval_m;
            const int _cleanup_subscribers_validity_m;
            const int _subscribers_refresh_interval_s;
            const int _wait_replica_timeout_s;

            const std::string _sid;
            std::atomic<long> _next_rid;
            
            // <rid, request_ptr>
            std::shared_mutex _mutex_requests;
            std::shared_mutex _mutex_closed_requests;
            std::unordered_map<std::string, metadata::Request*> _requests;
            std::unordered_map<std::string, metadata::Request*> _closed_requests;

            // <service w/ tag, <region, subscriber_ptr>>
            std::unordered_map<std::string, std::unordered_map<std::string, metadata::Subscriber*>> _subscribers;
            std::shared_mutex _mutex_subscribers;

        public:
            Server(std::string sid, json settings);
            Server(std::string sid);
            ~Server();

            std::atomic<int> _prevented_inconsistencies = 0;

            /**
             * Get subscriber associated with the service and region
             * 
             * @param service
             * @param region
             * @return metadata::Subscriber* 
             */
            metadata::Subscriber * getSubscriber(const std::string& service, const std::string& region);

            /**
             * Publish branches for interested subscribers
             * 
             * @param service
             * @param tag
             * @param bid 
             */
            void publishBranches(const std::string& service, const std::string& tag, const std::string& bid);

            /**
             * Process that periodically cleans old and disconnected subscribers
             * 
             */
            void initSubscribersCleanup();

            /**
             * Process that periodically cleans old and unused requests
             * 
             */
            void initRequestsCleanup();

            /**
             * Return server identifier
             * 
             * @return The server identifier 
             */
            std::string getSid();

            /**
             * Generate an identifier for a new request
             * 
             * @return The new identifier
             */
            std::string genRid();

            /**
             * Generate a (small) identifier for a new branch
             * 
             * @param request The request where the branch is going to be registered
             * @return the new identifier 
             */
            std::string genBid(metadata::Request * request);

            /**
             * Helper for parsing full id
             * - (GENERIC)      full_rid       ->   <rid, async_zone_id>
             * - (CLOSE BRANCH) composed_rid   ->   <full_rid, bid>
             * 
             * @param full_id The full id to be parsed 
             * @return rid and bid
             */
            std::pair<std::string, std::string> parseFullId(const std::string& full_id);

            /**
             * Helper for composing full id
             * - (GENERIC)      full_rid       ->   <rid, async_zone_id>
             * - (CLOSE BRANCH) composed_rid   ->   <full_rid, bid>
             * 
             * @param primary_id The first identifier
             * @param secondary_id The second identifier
             * @return new composed id in the format <primary_id:secondary_id>
             */
            std::string composeFullId(const std::string& primary_id, const std::string& secondary_id);

            /**
             * Register a new sub request originating from an async branch within the current subrequest
             * 
             * @param request Current request ptr
             * @param async_zone_id Current subrequest
             * @param gen_id If disabled, the next async_zone_id is not generated
             * @return new subrequest
            */
            std::string addNextAsyncZone(metadata::Request * request, const std::string& async_zone_id, bool gen_id = true);

            /**
             * Returns the number of inconsistencies prevented so far
             * @return The number of prevented inconsistencies
             */
            long getNumInconsistencies();

            /**
             * Get request according to the provided identifier
             * 
             * @param rid Request identifier
             * @return Request if found, otherwise return nullptr
             */
            metadata::Request * getRequest(const std::string& rid);

            /**
             * Tr
             * 
             * @param rid Request identifier
             * @return Request if found, otherwise return nullptr
             */
            metadata::Request * tryGetClosedRequest(const std::string& rid);

            /**
             * Get request according to the provided identifier or register request if not registered yet
             * 
             * @param rid Request identifier
             * @return Request if successfully registered, otherwise return nullptr
             */
            metadata::Request * getOrRegisterRequest(std::string rid);

            // helper for GTest: it calls registerBranch method
            std::string registerBranchGTest(metadata::Request * request, 
                const std::string& async_zone_id, const std::string& service, 
                const utils::ProtoVec& regions, const std::string& tag, const std::string& current_service_bid);

            /**
             * Register new branch for a given request
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param regions The regions context
             * @param tag The service tag
             * @param current_service_bid The parent service in the dependency graph
             * @param monitor Monitor branch by publishing identifier to subscribers
             * @param bid The set of branches identifier: empty if request is from client
             * @return 
             * - The new identifier (core_bid) of the set of branches 
             * - Or empty if an error ocurred (branches already exist with bid)
             */
            metadata::Branch * registerBranch(metadata::Request * request, const std::string& async_zone_id, const std::string& service, 
                const utils::ProtoVec& regions, const std::string& tag, const std::string& current_service, 
                const std::string& bid, bool monitor);

            /**
             * Close a branch according to its identifier
             * 
             * @param request Request where the branch is registered
             * @param bid The identifier of the set of branches where the current branch was registered
             * @param region Region where branch was registered
             * @param service Service where branch was registered
             * @return one of three values:
             * - 1 if branch was closed
             * - 0 if branch was already closed before
             * - (-1) if encountered error from either (i) wrong bid, wrong region, or error in sub_requests tbb map
             */
            int closeBranch(metadata::Request * request, const std::string& bid, const std::string& region);

            /**
             * Wait until request is closed for a given context (none, service, region or service and region)
             * 
             * @param request Request where the branch is registered
             * @param async_zone_id Current subrequest
             * @param service The service context
             * @param region The region we are waiting for the service on
             * @param tag  The specified operation tag for this service
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             * @param current_service Current service doing the wait call
             * @param wait_deps If enabled, it waits for all dependencies of the service
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context (service/region) was not found
             * - (-3) if context (curr service) was not found
             * - (-4) if tag was not found
             */
            int wait(metadata::Request * request, const std::string& async_zone_id, const std::string& service, 
                const::std::string& region, std::string tag = "", 
                bool async = false, int timeout = 0, std::string current_service = "", bool wait_deps = false);
            
            /**
             * Check status of the request for a given context (none, service, region or service and region)
             * 
             * @param request Request where the branch is registered
             * @param async_zone_id Current subrequest
             * @param service The service context
             * @param region The region context
             * @param detailed Enable detailed information (status for tagged branches and dependencies)
             * @return Possible return values of Status.status:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if request is UNKNOWN
             * - (-3) if context was not found
             */
            utils::Status checkStatus(metadata::Request * request, const std::string &async_zone_id,
                const std::string& service, const std::string& region, bool detailed = false);

            /**
             * Fetch dependencies in the call graph
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param async_zone_id Current subrequest
             * @return Possible return values of Dependencies.res:
             * - 0 if OK
             * - (-2) if service was not found
             * - (-3) if context was not found
             */
            utils::Dependencies fetchDependencies(metadata::Request * request, const std::string& service,
                const std::string& async_zone_id);
            
            /**
             * Get number of inconsistencies prevented so far using the blocking methods
             * 
             * @return The number of inconsistencies
             */
            long getNumPreventedInconsistencies();
        
    };
}

#endif
