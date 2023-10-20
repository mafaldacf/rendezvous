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

void ReplicaClient::waitCompletionQueue(const std::string& request, AsyncRequestHelper& req_helper) {
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
    AsyncRequestHelper req_helper;
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
}

void ReplicaClient::registerRequest(const std::string& rid) {
    if (utils::ASYNC_REPLICATION) {
        std::thread([this, rid]() {
            _doRegisterRequest(rid);
        }).detach();
    }
    else {
        _doRegisterRequest(rid);
    }
}

void ReplicaClient::saveAsyncCall(AsyncRequestHelper &req_helper, grpc::ClientContext * context, grpc::Status * status, rendezvous_server::Empty * response) {
    req_helper.contexts.emplace_back(context);
    req_helper.statuses.emplace_back(status);
    req_helper.responses.emplace_back(response);
    req_helper.rpcs[req_helper.nrpcs]->Finish(response, status, (void*)1);
    req_helper.nrpcs++;
}

void ReplicaClient::_doRegisterBranch(const std::string& rid, const std::string& async_zone, const std::string& core_bid,
    const std::string& service, const std::string& tag, 
    const google::protobuf::RepeatedPtrField<std::string>& regions, bool monitor,
    const rendezvous_server::RequestContext& ctx_replica) {

        AsyncRequestHelper req_helper;
        for (const auto& server : _servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            grpc::Status * status = new grpc::Status();
            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            rendezvous_server::RegisterBranchMessage request;

            request.set_rid(rid);
            request.set_async_zone(async_zone);
            request.set_core_bid(core_bid);
            request.set_service(service);
            request.set_tag(tag);
            request.set_monitor(monitor);
            request.mutable_regions()->CopyFrom(regions);

            // async replication requires context propagation
            if (utils::ASYNC_REPLICATION) {
                request.mutable_context()->CopyFrom(ctx_replica);
            }

            req_helper.rpcs.emplace_back(server->AsyncRegisterBranch(context, request, &req_helper.queue));
            saveAsyncCall(req_helper, context, status, response);
        }
        waitCompletionQueue("RB", req_helper);
    }

void ReplicaClient::registerBranch(const std::string& rid, const std::string& async_zone, const std::string& core_bid,
    const std::string& service, const std::string& tag, 
    const google::protobuf::RepeatedPtrField<std::string>& regions, bool monitor,
    const rendezvous_server::RequestContext& ctx_replica) {

        if (utils::ASYNC_REPLICATION) {
        std::thread([this, rid, async_zone, core_bid, service, tag, regions, monitor, ctx_replica]() {
            _doRegisterBranch(rid, async_zone, core_bid, service, tag, regions, monitor, ctx_replica);
        }).detach();
        }
        else {
            _doRegisterBranch(rid, async_zone, core_bid, service, tag, regions, monitor, ctx_replica);
        }
}

void ReplicaClient::_doCloseBranch(const std::string& rid, const std::string& core_bid, const std::string& region, 
    const rendezvous_server::RequestContext& ctx_replica) {

        AsyncRequestHelper req_helper;
        for (const auto& server : _servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            req_helper.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            req_helper.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            req_helper.responses.emplace_back(response);

            rendezvous_server::CloseBranchMessage request;
            request.set_rid(rid);
            request.set_core_bid(core_bid);
            request.set_region(region);

            // async replication requires context propagation
            if (utils::ASYNC_REPLICATION) {
                request.mutable_context()->CopyFrom(ctx_replica);
            }

            req_helper.rpcs.emplace_back(server->AsyncCloseBranch(context, request, &req_helper.queue));
            req_helper.rpcs[req_helper.nrpcs]->Finish(response, status, (void*)1);
            req_helper.nrpcs++;
        }
        waitCompletionQueue("CB", req_helper);
    }

void ReplicaClient::closeBranch(const std::string& rid, const std::string& core_bid, const std::string& region, 
const rendezvous_server::RequestContext& ctx_replica) {

    if (utils::ASYNC_REPLICATION) {
        std::thread([this, rid, core_bid, region, ctx_replica]() {
            _doCloseBranch(rid, core_bid, region, ctx_replica);
        }).detach();
    }
    else {
        _doCloseBranch(rid, core_bid, region, ctx_replica);
    }
}

replicas::ReplicaClient::AsyncRequestHelper * ReplicaClient::addWaitLog(const std::string& rid, 
    const std::string& async_zone, const std::string& target_service) {

    AsyncRequestHelper * req_helper = new AsyncRequestHelper{};

    for (const auto& server : _servers) {
        grpc::ClientContext * context = new grpc::ClientContext();
        req_helper->contexts.emplace_back(context);

        grpc::Status * status = new grpc::Status();
        req_helper->statuses.emplace_back(status);

        rendezvous_server::Empty * response = new rendezvous_server::Empty();
        req_helper->responses.emplace_back(response);

        rendezvous_server::AddWaitLogMessage request;
        request.set_rid(rid);
        request.set_async_zone(async_zone);
        request.set_target_service(target_service);

        req_helper->rpcs.emplace_back(server->AsyncAddWaitLog(context, request, &req_helper->queue));
        req_helper->rpcs[req_helper->nrpcs]->Finish(response, status, (void*)1);
        req_helper->nrpcs++;
    }

    return req_helper;
}

void ReplicaClient::removeWaitLog(const std::string& rid, const std::string& async_zone, 
    const std::string& target_service, 
    replicas::ReplicaClient::AsyncRequestHelper * add_wait_log_async_request_helper) {

    std::thread([this, rid, async_zone, target_service, add_wait_log_async_request_helper]() {
        // wait for previous requests for adding to wait log
        waitCompletionQueue("AWL", *add_wait_log_async_request_helper);

        AsyncRequestHelper req_helper;
        for (const auto& server : _servers) {
            grpc::ClientContext * context = new grpc::ClientContext();
            req_helper.contexts.emplace_back(context);

            grpc::Status * status = new grpc::Status();
            req_helper.statuses.emplace_back(status);

            rendezvous_server::Empty * response = new rendezvous_server::Empty();
            req_helper.responses.emplace_back(response);

            rendezvous_server::RemoveWaitLogMessage request;
            request.set_rid(rid);
            request.set_async_zone(async_zone);
            request.set_target_service(target_service);

            req_helper.rpcs.emplace_back(server->AsyncRemoveWaitLog(context, request, &req_helper.queue));
            req_helper.rpcs[req_helper.nrpcs]->Finish(response, status, (void*)1);
            req_helper.nrpcs++;
        }
        waitCompletionQueue("RWL", req_helper);
    }).detach();
}