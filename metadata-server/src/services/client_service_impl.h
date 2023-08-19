#ifndef CLIENT_SERVICE_H
#define CLIENT_SERVICE_H

#include "server.grpc.pb.h"
#include "../metadata/request.h"
#include "../metadata/subscriber.h"
#include "../server.h"
#include "../replicas/replica_client.h"
#include "../utils.h"
#include <atomic>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace service {

    class ClientServiceImpl final : public rendezvous::ClientService::Service {

        private:
            bool _CONSISTENCY_CHECKS;
            std::shared_ptr<rendezvous::Server> _server;
            replicas::ReplicaClient _replica_client;
            // debugging purposes
            std::atomic<int> _num_wait_calls;
            int _num_replicas;

            metadata::Request * _getRequest(const std::string& rid);

        public:
            ClientServiceImpl(std::shared_ptr<rendezvous::Server> server, std::vector<std::string> addrs);

            grpc::Status Subscribe(grpc::ServerContext * context,
                const rendezvous::SubscribeMessage * request,
                grpc::ServerWriter<rendezvous::SubscribeResponse> * writer) override;

            grpc::Status RegisterRequest(grpc::ServerContext * context, 
                const rendezvous::RegisterRequestMessage * request, 
                rendezvous::RegisterRequestResponse * response) override;

            grpc::Status RegisterBranch(grpc::ServerContext * context, 
                const rendezvous::RegisterBranchMessage * request, 
                rendezvous::RegisterBranchResponse * response) override;

            grpc::Status RegisterBranchesDatastores(grpc::ServerContext * context, 
                const rendezvous::RegisterBranchesDatastoresMessage * request, 
                rendezvous::RegisterBranchesDatastoresResponse * response) override;

            grpc::Status CloseBranch(grpc::ServerContext * context, 
                const rendezvous::CloseBranchMessage * request, 
                rendezvous::Empty * response) override;

            grpc::Status WaitRequest(grpc::ServerContext * context, 
                const rendezvous::WaitRequestMessage * request, 
                rendezvous::WaitRequestResponse * response) override;

            grpc::Status CheckRequest(grpc::ServerContext * context, 
                const rendezvous::CheckRequestMessage * request, 
                rendezvous::CheckRequestResponse * response) override;

            grpc::Status CheckRequestByRegions(grpc::ServerContext * context, 
                const rendezvous::CheckRequestByRegionsMessage * request, 
                rendezvous::CheckRequestByRegionsResponse * response) override;
        
    };
}

#endif
