#include "client_service_impl.h"

using namespace service;

ClientServiceImpl::ClientServiceImpl(
  std::shared_ptr<rendezvous::Server> server, 
  std::vector<std::string> addrs)
  : _server(server), _num_wait_calls(0), _replica_client(addrs),
  _num_replicas(addrs.size()+1) /* current replica is not part of the address vector */ {

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
  std::string composed_rid = request->rid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  bool monitor = request->monitor();
  bool async = request->async();
  rendezvous::RequestContext ctx = request->context();
  int num = request->regions().size();
  const auto& regions = request->regions();

  // FIXME: VERIFY REGINS ONE BY ONE!!!

  spdlog::trace("> [RB] register #{} branches for request '{}' on service '{}:{}' (async={}, monitor={})", num, composed_rid, service, tag, async, monitor);
  
  // service is empty
  if (service.empty()) {
    spdlog::error("< [RB] Error: service empty for rid '{}'", composed_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_SERVICE_EMPTY);
  }

  // parse full rid into <rid, sub_rid>
  auto ids = _server->parseFullId(composed_rid);
  const std::string& root_rid = ids.first;
  std::string sub_rid = ids.second;
  // rid cannot be empty (but sub_rid can when we are in the root)
  if (root_rid.empty()) {
    spdlog::error("< [RB] Error parsing composed rid '{}'", composed_rid);
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_RID);
  }

  metadata::Request * rdv_request = _getRequest(root_rid);

  // request id is not valid
  if (rdv_request == nullptr) {
    spdlog::error("< [RB] Error: invalid request for composed rid '{}'", composed_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  // if branch is async, we register a new "sub" request id
  if (async) {
    sub_rid = _server->addNextSubRequest(rdv_request, sub_rid);
    if (sub_rid.empty()) {
      spdlog::error("< [RB] Error: invalid request '{}' (sub_rid does not exist)", composed_rid);
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST_SUB_RID);
    }
  }

  const std::string& core_bid = _server->registerBranch(rdv_request, sub_rid, service, regions, tag, ctx.prev_service(), monitor);

  // could not create branch (tag already exists)
  if (core_bid.empty()) {
    spdlog::error("< [RB] Error: tag '{}' already exists for service '{}' OR invalid context (field: prev_service)", 
      tag, service, composed_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_TAG_ALREADY_EXISTS_OR_INVALID_CONTEXT);
  }

  composed_rid = _server->composeFullId(root_rid, sub_rid);
  response->set_rid(composed_rid);
  response->set_bid(_server->composeFullId(core_bid, composed_rid));
  ctx.set_prev_service(service);

  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    if (utils::ASYNC_REPLICATION) {
      // update current context
      std::string sid = _server->getSid();
      int version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
      ctx.mutable_versions()->insert({sid, version});
    }
    response->mutable_context()->CopyFrom(ctx);
    _replica_client.registerBranch(root_rid, sub_rid, core_bid, service, tag, regions, monitor, async, ctx);
  }
  return grpc::Status::OK;
}


grpc::Status ClientServiceImpl::RegisterBranchesDatastores(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchesDatastoresMessage* request, 
  rendezvous::RegisterBranchesDatastoresResponse* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;
  const std::string& rid = request->rid();
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  // client specifies different regions for each datastores using the DatastoreBranching type
  if (request->branches().size() > 0){
    for (const auto& branches : request->branches()) {
      std::string bid = _server->registerBranch(rdv_request, "", branches.datastore(), branches.regions(), branches.tag(), "prev_service");
      if (bid.empty()) {
        return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, utils::ERR_MSG_BRANCH_ALREADY_EXISTS);
      }
      response->add_bids(bid);
    }
  }

  // client uses same set of regions for all datastores and each datastore has a specific tag
  else if (request->datastores().size() > 0 && request->regions().size() > 0) {
    const auto& regions = request->regions();
    auto tags = request->tags();
    int i = 0;
    for (const auto& datastore: request->datastores()) {
      std::string bid = _server->registerBranch(rdv_request, "", datastore, regions, tags[i], "prev_service");
      if (bid.empty()) {
        return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, utils::ERR_MSG_BRANCH_ALREADY_EXISTS);
      }
      response->add_bids(bid);
      i++;
    }
  }

  else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_REGISTER_BRANCHES_INVALID_DATASTORES);
  }

  response->set_rid(rdv_request->getRid());

  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    // FIXME: adapt to register branches for datastores (SERVERLESS VERSION)
    /* rendezvous::RequestContext ctx = request->context();
    int version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
    ctx.mutable_versions()->insert({sid, version});
    response->mutable_context()->CopyFrom(ctx);
    _replica_client.registerBranchesDatastores(rdv_request->getRid(), bid, service, region, sid, version); */
  }

  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous::CloseBranchMessage* request, 
  rendezvous::Empty* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& region = request->region();
  bool force = request->force();
  const std::string& composed_bid = request->bid();

  spdlog::trace("> [CB] closing branch with full bid '{}' on region '{}' (force={})", composed_bid, region, force);

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

  int res = _server->closeBranch(rdv_request, bid, region, force);
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
    _replica_client.closeBranch(root_rid, bid, region, ctx);
  }
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::WaitRequest(grpc::ServerContext* context, 
  const rendezvous::WaitRequestMessage* request, 
  rendezvous::WaitRequestResponse* response) {
  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& full_rid = request->rid();
  const std::string& service = request->service();
  const auto& services = request->services();
  const std::string& tag = request->tag();
  const std::string& region = request->region();
  bool wait_deps = request->wait_deps();
  //bool async = request->async();
  int timeout = request->timeout();

  spdlog::trace("> [WR] wait call for request '{}' on service '{}' and region '{}'", full_rid, service, region);

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

  // parse full rid into <rid, sub_rid>
  auto ids = _server->parseFullId(full_rid);
  std::string rid = ids.first;
  std::string sub_rid = ids.second;

  // rid cannot be empty (but sub_rid can when we are in the root)
  if (rid.empty()) {
    spdlog::error("< [RB] Error parsing full rid '{}'", full_rid);
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_RID);
  }

  // check if request exists
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [WR] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  // wait until current replica is consistent with this request
  if (utils::ASYNC_REPLICATION && _num_replicas > 1) {
    rdv_request->getVersionsRegistry()->waitRemoteVersions(request->context());
  }

  int result;

  // wait logic for multiple services
  if (services.size() > 0) {
    for (const auto& service: services) {
      result = _server->wait(rdv_request, sub_rid, service, region, tag, request->context().prev_service(), utils::ASYNC_REPLICATION, timeout, wait_deps);
      if (result < 0) {
        break;
      }
    }
  }
  else {
    int result = _server->wait(rdv_request, sub_rid, service, region, tag, request->context().prev_service(), utils::ASYNC_REPLICATION, timeout, wait_deps);
    if (result == 1) {
      response->set_prevented_inconsistency(true);
    }
  }
  // parse errors
  if (result == -1) {
    response->set_timed_out(true);
  }
  else if (result == -2) {
    spdlog::error("< [WR] Error: invalid context (service/region)", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_SERVICE_REGION);
  } else if (result == -3) {
    spdlog::error("< [WR] Error: invalid context (field: prev_service)", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_CONTEXT);
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

  const std::string& composed_rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  bool detailed = request->detailed();

  spdlog::trace("> [CS] query for request '{}' on service '{}' and region '{}' (detailed={})", composed_rid, service, region, detailed);

  // parse full rid into <rid, sub_rid>
  auto ids = _server->parseFullId(composed_rid);
  const std::string& root_rid = ids.first;
  std::string sub_rid = ids.second;


  // check if request exists
  metadata::Request * rdv_request = _getRequest(root_rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [CS] Error: invalid request for composed rid '{}'", composed_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  const auto& result = _server->checkStatus(rdv_request, sub_rid, service, region, request->context().prev_service(), detailed);

  if (result.status == INVALID_CONTEXT) {
    spdlog::error("< [CS] Error: invalid context (prev_service or rid) for composed rid {}", composed_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_CONTEXT);
  }

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

  spdlog::trace("> [FD] query for request '{}' on service '{}'", rid, service);
  
  // check if request exists
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [FD] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  const auto& result = _server->fetchDependencies(rdv_request, service, request->context().prev_service());

  if (result.res == INVALID_CONTEXT) {
    spdlog::error("< [FD] Error: invalid context (field: prev_service)", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_CONTEXT);
  }
  else if (result.res == INVALID_SERVICE) {
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