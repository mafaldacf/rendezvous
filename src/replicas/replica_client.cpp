#include "replica_client.h"

using namespace replicas;

ReplicaClient::ReplicaClient(std::vector<std::string> addrs) {
    // by default, addrs does not contain the address of the current replica
    for (const auto& addr : addrs) {
      auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
      auto stub = rendezvous_server::RendezvousServerService::NewStub(channel);
      servers.push_back(std::move(stub));
    }
}

void ReplicaClient::waitCompletionQueue(const std::string& request, struct RequestHelper& rh) {
    for(int i = 0; i < rh.nrpcs; i++) {
        void * tagPtr;
        bool ok = false;

        rh.completionQueue.Next(&tagPtr, &ok);
        const size_t tag = size_t(tagPtr);
        const grpc::Status & status = *(rh.statuses[tag-1].get());

        if (status.ok()) {
            log("[%s] RPC #%zu OK", request.c_str(), tag);
        }
        else {
            log("[%s] RPC #%zu ERROR: %s", request.c_str(), tag, status.error_message().c_str());
        }
    }
}

void ReplicaClient::sendRegisterRequest(const std::string& rid) {
    std::thread([this, rid]() {
        struct RequestHelper rh;

        for (const auto& server : servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            rh.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            rh.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            rh.responses.emplace_back(response);

            rendezvous_server::RegisterRequestMessage request;
            request.set_rid(rid);
            
            rh.rpcs.emplace_back(server->AsyncregisterRequest(context, request, &rh.completionQueue));
            rh.rpcs[rh.nrpcs]->Finish(response, status, (void*)1);

            rh.nrpcs++;
        }

        waitCompletionQueue("Register Request", rh);

     }).detach();
}

void ReplicaClient::sendRegisterBranch(const std::string& rid, const std::string& bid, const std::string& service, const std::string& region, const std::string& id, const int& version) {
    std::thread([this, rid, bid, service, region, id, version]() {
        struct RequestHelper rh;

        for (const auto& server : servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            rh.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            rh.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            rh.responses.emplace_back(response);

            rendezvous_server::ReplicaRequestContext ctx;
            ctx.set_replicaid(id);
            ctx.set_requestversion(version);

            rendezvous_server::RegisterBranchMessage request;
            request.set_rid(rid);
            request.set_bid(bid);
            request.set_service(service);
            request.set_region(region);
            request.mutable_context()->CopyFrom(ctx);

            rh.rpcs.emplace_back(server->AsyncregisterBranch(context, request, &rh.completionQueue));
            rh.rpcs[rh.nrpcs]->Finish(response, status, (void*)1);

            rh.nrpcs++;
        }

        waitCompletionQueue("Register Branch", rh);

    }).detach();
}

void ReplicaClient::sendRegisterBranches(const std::string& rid, const std::string& bid, const std::string& service, const google::protobuf::RepeatedPtrField<std::string>& regions, const std::string& id, const int& version) {
    std::thread([this, rid, bid, service, regions, id, version]() {
        struct RequestHelper rh;

        for (const auto& server : servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            rh.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            rh.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            rh.responses.emplace_back(response);

            rendezvous_server::ReplicaRequestContext ctx;
            ctx.set_replicaid(id);
            ctx.set_requestversion(version);

            rendezvous_server::RegisterBranchesMessage request;
            request.set_rid(rid);
            request.set_bid(bid);
            request.set_service(service);
            request.mutable_regions()->CopyFrom(regions);
            request.mutable_context()->CopyFrom(ctx);

            rh.rpcs.emplace_back(server->AsyncregisterBranches(context, request, &rh.completionQueue));
            rh.rpcs[rh.nrpcs]->Finish(response, status, (void*)1);

            rh.nrpcs++;
        }

        waitCompletionQueue("Register Branches", rh);

    }).detach();
}

void ReplicaClient::sendCloseBranch(const std::string& rid, const std::string& bid, const std::string& service, const std::string& region) {
    std::thread([this, rid, bid, service, region]() {
        struct RequestHelper rh;

        for (const auto& server : servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            rh.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            rh.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            rh.responses.emplace_back(response);

            rendezvous_server::CloseBranchMessage request;
            request.set_rid(rid);
            request.set_bid(bid);
            request.set_service(service);
            request.set_region(region);

            rh.rpcs.emplace_back(server->AsynccloseBranch(context, request, &rh.completionQueue));
            rh.rpcs[rh.nrpcs]->Finish(response, status, (void*)1);

            rh.nrpcs++;
        }

        waitCompletionQueue("Close Branch", rh);

    }).detach();
}