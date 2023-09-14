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
            bool _consistency_checks;
            bool _async_replication;
            std::shared_ptr<rendezvous::Server> _server;
            replicas::ReplicaClient _replica_client;
            
            // debugging purposes
            std::atomic<int> _num_wait_calls;
            int _num_replicas;

            metadata::Request * _getRequest(const std::string& rid);

        public:
            ClientServiceImpl(std::shared_ptr<rendezvous::Server> server, std::vector<std::string> addrs, 
                bool async_replication);

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

            grpc::Status Wait(grpc::ServerContext * context, 
                const rendezvous::WaitMessage * request, 
                rendezvous::WaitResponse * response) override;

            grpc::Status CheckStatus(grpc::ServerContext * context, 
                const rendezvous::CheckStatusMessage * request, 
                rendezvous::CheckStatusResponse * response) override;
        
    };
}

#endif
