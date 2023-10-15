#include "client_service_impl.h"

using namespace service;

ClientServiceImpl::ClientServiceImpl(
  std::shared_ptr<rendezvous::Server> server, 
  std::vector<std::string> addrs)
  : _server(server), _num_wait_calls(0), _replica_client(addrs),
  _num_replicas(addrs.size()+1) /* current replica is not part of the address vector */ {

  _pending_service_branches = std::unordered_map<std::string, PendingServiceBranch*>();
}

metadata::Request * ClientServiceImpl::_getRequest(const std::string& rid) {
  metadata::Request * request;
  // one metadata server
  if (_num_replicas == 1) {
    request = _server->getRequest(rid);
  }
  // replicated servers
  else {
    request = _server->getOrRegisterRequest(rid);
  }
  return request;
}

grpc::Status ClientServiceImpl::Subscribe(grpc::ServerContext * context,
  const rendezvous::SubscribeMessage * request,
  grpc::ServerWriter<rendezvous::SubscribeResponse> * writer) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;
  spdlog::info("> [SUB] loading subscriber for service '{}' and region '{}'", request->service(), request->region());
  metadata::Subscriber * subscriber = _server->getSubscriber(request->service(), request->region());
  rendezvous::SubscribeResponse response;

  while (!context->IsCancelled()) {
    auto subscribedBranch = subscriber->pop(context);
    if (!subscribedBranch.bid.empty()) {
      response.set_bid(subscribedBranch.bid);
      response.set_tag(subscribedBranch.tag);
      //spdlog::debug("< [SUB] sending bid -->  '{}' for tag '{}'", subscribedBranch.bid, subscribedBranch.tag);
      writer->Write(response);
    }
  }

  spdlog::info("< [SUB] context CANCELLED for service '{}' and region '{}'", request->service(), request->region());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::RegisterRequest(grpc::ServerContext* context, 
  const rendezvous::RegisterRequestMessage* request, 
  rendezvous::RegisterRequestResponse* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;
  std::string rid = request->rid();
  metadata::Request * rdv_request;
  spdlog::trace("> [RR] register request '{}'", rid.c_str());
  rdv_request = _server->getOrRegisterRequest(rid);
  response->set_rid(rdv_request->getRid());
  
  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    if (utils::ASYNC_REPLICATION) {
      // initialize empty metadata for client to propagate
      rendezvous::RequestContext ctx;
      response->mutable_context()->CopyFrom(ctx);
    }
    _replica_client.registerRequest(rdv_request->getRid());
  }
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::RegisterBranch(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchMessage* request, 
  rendezvous::RegisterBranchResponse* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;
  rendezvous::RequestContext ctx = request->context();
  std::string rid = request->rid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  const std::string& service_call_bid = request->service_call_bid();
  std::string async_zone = ctx.async_zone();
  bool monitor = request->monitor();
  int num = request->regions().size();
  const auto& regions = request->regions();

  spdlog::trace("> [RB] register #{} branches for request '{}' on service '{}:{}' (monitor={})", num, rid, service, tag, monitor);
  
  if (service.empty()) {
    spdlog::error("< [RB] Error: service empty for rid '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_SERVICE_EMPTY);
  }

  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [RB] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  if (async_zone.empty()) {
    async_zone = utils::ROOT_ASYNC_ZONE_ID;
  }

  const std::string& core_bid = _server->genBid(rdv_request);

  bool r = _server->registerBranch(rdv_request, async_zone, service, regions, tag, ctx.current_service(), core_bid, monitor);

  // could not create branch (tag already exists)
  if (!r) {
    spdlog::error("< [RB] Error: could not register branch for core bid {}", core_bid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_TAG_ALREADY_EXISTS_OR_INVALID_CONTEXT);
  }

  response->set_rid(rid);
  response->set_bid(_server->composeFullId(core_bid, rid));
  ctx.set_current_service(service);
  response->mutable_context()->CopyFrom(ctx);

  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    rendezvous_server::RequestContext ctx_replica;
    if (utils::ASYNC_REPLICATION) {
      const std::string& sid = _server->getSid();
      int new_version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
      ctx_replica.set_sid(sid);
      ctx_replica.set_version(new_version);
    }
    _replica_client.registerBranch(rid, async_zone, core_bid, service, tag, regions, monitor, ctx, ctx_replica);
  }

  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::RegisterBranches(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchesMessage* request, 
  rendezvous::RegisterBranchesResponse* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;
  const auto& contexts = request->contexts();
  std::string rid = request->rid();
  const auto& services = request->services();
  const auto& tags = request->tags();
  const std::string& service_call_bid = request->service_call_bid();
  const std::string& region = request->region();

  if (tags.size() > 0 && services.size() != tags.size()) {
    spdlog::error("< [RBs] Error: number of services and tags do not match for for rid {}", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_NUM_SERVICES_TAGS_DOES_NOT_MATCH);
  }

  if (contexts.size() > 0 && services.size() != contexts.size()) {
    spdlog::error("< [RBs] Error: number of services and contexts do not match for for rid {}", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_NUM_SERVICES_CONTEXTS_DOES_NOT_MATCH);
  }

  spdlog::trace("> [RBs] register #{} service branches for request '{}')", services.size(), rid);

  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [RBs] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  for (int i = 0; i < services.size(); i++) {
    const std::string& service = services[i];
    const std::string& tag = tags.size() > 0 ? tags[i] : "";
    // workaround
    utils::ProtoVec regions;
    regions.Add()->assign(region);

    rendezvous::RequestContext ctx = contexts[i];
    std::string async_zone = ctx.async_zone();
    if (async_zone.empty()) {
      async_zone = utils::ROOT_ASYNC_ZONE_ID;
    }

    const std::string& current_service = ctx.current_service();

    const std::string& core_bid = _server->genBid(rdv_request);

    bool r = true;
    if (ASYNC_SERVICE_REGISTER_CALLS) {
      // add as a pending register to be completed later
      PendingServiceBranch * pending_service;

      std::unique_lock<std::mutex> lock_map(_mutex_pending_service_branches);
      auto it = _pending_service_branches.find(service_call_bid);
      if (it == _pending_service_branches.end()) {
        pending_service = new PendingServiceBranch {};
        _pending_service_branches[service_call_bid] = pending_service;
        _cond_pending_service_branch.notify_all();
      }
      else {
        pending_service = it->second;
      }
      lock_map.release();

      std::unique_lock<std::mutex> lock(pending_service->mutex);
      pending_service->num++;
      lock.unlock();

      std::thread([this, rdv_request, async_zone, service, regions, tag, current_service, core_bid, pending_service]() {
        _server->registerBranch(rdv_request, async_zone, service, regions, tag, current_service, core_bid, false);
        std::unique_lock<std::mutex> lock(pending_service->mutex);
        pending_service->num--;
        pending_service->condv.notify_all();
        lock.unlock();
      }).detach();
    }
    else {
      r = _server->registerBranch(rdv_request, async_zone, service, regions, tag, current_service, core_bid, false);
    }

    // could not create branch (tag already exists)
    if (!r) {
      spdlog::error("< [RBs] Error: could not register branch for core bid {}", core_bid);
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_TAG_ALREADY_EXISTS_OR_INVALID_CONTEXT);
    }

    response->set_rid(rid);
    response->add_bids(_server->composeFullId(core_bid, rid));

    // replicate client request to remaining replicas
    if (_num_replicas > 1) {
      rendezvous_server::RequestContext ctx_replica;
      if (utils::ASYNC_REPLICATION) {
        const std::string& sid = _server->getSid();
        int new_version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
        ctx_replica.set_sid(sid);
        ctx_replica.set_version(new_version);
      }
      _replica_client.registerBranch(rid, async_zone, core_bid, service, tag, regions, false, ctx, ctx_replica);
    }

    ctx.set_current_service(service);
    rendezvous::RequestContext * new_ctx_ptr = response->add_contexts();
    new_ctx_ptr->CopyFrom(ctx);
  }

  return grpc::Status::OK;

  /* for (int i = 0; i < services.size(); i++) {
    rendezvous::RegisterBranchMessage request_v2;
    request_v2.set_rid(rid);
    request_v2.set_service(services[i]);
    request_v2.set_tag(tags[i]);
    request_v2.add_regions(region);
    request_v2.mutable_context()->CopyFrom(contexts[i]);
    rendezvous::RegisterBranchResponse response_v2;
    grpc::Status status = registerBranch(&request_v2, &response_v2);
  } */

  return grpc::Status::OK;
}


grpc::Status ClientServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous::CloseBranchMessage* request, 
  rendezvous::Empty* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& region = request->region();
  const std::string& composed_bid = request->bid();
  bool close_service = request->close_service();

  spdlog::trace("> [CB] closing branch with full bid '{}' on region '{}'", composed_bid, region);

  // parse composed id into <bid, root_rid>
  auto ids = _server->parseFullId(composed_bid);
  const std::string& bid = ids.first;
  const std::string& root_rid = ids.second;
  if (bid.empty() || root_rid.empty()) {
    spdlog::error("< [CB] Error parsing full bid '{}'", root_rid);
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_BID);
  }

  metadata::Request * rdv_request = _getRequest(root_rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [CB] Error: invalid request '{}'", root_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  int res = _server->closeBranch(rdv_request, bid, region);
  if (res == 0) {
    spdlog::error("< [CB] Error: branch not found for provided bid (composed_bid): {}", composed_bid);
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  } else if (res == -1) {
    spdlog::error("< [CB] Error: region '{}' for branch with bid (composed_bid '{}' not found", region, composed_bid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }

  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    rendezvous::RequestContext ctx = request->context();
    rendezvous_server::RequestContext ctx_replica;
    if (utils::ASYNC_REPLICATION) {
      const std::string& sid = _server->getSid();
      int version = rdv_request->getVersionsRegistry()->getLocalVersion(sid);
      ctx_replica.set_sid(sid);
      ctx_replica.set_version(version);
    }
    _replica_client.closeBranch(root_rid, bid, region, ctx, ctx_replica);
  }
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::WaitRequest(grpc::ServerContext* context, 
  const rendezvous::WaitRequestMessage* request, 
  rendezvous::WaitRequestResponse* response) {
  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  rendezvous::RequestContext ctx = request->context();
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const auto& services = request->services();
  const std::string& tag = request->tag();
  const std::string& region = request->region();
  bool wait_deps = request->wait_deps();
  std::string async_zone = ctx.async_zone();
  const std::string& current_service = ctx.current_service();
  //bool async = request->async();
  int timeout = request->timeout();

  spdlog::trace("> [WR] wait call for request '{}' on service '{}' and region '{}'", rid, service, region);

  // validate parameters
  if (timeout < 0) {
    spdlog::error("< [WR] Error: invalid timeout ({})", timeout);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TIMEOUT);
  } if (!service.empty() && services.size() > 0) {
    spdlog::error("< [WR] Error: cannot provide 'service' and 'services' parameters simultaneously", tag);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_SERVICES_EXCLUSIVE);
  } if (!tag.empty() && service.empty()) {
    spdlog::error("< [WR] Error: service not specified for tag '{}'", tag);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TAG_USAGE);
  }

  // check if request exists
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [WR] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  if (async_zone.empty()) {
    async_zone = utils::ROOT_ASYNC_ZONE_ID;
  }

  int result;

  replicas::ReplicaClient::AsyncRequestHelper * async_request_helper = _replica_client.addWaitLog(rid, async_zone, service);

  // perform wait on a single service
  if (services.size() == 0) {
    int result = _server->wait(rdv_request, async_zone, service, region, tag, utils::ASYNC_REPLICATION, timeout, current_service, wait_deps);
    if (result == 1) {
      response->set_prevented_inconsistency(true);
    }
  }
  // otherwise, perform wait on multiple provided services
  else {
    for (const auto& service: services) {
      result = _server->wait(rdv_request, async_zone, service, region, tag, utils::ASYNC_REPLICATION, timeout, current_service, wait_deps);
      if (result < 0) {
        break;
      }
    }
  }

  _replica_client.removeWaitLog(rid, async_zone, service, async_request_helper);

  // parse errors
  if (result == -1) {
    response->set_timed_out(true);
  } else if (result == -2) {
    spdlog::error("< [WR] Error: invalid context (service/region)", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_SERVICE_REGION);
  } else if (result == -3) {
    spdlog::error("< [WR] Error: current service branch needs to be registered before any wait call", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_WAIT_CALL_NO_CURRENT_SERVICE);
  } else if (result == -4) {
    spdlog::error("< [WR] Error: invalid tag", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TAG);
  }
  spdlog::trace("< [WR] returning call for request '{}' on service '{}' and region '{}' (r={})", rid, service, region, result);
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CheckStatus(grpc::ServerContext* context, 
  const rendezvous::CheckStatusMessage* request, 
  rendezvous::CheckStatusResponse* response) {
  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  const std::string& async_zone_id = request->context().async_zone();
  bool detailed = request->detailed();

  spdlog::trace("> [CS] query for request '{}' on service '{}' and region '{}' (async_zone={}, detailed={})", rid, service, region, async_zone_id, detailed);

  // check if request exists
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [CS] Error: invalid request for composed rid '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  const auto& result = _server->checkStatus(rdv_request, async_zone_id, service, region, detailed);

  // detailed information with status of all tagged branches
  if (detailed) {
    // get info for tagged branches
    auto * tagged = response->mutable_tagged();
    response->set_status(static_cast<rendezvous::RequestStatus>(result.status));
    for (auto pair = result.tagged.begin(); pair != result.tagged.end(); pair++) {
      // <service, status>
      (*tagged)[pair->first] = static_cast<rendezvous::RequestStatus>(pair->second);
    }

    // get info for regions
    auto * regions = response->mutable_regions();
    response->set_status(static_cast<rendezvous::RequestStatus>(result.status));
    for (auto pair = result.regions.begin(); pair != result.regions.end(); pair++) {
      // <region, status>
      (*regions)[pair->first] = static_cast<rendezvous::RequestStatus>(pair->second);
    }
  }

  response->set_status(static_cast<rendezvous::RequestStatus>(result.status));
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::FetchDependencies(grpc::ServerContext* context, 
  const rendezvous::FetchDependenciesMessage* request, 
  rendezvous::FetchDependenciesResponse* response) {
  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& async_zone_id = request->context().async_zone();

  spdlog::trace("> [FD] query for request '{}' on service '{}'", rid, service);
  
  // check if request exists
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [FD] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  const auto& result = _server->fetchDependencies(rdv_request, service, async_zone_id);

  if (result.res == INVALID_SERVICE) {
    spdlog::error("< [FD] Error: invalid service", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_SERVICE);
  }

  for (const auto& dep: result.deps) {
    response->add_deps(dep);
  }

  // only used when service is specified
  for (const auto& indirect_dep: result.indirect_deps) {
    response->add_indirect_deps(indirect_dep);
  }
  

  return grpc::Status::OK;
}