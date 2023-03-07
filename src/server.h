#ifndef SERVER_H
#define SERVER_H

#include "rendezvous.grpc.pb.h"
#include "request.h"
#include "branch.h"
#include <atomic>
#include <mutex>
#include <iostream>
#include <memory>
#include <string>

namespace server {

    class Server {

        private:
            std::string sid;
            std::atomic<long> nextRid;
            std::unordered_map<std::string, metadata::Request*> * requests;

            // Track number of prevented inconsistencies
            std::atomic<long> inconsistencies;

            // Concurrency Control
            std::mutex mutex_requests;

        public:
            Server(std::string sid);
            ~Server();

            /**
             * Get request according to the provided identifier or register request if not registered yet
             * 
             * @param rid Request identifier
             * @return Request if successfully registered, otherwise return nullptr
             */
            metadata::Request * getOrRegisterRequest(std::string rid);

            /**
             * Get request according to the provided identifier
             * 
             * @param rid Request identifier
             * @return Request if found, otherwise return nullptr
             */
            metadata::Request * getRequest(const std::string& rid);

            /**
             * Register a new request
             * 
             * @param rid Request identifier: empty or not
             * @return The new request if it was not yet registered, otherwise return nullptr
             */
            metadata::Request * registerRequest(std::string rid);

            /**
             * Register a new branch for a given request
             * 
             * @param request Request where the branch is registered
             * @param service The service context (optional)
             * @param region The region context (optional)
             * @return The new branch identifiers
             */
            std::string registerBranch(metadata::Request * request, const std::string& service, const std::string& region);

            /**
             * Register new branches for a given request
             * 
             * @param request Request where the branch is registered
             * @param service The service context (optional)
             * @param region The regions context for each branch
             * @return The new identifier of the set of branches
             */
            std::string registerBranches(metadata::Request * request, const std::string& service, const google::protobuf::RepeatedPtrField<std::string>& regions);

            /**
             * Remove a branch according to its identifier
             * 
             * @param rid Request identifier
             * @param service Service where branch was registered
             * @param region Region where branch was registered
             * @param bid The identifier of the set of branches where the current branch was registered
             * @return Possible return values:
             * - 0 if branch was successfully removed
             * - (-1) if rid is invalid
             * - (-2) if no branch was found for a given service
             * - (-3) if no branch was found for a given region
             */
            int closeBranch(const std::string& rid, const std::string& service, const std::string& region, const std::string& bid);

            /**
             * Wait until request is closed for a given context (none, service, region or service and region)
             * 
             * @param rid Request identifier
             * @param service The service context (optional)
             * @param region The region context (optional)
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if rid is invalid
             * - (-2) if no status was found for a given service
             * - (-3) if no status was found for a given region
             */
            int waitRequest(const std::string& rid, const std::string& service, const std::string& region);
            
            /**
             * Check status of the request for a given context (none, service, region or service and region)
             * 
             * @param rid Request identifier
             * @param service The service context (optional)
             * @param region The region context (optional)
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - (-1) if rid is invalid
             * - (-2) if no status was found for a given service
             * - (-3) if no status was found for a given region
             */
            int checkRequest(const std::string& rid, const std::string& service, const std::string& region);
            
            /**
             * Check status of request for each available region and for a given contex (service)
             *
             * @param rid Request identifier
             * @param service The name of the service that defines the waiting context (optional)
             * @param status Sets value depending on the outcome:
             * - (-1) if rid is invalid
             * - (-2) if no status was found for a given service
             *
             * @return Map of status of the request (OPENED or CLOSED) for each region
             */
            
            std::map<std::string, int> checkRequestByRegions(const std::string& rid, const std::string& service, int * status);
            
            /**
             * Get number of inconsistencies prevented so far using the blocking methods
             * 
             * @return The number of inconsistencies
             */
            long getPreventedInconsistencies();
        
    };
}

#endif
