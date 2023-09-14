#include "client_service_impl.h"

using namespace service;

ClientServiceImpl::ClientServiceImpl(
  std::shared_ptr<rendezvous::Server> server, 
  std::vector<std::string> addrs,
  bool async_replication)
  : _server(server), _num_wait_calls(0), _replica_client(addrs, async_replication),
  _async_replication(async_replication),
  _num_replicas(addrs.size()+1) /* current replica is not part of the address vector */ {

    // get consistency checks bool from environment
    auto consistency_checks_env = std::getenv("CONSISTENCY_CHECKS");
    if (consistency_checks_env) {
      _consistency_checks = (atoi(consistency_checks_env) == 1);
    } else { // true by default
      _consistency_checks = true;
    }
    spdlog::info("CONSYSTENCY CHECKS: '{}'", _consistency_checks);
    spdlog::info("ASYNC REPLICATION: '{}'", async_replication);
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

  if (!_consistency_checks) return grpc::Status::OK;
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

  if (!_consistency_checks) return grpc::Status::OK;
  std::string rid = request->rid();
  metadata::Request * rdv_request;
  spdlog::trace("> [RR] register request '{}'", rid.c_str());
  rdv_request = _server->getOrRegisterRequest(rid);
  response->set_rid(rdv_request->getRid());
  
  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    if (_async_replication) {
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

  if (!_consistency_checks) return grpc::Status::OK;
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  bool monitor = request->monitor();
  rendezvous::RequestContext ctx = request->context();
  metadata::Request * rdv_request;
  int num = request->regions().size();

  spdlog::trace("> [RB] register #{} branches for request '{}' on service '{}:{}' (monitor={})", num, rid, service, tag, monitor);

  const auto& regions = request->regions();
  rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [RB] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  std::string bid = _server->registerBranch(rdv_request, service, regions, tag, ctx.parent_service(), monitor);
  if (bid.empty()) {
    spdlog::error("< [RB] Error: tag '{}' already exists for service '{}' in request '{}'", tag, service, rid);
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, utils::ERR_MSG_TAG_ALREADY_EXISTS);
  }
  response->set_rid(rdv_request->getRid());
  response->set_bid(bid);

  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    rendezvous::RequestContext ctx = request->context();
    if (_async_replication) {
      // update current context
      std::string sid = _server->getSid();
      int version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
      ctx.mutable_versions()->insert({sid, version});
      response->mutable_context()->CopyFrom(ctx);
    }
    _replica_client.registerBranch(rdv_request->getRid(), bid, service, tag, regions, monitor, ctx);
  }
  return grpc::Status::OK;
}


grpc::Status ClientServiceImpl::RegisterBranchesDatastores(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchesDatastoresMessage* request, 
  rendezvous::RegisterBranchesDatastoresResponse* response) {

  if (!_consistency_checks) return grpc::Status::OK;
  const std::string& rid = request->rid();
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  // client specifies different regions for each datastores using the DatastoreBranching type
  if (request->branches().size() > 0){
    for (const auto& branches : request->branches()) {
      std::string bid = _server->registerBranch(rdv_request, branches.datastore(), branches.regions(), branches.tag(), "parent_service");
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
      std::string bid = _server->registerBranch(rdv_request, datastore, regions, tags[i], "parent_service");
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

  if (!_consistency_checks) return grpc::Status::OK;
  const std::string& region = request->region();
  bool force = request->force();
  spdlog::trace("> [CB] closing branch with full bid '{}' on region '{}' (force={})", request->bid(), region, force);

  // parse identifiers from <bid>:<rid>
  auto ids = _server->parseFullBid(request->bid());
  std::string bid = ids.first;
  std::string rid = ids.second;
  if (rid.empty() || bid.empty()) {
    spdlog::error("< [CB] Error parsing full bid '{}'", request->bid());
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_BID);
  }

  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [CB] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  int res = _server->closeBranch(rdv_request, bid, region, force);
  if (res == 0) {
    spdlog::error("< [CB] Error: branch with full bid '{}' not found", request->bid());
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  }
  else if (res == -1) {
    spdlog::error("< [CB] Error: region '{}' for branch with full bid '{}' not found", region, request->bid());
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }

  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    rendezvous::RequestContext ctx = request->context();
    _replica_client.closeBranch(request->bid(), region, ctx);
  }
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::WaitRequest(grpc::ServerContext* context, 
  const rendezvous::WaitRequestMessage* request, 
  rendezvous::WaitRequestResponse* response) {
  if (!_consistency_checks) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  const std::string& region = request->region();
  //bool async = request->async();
  int timeout = request->timeout();

  spdlog::trace("> [WR] wait call for request '{}' on service '{}' and region '{}'", rid, service, region);

  // validate parameters
  if (timeout < 0) {
    spdlog::error("< [WR] Error: invalid timeout ({})", timeout);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TIMEOUT);
  }
  if (!tag.empty() && service.empty()) {
    spdlog::error("< [WR] Error: service not specified for tag '{}'", tag);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TAG_USAGE);
  }

  // check if request exists
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [WR] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  // wait until current replica is consistent with this request
  if (_async_replication && _num_replicas > 1) {
    rdv_request->getVersionsRegistry()->waitRemoteVersions(request->context());
  }
  
  // provide in-depth info about effectiveness of wait request (prevented inconsistency, timedout, etc)
  int result = _server->waitRequest(rdv_request, service, region, tag, true, timeout);
  if (result == 1) {
    response->set_prevented_inconsistency(true);
  }
  else if (result == -1) {
    response->set_timed_out(true);
  }
  spdlog::trace("< [WR] returning call for request '{}' on service '{}' and region '{}' (r={})", rid, service, region, result);
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CheckRequest(grpc::ServerContext* context, 
  const rendezvous::CheckRequestMessage* request, 
  rendezvous::CheckRequestResponse* response) {
  if (!_consistency_checks) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  bool detailed = request->detailed();

  spdlog::trace("> [CR] query for request '{}' on service '{}' and region '{}' (detailed=)", rid, service, region, detailed);
  
  // check if request exists
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [CR] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  // detailed information with status of all tagged branches
  if (detailed) {
    if (service.empty()) {
      spdlog::error("< [CR] Error: service not specified for detailed query");
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, utils::ERR_MSG_FAILED_DETAILED_QUERY);
    }
    auto result = _server->checkDetailedRequest(rdv_request, service, region);

    // get info for tagged branches
    auto * tagged = response->mutable_tagged();
    response->set_status(static_cast<rendezvous::RequestStatus>(result.status));
    for (auto pair = result.tagged.begin(); pair != result.tagged.end(); pair++) {
      // <service, status>
      (*tagged)[pair->first] = static_cast<rendezvous::RequestStatus>(pair->second);
    }

    // get info for children/dependencies branches
    auto * children = response->mutable_direct_dependencies();
    response->set_status(static_cast<rendezvous::RequestStatus>(result.status));
    for (auto pair = result.children.begin(); pair != result.children.end(); pair++) {
      // <child service name, status>
      (*children)[pair->first] = static_cast<rendezvous::RequestStatus>(pair->second);
    }

    // get info for regions
    auto * regions = response->mutable_regions();
    response->set_status(static_cast<rendezvous::RequestStatus>(result.status));
    for (auto pair = result.regions.begin(); pair != result.regions.end(); pair++) {
      // <region, status>
      (*regions)[pair->first] = static_cast<rendezvous::RequestStatus>(pair->second);
    }
  }
  // basic information for current context
  else {
    int result = _server->checkRequest(rdv_request, service, region);
    response->set_status(static_cast<rendezvous::RequestStatus>(result));
  }
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CheckRequestByRegions(grpc::ServerContext* context, 
  const rendezvous::CheckRequestByRegionsMessage* request, 
  rendezvous::CheckRequestByRegionsResponse* response) {
  if (!_consistency_checks) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();

  spdlog::trace("> [CRR] query for request '{}' on service '{}'", rid, service);
  
  // check if request exists
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::error("< [CRR] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  // output info
  std::map<std::string, int> result = _server->checkRequestByRegions(rdv_request, service);
  auto * status = response->mutable_status();
  for (auto pair = result.begin(); pair != result.end(); pair++) {
    // <region, status>
    (*status)[pair->first] = static_cast<rendezvous::RequestStatus>(pair->second);
  }
  return grpc::Status::OK;
}