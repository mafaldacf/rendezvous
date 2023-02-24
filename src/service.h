#ifndef SERVICE_H
#define SERVICE_H

#include "monitor.grpc.pb.h"
#include "request.h"
#include <atomic>
#include <mutex>

namespace service {

    class MonitorServiceImpl final : public monitor::MonitorService::Service {

        private:
            std::atomic<long> nextRID{0};
            std::map<long, request::Request*> requests = std::map<long, request::Request*>();

            // Concurrency Control
            std::mutex mutex_requests;

        public:
            request::Request * getRequest(long);

            grpc::Status registerRequest(grpc::ServerContext*, const monitor::Empty* request, monitor::RegisterRequestResponse*) override;
            grpc::Status registerBranch(grpc::ServerContext*, const monitor::RegisterBranchMessage* request, monitor::RegisterBranchResponse*) override;
            grpc::Status registerBranches(grpc::ServerContext*, const monitor::RegisterBranchesMessage* request, monitor::RegisterBranchesResponse*) override;
            grpc::Status closeBranch(grpc::ServerContext*, const monitor::CloseBranchMessage* request, monitor::Empty*) override;
            grpc::Status waitRequest(grpc::ServerContext*, const monitor::WaitRequestMessage* request, monitor::Empty*) override;
            grpc::Status checkRequest(grpc::ServerContext*, const monitor::CheckRequestMessage* request, monitor::CheckRequestResponse*) override;
            grpc::Status checkRequestByRegions(grpc::ServerContext*, const monitor::CheckRequestByRegionsMessage* request, monitor::CheckRequestByRegionsResponse*) override;
        
    };
}

#endif
