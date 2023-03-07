#ifndef SERVICE_H
#define SERVICE_H

#include "rendezvous.grpc.pb.h"
#include "request.h"
#include <atomic>
#include <mutex>
#include "server.h"
#include <iostream>
#include <memory>
#include <string>

// debug mode
#ifndef DEBUG 
#define DEBUG 1
#endif

#if DEBUG
#define log(...) {\
    char str[255];\
    sprintf(str, __VA_ARGS__);\
    std::cout << "[" << __FUNCTION__ << "] " << str << std::endl;\
    }
#else
#define log(...)
#endif

// measure overhead of API without any consistency checks
#ifndef NO_CONSISTENCY_CHECKS 
#define NO_CONSISTENCY_CHECKS 0
#endif 

namespace service {

    const std::string ERROR_MESSAGE_SERVICE_NOT_FOUND = "Request status not found for the provided service";
    const std::string ERROR_MESSAGE_REGION_NOT_FOUND = "Request status not found for the provided region";
    const std::string ERROR_MESSAGE_INVALID_REQUEST = "Invalid request identifier";
    const std::string ERROR_MESSAGE_INVALID_BRANCH = "No branch was found with the provided bid";
    const std::string ERROR_MESSAGE_INVALID_BRANCH_SERVICE = "No branch was found for the provided service";
    const std::string ERROR_MESSAGE_INVALID_BRANCH_REGION = "No branch was found for the provided service";
    const std::string ERROR_MESSAGE_EMPTY_REGION = "Region cannot be empty";
    const std::string ERROR_MESSAGE_REQUEST_ALREADY_EXISTS = "A request was already registered with the provided identifier";

    class RendezvousServiceImpl final : public rendezvous::RendezvousService::Service {

        private:
            server::Server server;

        public:
            RendezvousServiceImpl(std::string sid);

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
