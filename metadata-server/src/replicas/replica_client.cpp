#include "replica_client.h"

using namespace replicas;

ReplicaClient::ReplicaClient(std::vector<std::string> addrs) {
    // by default, addrs does not contain the address of the current replica
    for (const auto& addr : addrs) {
      auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
      auto stub = rendezvous_server::ServerService::NewStub(channel);
      _servers.push_back(std::move(stub));
    }
}

void ReplicaClient::waitCompletionQueue(const std::string& request, struct RequestHelper& req_helper) {
    for(int i = 0; i < req_helper.nrpcs; i++) {
        void * tagPtr;
        bool ok = false;

        req_helper.queue.Next(&tagPtr, &ok);
        const size_t tag = size_t(tagPtr);
        const grpc::Status & status = *(req_helper.statuses[tag-1].get());

        if (!status.ok()) {
            spdlog::error("[REPLICA CLIENT - {}] RPC #{} ERROR: {}", request.c_str(), tag, status.error_message().c_str());
        }
    }
}

void ReplicaClient::sendRegisterRequest(const std::string& rid) {
    std::thread([this, rid]() {
        struct RequestHelper req_helper;

        for (const auto& server : _servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            req_helper.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            req_helper.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            req_helper.responses.emplace_back(response);

            rendezvous_server::RegisterRequestMessage request;
            request.set_rid(rid);
            
            req_helper.rpcs.emplace_back(server->AsyncRegisterRequest(context, request, &req_helper.queue));
            req_helper.rpcs[req_helper.nrpcs]->Finish(response, status, (void*)1);

            req_helper.nrpcs++;
        }

        waitCompletionQueue("RR", req_helper);

     }).detach();
}

void ReplicaClient::sendRegisterBranch(const std::string& rid, const std::string& bid, const std::string& service, 
    const std::string& tag, const google::protobuf::RepeatedPtrField<std::string>& regions, bool monitor,
    std::string id, int version) {

    std::thread([this, rid, bid, service, tag, regions, monitor, id, version]() {
        struct RequestHelper req_helper;

        for (const auto& server : _servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            req_helper.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            req_helper.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            req_helper.responses.emplace_back(response);

            rendezvous_server::RegisterBranchMessage request;
            request.set_rid(rid);
            request.set_bid(bid);
            request.set_service(service);
            request.set_tag(tag);
            request.set_monitor(monitor);
            request.mutable_regions()->CopyFrom(regions);

            if (CONTEXT_PROPAGATION) {
                rendezvous_server::ReplicaRequestContext ctx;
                ctx.set_replica_id(id);
                ctx.set_request_version(version);
                request.mutable_context()->CopyFrom(ctx);
            }

            req_helper.rpcs.emplace_back(server->AsyncRegisterBranch(context, request, &req_helper.queue));
            req_helper.rpcs[req_helper.nrpcs]->Finish(response, status, (void*)1);

            req_helper.nrpcs++;
        }

        waitCompletionQueue("RB", req_helper);

    }).detach();
}

void ReplicaClient::sendCloseBranch(const std::string& bid, const std::string& region, 
    std::string id, int version) {

    std::thread([this, bid, region, id, version]() {
        struct RequestHelper req_helper;

        for (const auto& server : _servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            req_helper.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            req_helper.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            req_helper.responses.emplace_back(response);

            rendezvous_server::CloseBranchMessage request;
            request.set_bid(bid);
            request.set_region(region);

            if (CONTEXT_PROPAGATION) {
                rendezvous_server::ReplicaRequestContext ctx;
                ctx.set_replica_id(id);
                ctx.set_request_version(version);
                request.mutable_context()->CopyFrom(ctx);
            }

            req_helper.rpcs.emplace_back(server->AsyncCloseBranch(context, request, &req_helper.queue));
            req_helper.rpcs[req_helper.nrpcs]->Finish(response, status, (void*)1);

            req_helper.nrpcs++;
        }

        waitCompletionQueue("CB", req_helper);

    }).detach();
}