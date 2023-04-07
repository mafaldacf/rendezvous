#ifndef SERVER_SERVICE_H
#define SERVER_SERVICE_H

#include "rendezvous_server.grpc.pb.h"
#include "../metadata/request.h"
#include "../server.h"
#include "../utils.h"
#include <atomic>
#include <mutex>
#include <iostream>
#include <memory>
#include <string>

namespace service {

    class RendezvousServerServiceImpl final : public rendezvous_server::RendezvousServerService::Service {

        private:
            std::shared_ptr<rendezvous::Server> server;

        public:
            RendezvousServerServiceImpl(std::shared_ptr<rendezvous::Server> server);

            /* gRPC generated methods*/
            grpc::Status registerRequest(grpc::ServerContext * context, const rendezvous_server::RegisterRequestMessage * request, rendezvous_server::Empty * response) override;
            grpc::Status registerBranch(grpc::ServerContext * context, const rendezvous_server::RegisterBranchMessage * request, rendezvous_server::Empty * response) override;
            grpc::Status registerBranches(grpc::ServerContext * context, const rendezvous_server::RegisterBranchesMessage * request, rendezvous_server::Empty * response) override;
            grpc::Status closeBranch(grpc::ServerContext * context, const rendezvous_server::CloseBranchMessage * request, rendezvous_server::Empty * response) override;
    };
}

#endif
