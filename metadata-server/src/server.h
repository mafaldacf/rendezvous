#ifndef SERVER_H
#define SERVER_H

#include "metadata/request.h"
#include "metadata/branch.h"
#include "metadata/subscriber.h"
#include "replicas/version_registry.h"
#include "replicas/replica_client.h"
#include "utils.h"
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

        private:
            static const int REGISTER = 1;
            static const int REMOVE = -1;

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
            std::unordered_map<std::string, metadata::Request*> _requests;

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
             * Generate an full identifier from rid and bid
             * 
             * @param request
             * @param bid
             * @return the new identifier 
             */
            std::string getFullBid(metadata::Request * request, const std::string& bid);

            /**
             * Parse rid and bid from full bid
             * 
             * @param full_bid 
             * @return rid and bid
             */
            std::pair<std::string, std::string> parseFullBid(const std::string& full_bid);

            /**
             * Returns the number of inconsistencies prevented so far
             * 
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
             * Get request according to the provided identifier or register request if not registered yet
             * 
             * @param rid Request identifier
             * @return Request if successfully registered, otherwise return nullptr
             */
            metadata::Request * getOrRegisterRequest(std::string rid);

            /**
             * Register a new branch for a region
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param region The region context
             * @param tag The service tag
             * @param bid The set of branches identifier: empty if request is from client
             * @return The new branch identifiers or empty if an error ocurred (branches already exist with bid)
             */
            std::string registerBranchRegion(metadata::Request * request, const std::string& service, 
                const std::string& region, const std::string& tag, std::string bid = "");
            /**
             * Register new branch for a given request
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param regions The regions context
             * @param tag The service tag
             * @param prev_service The parent service in the dependency graph
             * @param monitor Monitor branch by publishing identifier to subscribers
             * @param bid The set of branches identifier: empty if request is from client
             * @return The new identifier of the set of branches or empty if an error ocurred (branches already exist with bid)
             */
            std::string registerBranch(metadata::Request * request, const std::string& service, 
                const utils::ProtoVec& regions, const std::string& tag, const std::string& prev_service, 
                bool monitor = false, std::string bid = "");

            /**
             * Close a branch according to its identifier
             * 
             * @param request Request where the branch is registered
             * @param bid The identifier of the set of branches where the current branch was registered
             * @param region Region where branch was registered
             * @param service Service where branch was registered
             * @param force Force waiting until branch is registered
             * @return 1 if branch was closed, 0 if branch was not found and -1 if regions does not exist
             */
            int closeBranch(metadata::Request * request, const std::string& bid, 
                const std::string& region, bool force = false);

            /**
             * Wait until request is closed for a given context (none, service, region or service and region)
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param region The region we are waiting for the service on
             * @param tag The specified operation tag for this service
             * @param prev_service Previously registered service
             * @param async Force to wait for asynchronous creation of a single branch
             * @param timeout Timeout in seconds
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if timeout was reached
             * - (-2) if context (service/region) was not found
             * - (-3) if context (prev service) was not found
             * - (-4) if tag was not found
             */
            int wait(metadata::Request * request, const std::string& service, const::std::string& region, 
                std::string tag = "", std::string prev_service = "", bool async = false, int timeout = 0);
            
            /**
             * Check status of the request for a given context (none, service, region or service and region)
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param region The region context
             * @param prev_service Previously registered service
             * @param detailed Enable detailed information (status for tagged branches and dependencies)
             * @return Possible return values of Status.status:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if request is UNKNOWN
             * - (-3) if context was not found
             */
            utils::Status checkStatus(metadata::Request * request, const std::string& service, 
                const std::string& region, std::string prev_service = "", bool detailed = false);

            /**
             * Fetch dependencies in the call graph
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param prev_service Previously registered service
             * @return Possible return values of Dependencies.res:
             * - 0 if OK
             * - (-2) if service was not found
             * - (-3) if context was not found
             */
            utils::Dependencies fetchDependencies(metadata::Request * request, const std::string& service, 
                std::string prev_service = "");
            
            /**
             * Get number of inconsistencies prevented so far using the blocking methods
             * 
             * @return The number of inconsistencies
             */
            long getNumPreventedInconsistencies();
        
    };
}

#endif
