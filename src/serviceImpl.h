#ifndef SERVICE_H
#define SERVICE_H

#include "monitor.grpc.pb.h"
#include "request.h"
#include <atomic>
#include <mutex>
#include "server.h"

#ifndef DEBUG 
#define DEBUG 1 // set debug mode
#endif

#if DEBUG
#define log(...) {\
    char str[100];\
    sprintf(str, __VA_ARGS__);\
    std::cout << "[" << __FUNCTION__ << "] " << str << std::endl;\
    }
#else
#define log(...)
#endif

namespace service {

    const std::string ERROR_MESSAGE_CONTEXT_NOT_FOUND = "Request status not found for the provided context (service or region)";
    const std::string ERROR_MESSAGE_SERVICE_NOT_FOUND = "Request status not found for the provided service";
    const std::string ERROR_MESSAGE_INVALID_REQUEST = "Invalid request";
    const std::string ERROR_MESSAGE_INVALID_BRANCH = "Invalid branch for the provided request";
    const std::string ERROR_MESSAGE_REGIONS_NOT_FOUND = "Request status not found for any region";

    class MonitorServiceImpl final : public monitor::MonitorService::Service {

        private:
            server::Server server;

        public:

            /* gRPC generated methods*/
            grpc::Status registerRequest(grpc::ServerContext * context, const monitor::RegisterRequestMessage * request, monitor::RegisterRequestResponse * response) override;
            grpc::Status registerBranch(grpc::ServerContext * context, const monitor::RegisterBranchMessage * request, monitor::RegisterBranchResponse * response) override;
            grpc::Status registerBranches(grpc::ServerContext * context, const monitor::RegisterBranchesMessage * request, monitor::RegisterBranchesResponse * response) override;
            grpc::Status closeBranch(grpc::ServerContext * context, const monitor::CloseBranchMessage * request, monitor::Empty * response) override;
            grpc::Status waitRequest(grpc::ServerContext * context, const monitor::WaitRequestMessage * request, monitor::Empty * response) override;
            grpc::Status checkRequest(grpc::ServerContext * context, const monitor::CheckRequestMessage * request, monitor::CheckRequestResponse * response) override;
            grpc::Status checkRequestByRegions(grpc::ServerContext * context, const monitor::CheckRequestByRegionsMessage * request, monitor::CheckRequestByRegionsResponse * response) override;
            grpc::Status getPreventedInconsistencies(grpc::ServerContext * context, const monitor::Empty * request, monitor::GetPreventedInconsistenciesResponse * response) override;
        
    };
}

#endif
