#ifndef SERVER_SERVICE_H
#define SERVER_SERVICE_H

#include "server.grpc.pb.h"
#include "../metadata/request.h"
#include "../server.h"
#include "../utils/grpc_service.h"
#include "../utils/metadata.h"
#include "../utils/settings.h"
#include <atomic>
#include <mutex>
#include <iostream>
#include <memory>
#include <string>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace service {

    class ServerServiceImpl final : public rendezvous_server::ServerService::Service {

        private:
            std::shared_ptr<rendezvous::Server> _server;
            bool _consistency_checks;

        public:
            ServerServiceImpl(std::shared_ptr<rendezvous::Server> server, bool consistency_checks);

            /* gRPC generated methods*/
            grpc::Status RegisterRequest(grpc::ServerContext * context, 
                const rendezvous_server::RegisterRequestMessage * request, 
                rendezvous_server::Empty * response) override;

            grpc::Status RegisterBranch(grpc::ServerContext * context, 
                const rendezvous_server::RegisterBranchMessage * request, 
                rendezvous_server::Empty * response) override;
            
            grpc::Status CloseBranch(grpc::ServerContext * context, 
                const rendezvous_server::CloseBranchMessage * request, 
                rendezvous_server::Empty * response) override;

            grpc::Status AddWaitLog(grpc::ServerContext * context, 
                const rendezvous_server::AddWaitLogMessage * request, 
                rendezvous_server::Empty * response) override;

            grpc::Status RemoveWaitLog(grpc::ServerContext * context, 
                const rendezvous_server::RemoveWaitLogMessage * request, 
                rendezvous_server::Empty * response) override;
    };
}

#endif
