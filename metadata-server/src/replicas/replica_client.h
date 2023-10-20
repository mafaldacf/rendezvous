#ifndef REPLICA_CLIENT_H
#define REPLICA_CLIENT_H

#include "client.grpc.pb.h"
#include "server.grpc.pb.h"
#include "../utils/grpc_service.h"
#include "../utils/metadata.h"
#include "../utils/settings.h"
#include <grpcpp/grpcpp.h>
#include <string>
#include <thread>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace replicas {

    class ReplicaClient {

        public:
            // Helper structure for grpc requests
            typedef struct AsyncRequestHelperStruct {
                int nrpcs = 0;
                grpc::CompletionQueue queue;
                std::vector<std::unique_ptr<grpc::Status>> statuses;
                std::vector<std::unique_ptr<grpc::ClientContext>> contexts;
                std::vector<std::unique_ptr<rendezvous_server::Empty>> responses;
                std::vector<std::unique_ptr<grpc::ClientAsyncResponseReader<rendezvous_server::Empty>>> rpcs;
                std::mutex mutex;
            } AsyncRequestHelper;

        private:
            std::vector<std::shared_ptr<rendezvous_server::ServerService::Stub>> _servers;
            void saveAsyncCall(AsyncRequestHelper &req_helper, grpc::ClientContext * context, grpc::Status * status, rendezvous_server::Empty * response);

            /* Helpers */
            void _doRegisterRequest(const std::string& rid);
            void _doRegisterBranch(const std::string& root_rid, const std::string& async_zone, const std::string& core_bid, 
                const std::string& service, const std::string& tag, 
                const google::protobuf::RepeatedPtrField<std::string>& regions, bool monitor,
                const rendezvous_server::RequestContext& ctx_replica);
            void _doCloseBranch(const std::string& root_rid, const std::string& core_bid, const std::string& region, 
                const rendezvous_server::RequestContext& ctx_replica);
            

        public:
            ReplicaClient(std::vector<std::string> addrs);

            /**
             * Wait for completion queue of async requests
             * 
             * @param request The type of request that is being handled
             * @param cq The completion queue
             * @param statuses Vector of status for each request
             * @param rpcs Number of RPCs performed
             */
            void waitCompletionQueue(const std::string& request, AsyncRequestHelper& rh);

            /**
             * Send register request call to all replicas
             * 
             * @param rid The identifier of the request
             */
            void registerRequest(const std::string& rid);
            /**
             * Send register branches call to all replicas
             * 
             * @param root_rid The identifier of the root request
             * @param async_zone The identifier of the async zone
             * @param core_bid The identifier of the set of branches (without rid)
             * @param service The service where the branches were registered
             * @param regions The regions where the branches were registered
             * @param monitor If enabled, we publish the branch for datastore monitor subscribers
             * @param ctx_replica Context targeted ot the replica
             */
            void registerBranch(const std::string& root_rid, const std::string& async_zone, const std::string& core_bid, 
                const std::string& service, const std::string& tag, 
                const google::protobuf::RepeatedPtrField<std::string>& regions, bool monitor,
                const rendezvous_server::RequestContext& ctx_replica);

            /**
             * Send close branch call to all replicas
             * 
             * @param root_rid The identifier of the root request
             * @param core_bid bid The identifier of the set of branches generated when the branch was registered (without rid)
             * @param region The region where the branch was registered
             * @param ctx_replica Context targeted ot the replica
             */
            void closeBranch(const std::string& root_rid, const std::string& core_bid, const std::string& region, 
                const rendezvous_server::RequestContext& ctx_replica);

            /**
             * Add wait call to log entry (asynchronous broadcast)
             * 
             * @param root_rid The identifier of the root request
             * @param async_zone The identifier of the asynchronous zone where the call is made
             * @param target_service The optional target service for the wait call
             * @return async request helper structure with all the context of the requests
             */
            AsyncRequestHelper * addWaitLog(const std::string& root_rid, const std::string& async_zone, const std::string& target_service);

            /**
             * Remove wait call from log entry (asynchronous broadcast)
             * 
             * @param root_rid The identifier of the root request
             * @param async_zone The identifier of the asynchronous zone where the call is made
             * @param target_service The optional target service for the wait call
             * @param add_wait_log_async_request_helper Async request helper from addWaitLog used to wait now to prevent removing before adding
             */
            void removeWaitLog(const std::string& root_rid, const std::string& async_zone, 
                const std::string& target_service, AsyncRequestHelper * add_wait_log_async_request_helper);

        };
    
}

#endif