#include "replica_client.h"

using namespace replicas;

ReplicaClient::ReplicaClient(std::vector<std::string> addrs, bool async_replication): 
    _async_replication(async_replication) {

    // by default, addrs does not contain the address of the current replica
    for (const auto& addr : addrs) {
      auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
      auto stub = rendezvous_server::ServerService::NewStub(channel);
      _servers.push_back(std::move(stub));
    }
}

void ReplicaClient::_waitCompletionQueue(const std::string& request, struct RequestHelper& req_helper) {
    for(int i = 0; i < req_helper.nrpcs; i++) {
        void * tagPtr;
        bool ok = false;

        req_helper.queue.Next(&tagPtr, &ok);
        const size_t tag = size_t(tagPtr);
        const grpc::Status & status = *(req_helper.statuses[tag-1].get());

        if (!status.ok()) {
            spdlog::critical("[REPLICA CLIENT - {}] RPC #{} ERROR: {}", request.c_str(), tag, status.error_message().c_str());
        }
    }
}

void ReplicaClient::_doRegisterRequest(const std::string& rid) {
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
    _waitCompletionQueue("RR", req_helper);
}

void ReplicaClient::registerRequest(const std::string& rid) {
    if (_async_replication) {
        std::thread([this, rid]() {
            _doRegisterRequest(rid);
        }).detach();
    }
    else {
        _doRegisterRequest(rid);
    }
}

void ReplicaClient::_doRegisterBranch(const std::string& root_rid, const std::string& sub_rid, const std::string& core_bid,
    const std::string& service, const std::string& tag, 
    const google::protobuf::RepeatedPtrField<std::string>& regions, bool monitor, bool async,
    const rendezvous::RequestContext& ctx) {

        struct RequestHelper req_helper;
        for (const auto& server : _servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            req_helper.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            req_helper.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            req_helper.responses.emplace_back(response);

            rendezvous_server::RegisterBranchMessage request;
            request.set_root_rid(root_rid);
            request.set_sub_rid(sub_rid);
            request.set_core_bid(core_bid);
            request.set_service(service);
            request.set_tag(tag);
            request.set_monitor(monitor);
            request.set_async(async);
            request.mutable_regions()->CopyFrom(regions);

            // async replication requires context propagation
            if (_async_replication) {
                rendezvous_server::RequestContext repl_context;
                for (const auto& v : ctx.versions()) {
                    repl_context.mutable_versions()->insert({v.first, v.second});
                }
                request.mutable_context()->CopyFrom(repl_context);
            }

            req_helper.rpcs.emplace_back(server->AsyncRegisterBranch(context, request, &req_helper.queue));
            req_helper.rpcs[req_helper.nrpcs]->Finish(response, status, (void*)1);

            req_helper.nrpcs++;
        }
        _waitCompletionQueue("RB", req_helper);
    }

void ReplicaClient::registerBranch(const std::string& root_rid, const std::string& sub_rid, const std::string& core_bid,
    const std::string& service, const std::string& tag, 
    const google::protobuf::RepeatedPtrField<std::string>& regions, bool monitor, bool async,
    const rendezvous::RequestContext& ctx) {

        if (_async_replication) {
        std::thread([this, root_rid, sub_rid, core_bid, service, tag, regions, monitor, async, ctx]() {
            _doRegisterBranch(root_rid, sub_rid, core_bid, service, tag, regions, monitor, async, ctx);
        }).detach();
        }
        else {
            _doRegisterBranch(root_rid, sub_rid, core_bid, service, tag, regions, monitor, async, ctx);
        }
}

void ReplicaClient::_doCloseBranch(const std::string& root_rid, const std::string& core_bid, 
    const std::string& region, const rendezvous::RequestContext& ctx) {

        struct RequestHelper req_helper;
        for (const auto& server : _servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            req_helper.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            req_helper.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            req_helper.responses.emplace_back(response);

            rendezvous_server::CloseBranchMessage request;
            request.set_root_rid(root_rid);
            request.set_core_bid(core_bid);
            request.set_region(region);

            // async replication requires context propagation
            if (_async_replication) {
                rendezvous_server::RequestContext repl_context;
                for (const auto& v : ctx.versions()) {
                    repl_context.mutable_versions()->insert({v.first, v.second});
                }
                request.mutable_context()->CopyFrom(repl_context);
            }

            req_helper.rpcs.emplace_back(server->AsyncCloseBranch(context, request, &req_helper.queue));
            req_helper.rpcs[req_helper.nrpcs]->Finish(response, status, (void*)1);

            req_helper.nrpcs++;
        }
        _waitCompletionQueue("CB", req_helper);
    }

void ReplicaClient::closeBranch(const std::string& root_rid, const std::string& core_bid, 
    const std::string& region, const rendezvous::RequestContext& ctx) {

    if (_async_replication) {
        std::thread([this, root_rid, core_bid, region, ctx]() {
            _doCloseBranch(root_rid, core_bid, region, ctx);
        }).detach();
    }
    else {
        _doCloseBranch(root_rid, core_bid, region, ctx);
    }

}