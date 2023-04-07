#ifndef CLIENT_SERVICE_H
#define CLIENT_SERVICE_H

#include "rendezvous_server.grpc.pb.h"
#include "../metadata/request.h"
#include "../server.h"
#include "../replicas/replica_client.h"
#include "../utils.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <iostream>
#include <memory>
#include <string>

namespace service {

    class RendezvousServiceImpl final : public rendezvous::RendezvousService::Service {

        private:
            std::shared_ptr<rendezvous::Server> server;
            replicas::ReplicaClient replicaClient;

            // debugging purposes
            std::atomic<int> waitRequestCalls;

        public:
            RendezvousServiceImpl(std::shared_ptr<rendezvous::Server> server, std::vector<std::string> addrs);

            /* gRPC generated methods*/
            grpc::Status registerRequest(grpc::ServerContext * context, const rendezvous::RegisterRequestMessage * request, rendezvous::RegisterRequestResponse * response) override;
            grpc::Status registerBranch(grpc::ServerContext * context, const rendezvous::RegisterBranchMessage * request, rendezvous::RegisterBranchResponse * response) override;
            grpc::Status registerBranches(grpc::ServerContext * context, const rendezvous::RegisterBranchesMessage * request, rendezvous::RegisterBranchesResponse * response) override;
            grpc::Status closeBranch(grpc::ServerContext * context, const rendezvous::CloseBranchMessage * request, rendezvous::Empty * response) override;
            grpc::Status waitRequest(grpc::ServerContext * context, const rendezvous::WaitRequestMessage * request, rendezvous::WaitRequestResponse * response) override;
            grpc::Status checkRequest(grpc::ServerContext * context, const rendezvous::CheckRequestMessage * request, rendezvous::CheckRequestResponse * response) override;
            grpc::Status checkRequestByRegions(grpc::ServerContext * context, const rendezvous::CheckRequestByRegionsMessage * request, rendezvous::CheckRequestByRegionsResponse * response) override;
            grpc::Status getPreventedInconsistencies(grpc::ServerContext * context, const rendezvous::Empty * request, rendezvous::GetPreventedInconsistenciesResponse * response) override;
        
    };
}

#endif
