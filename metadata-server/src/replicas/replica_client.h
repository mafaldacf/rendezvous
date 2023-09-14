#ifndef REPLICA_CLIENT_H
#define REPLICA_CLIENT_H

#include "client.grpc.pb.h"
#include "server.grpc.pb.h"
#include "../utils.h"
#include <grpcpp/grpcpp.h>
#include <string>
#include <thread>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace replicas {

    class ReplicaClient {

        // Helper structure for grpc requests
        struct RequestHelper {
            int nrpcs = 0;
            grpc::CompletionQueue queue;
            std::vector<std::unique_ptr<grpc::Status>> statuses;
            std::vector<std::unique_ptr<grpc::ClientContext>> contexts;
            std::vector<std::unique_ptr<rendezvous_server::Empty>> responses;
            std::vector<std::unique_ptr<grpc::ClientAsyncResponseReader<rendezvous_server::Empty>>> rpcs;
        };

        private:
            std::vector<std::shared_ptr<rendezvous_server::ServerService::Stub>> _servers;
            bool _async_replication;

            /**
             * Wait for completion queue of async requests
             * 
             * @param request The type of request that is being handled
             * @param cq The completion queue
             * @param statuses Vector of status for each request
             * @param rpcs Number of RPCs performed
             */
            void _waitCompletionQueue(const std::string& request, struct RequestHelper& rh);

            /* Helpers */
            void _doRegisterRequest(const std::string& rid);
            void _doRegisterBranch(const std::string& rid, const std::string& bid, const std::string& service, 
                const std::string& tag, const google::protobuf::RepeatedPtrField<std::string>& regions, bool monitor,
                const rendezvous::RequestContext& ctx);
            void _doCloseBranch(const std::string& bid, const std::string& region, 
                const rendezvous::RequestContext& ctx);

        public:
            ReplicaClient(std::vector<std::string> addrs, bool async_replication);

            /**
             * Send register request call to all replicas
             * 
             * @param rid The identifier of the request
             */
            void registerRequest(const std::string& rid);
            /**
             * Send register branches call to all replicas
             * 
             * @param rid The identifier of the request associated with the branch
             * @param bid The identifier of the set of branches
             * @param service The service where the branches were registered
             * @param regions The regions where the branches were registered
             * @param id The id of the current replica
             * @param version The request version of the current replica
             */
            void registerBranch(const std::string& rid, const std::string& bid, const std::string& service, 
                const std::string& tag, const google::protobuf::RepeatedPtrField<std::string>& regions, bool monitor,
                const rendezvous::RequestContext& ctx);

            /**
             * Send close branch call to all replicas
             * 
             * @param bid The identifier of the set of branches generated when the branch was registered
             * @param region The region where the branch was registered
             * @param id The id of the current replica
             * @param version The request version of the current replica
             */
            void closeBranch(const std::string& bid, const std::string& region, 
                const rendezvous::RequestContext& ctx);

        };
    
}

#endif