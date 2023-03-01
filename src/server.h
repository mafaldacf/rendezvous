#ifndef SERVER_H
#define SERVER_H

#include "monitor.grpc.pb.h"
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
            std::atomic<long> nextRID;
            std::map<std::string, request::Request*> * requests;

            // Track number of prevented inconsistencies
            std::atomic<long> inconsistencies;

            // Concurrency Control
            std::mutex mutex_requests;

        public:
            Server();
            ~Server();

            /**
             * Get request according to the provided identifier
             * 
             * @param rid Request identifier
             * @return Request if found, otherwise return nullptr
             */
            request::Request * getRequest(std::string rid);

            /**
             * Register a new request
             * 
             * @param rid Request identifier: empty or not
             * @return Request the new request 
             */
            request::Request * registerRequest(std::string rid);

            /**
             * Register a new branch for a given request
             * 
             * @param request Request where the branch is registered
             * @param service The service context (optional)
             * @param region The region context (optional)
             * @return Branch identifier (bid)
             */
            long registerBranch(request::Request * request, std::string service, std::string region);

            /**
             * Remove a branch for a given request
             * 
             * @param request Request where the branch is present
             * @param bid Branch identifier
             * @return Possible return values:
             * - 0 if branch was successfully removed 
             * - (-1) if bid is invalid
             */
            int closeBranch(request::Request * request, long bid);

            /**
             * Wait until request is closed for a given context (none, service, region or service and region)
             * 
             * @param request The request we want to wait for
             * @param service The service context (optional)
             * @param region The region context (optional)
             * @return Possible return values:
             * - 0 if call did not block, 
             * - 1 if inconsistency was prevented
             * - (-1) if no status was found for a given context (service or region)
             */
            int waitRequest(request::Request * request, std::string service, std::string region);
            
            /**
             * Check status of the request for a given context (none, service, region or service and region)
             * 
             * @param request The request we want to wait for
             * @param service The service context (optional)
             * @param region The region context (optional)
             * @return Possible return values:
             * - 0 if request is OPENED 
             * - 1 if request is CLOSED
             * - (-1) if no status was found for a given context (service or region)
             */
            int checkRequest(request::Request * request, std::string service, std::string region);
            
            /**
             * Check status of request for each available region and for a given contex (service)
             *
             * @param service The name of the service that defines the waiting context (optional)
             * @param status Sets value depending on the outcome:
             * - (-1) if no status was found for a given service
             * - (-2) if no status was found for any region
             *
             * @return Map of status of the request (OPENED or CLOSED) for each region
             */
            
            std::map<std::string, int> checkRequestByRegions(request::Request * request, std::string service, int * status);
            
            /**
             * Get number of inconsistencies prevented so far using the blocking methods
             * 
             * @return The number of inconsistencies
             */
            long getPreventedInconsistencies();
        
    };
}

#endif
