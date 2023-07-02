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

using json = nlohmann::json;

namespace rendezvous {

    class Server {

        private:
            static const int REGISTER = 1;
            static const int REMOVE = -1;

            /* Garbage collector for old requests and subscribers */
            const int _requests_cleanup_sleep_m;
            const int _subscribers_cleanup_sleep_m;
            const int _subscribers_max_wait_time_s;
            const int _wait_replica_timeout_s;

            const std::string _sid;
            std::atomic<long> _next_rid;
            std::atomic<long> _prevented_inconsistencies;
            
            // <rid, request_ptr>
            std::unordered_map<std::string, metadata::Request*> _requests;
            std::shared_mutex _mutex_requests;

            // <service w/ tag, <region, subscriber_ptr>>
            std::unordered_map<std::string, std::unordered_map<std::string, metadata::Subscriber*>> _subscribers;
            std::shared_mutex _mutex_subscribers;

        public:
            Server(std::string sid, json settings);
            Server(std::string sid);
            ~Server();

            /**
             * Get subscriber associated with the subscriber id
             * 
             * @param service 
             * @param tag
             * @param region
             * @return metadata::Subscriber* 
             */
            metadata::Subscriber * getSubscriber(const std::string& service, const std::string& tag, const std::string& region);

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
             * Generate an identifier for a new branch
             * 
             * @param request The request where the branch is going to be registered
             * @return the new identifier 
             */
            std::string genBid(metadata::Request * request);

            /**
             * Get rid from bid
             * 
             * @param bid 
             * @return std::string 
             */
            std::string parseRid(std::string bid);

            /**
             * Compute subscriber id from service and tag
             * 
             * @param service 
             * @param tag 
             * @return std::string 
             */
            std::string computeSubscriberId(const std::string& service, const std::string& tag);

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
             * Register a new branch for a given request
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param region The region context
             * @param tag The service tag
             * @param bid The set of branches identifier: empty if request is from client
             * @return The new branch identifiers or empty if an error ocurred (branches already exist with bid)
             */
            std::string registerBranch(metadata::Request * request, const std::string& service, const std::string& region, const std::string& tag, std::string bid = "");

            /**
             * Register new branches for a given request
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param regions The regions context for each branch
             * @param tag The service tag
             * @param bid The set of branches identifier: empty if request is from client
             * @return The new identifier of the set of branches or empty if an error ocurred (branches already exist with bid)
             */
            std::string registerBranches(metadata::Request * request, const std::string& service, const utils::ProtoVec& regions, const std::string& tag, std::string bid = "");

            /**
             * Close a branch according to its identifier
             * 
             * @param request Request where the branch is registered
             * @param bid The identifier of the set of branches where the current branch was registered
             * @param region Region where branch was registered
             * @param service Service where branch was registered
             * @param client_request True if request comes from client and false if request comes from replica
             * @return 1 if branch was closed, 0 if branch was not found and -1 if regions does not exist
             */
            int closeBranch(metadata::Request * request, const std::string& bid, const std::string& region, bool client_request = false);

            /**
             * Wait until request is closed for a given context (none, service, region or service and region)
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param timeout Timeout in seconds
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             */
            int waitRequest(metadata::Request * request, const std::string& service, const std::string& region, int timeout = 0);
            
            /**
             * Check status of the request for a given context (none, service, region or service and region)
             * 
             * @param request Request where the branch is registered
             * @param service The service context
             * @param region The region context
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - 2 if context was not found
             */
            int checkRequest(metadata::Request * request, const std::string& service, const std::string& region);
            
            /**
             * Check status of request for each available region and for a given contex (service)
             *
             * @param request Request where the branch is registered
             * @param service The name of the service that defines the waiting context
             *
             * @return Map of status of the request (OPENED or CLOSED) for each region
             */
            
            std::map<std::string, int> checkRequestByRegions(metadata::Request * request, const std::string& service);
            
            /**
             * Get number of inconsistencies prevented so far using the blocking methods
             * 
             * @return The number of inconsistencies
             */
            long getNumPreventedInconsistencies();
        
    };
}

#endif
