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

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  //spdlog::debug("[SUBSCRIBER] loading subscriber for service '{}' and region '{}'", request->service().c_str(), request->region().c_str());
  metadata::Subscriber * subscriber = _server->getSubscriber(request->service(), request->region());
  rendezvous::SubscribeResponse response;

  while (!context->IsCancelled()) {
    auto subscribedBranch = subscriber->pop(context);
    if (!subscribedBranch.bid.empty()) {
      response.set_bid(subscribedBranch.bid);
      response.set_tag(subscribedBranch.tag);
      //spdlog::debug("[SUBSCRIBER] sending bid -->  {} for tag {}", subscribedBranch.bid.c_str(), subscribedBranch.tag.c_str());
      writer->Write(response);
    }
  }

  //spdlog::debug("[SUBSCRIBER] context CANCELLED for service '{}' and region '{}'", request->service().c_str(), request->region().c_str());

  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::RegisterRequest(grpc::ServerContext* context, 
  const rendezvous::RegisterRequestMessage* request, 
  rendezvous::RegisterRequestResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  std::string rid = request->rid();
  metadata::Request * rdv_request;
  //spdlog::trace("> registering request '{}'", rid.c_str());
  rdv_request = _server->getOrRegisterRequest(rid);
  response->set_rid(rdv_request->getRid());

  if (_num_replicas > 1) {
    if (CONTEXT_PROPAGATION) {
      // initialize empty metadata
      rendezvous::RequestContext ctx;
      response->mutable_context()->CopyFrom(ctx);
    }
    _replica_client.sendRegisterRequest(rdv_request->getRid());
  }

  //spdlog::trace("< registered request '{}'", rdv_request->getRid().c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::RegisterBranch(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchMessage* request, 
  rendezvous::RegisterBranchResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  rendezvous::RequestContext ctx = request->context();
  metadata::Request * rdv_request;
  int num = request->regions().size();

  //spdlog::trace("> registering {} branches for request '{}' on service '{}'", num, rid, service);
  if (num == 0) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_EMPTY_REGION);
  }

  const auto& regions = request->regions();
  rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  std::string bid = _server->registerBranch(rdv_request, service, regions, tag);
  response->set_rid(rdv_request->getRid());
  response->set_bid(bid);

  if (_num_replicas > 1) {
    if (CONTEXT_PROPAGATION) {
      rendezvous::RequestContext ctx = request->context();
      std::string sid = _server->getSid();
      int version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
      ctx.mutable_versions()->insert({sid, version});
      response->mutable_context()->CopyFrom(ctx);
      _replica_client.sendRegisterBranch(rdv_request->getRid(), bid, service, regions, sid, version);
    }
    else {
      _replica_client.sendRegisterBranch(rdv_request->getRid(), bid, service, regions);
    }
  }
  
  //spdlog::trace("< registered {} branches '{}' for request '{}' on service '{}'", num, bid, rid, service);
  return grpc::Status::OK;
}


grpc::Status ClientServiceImpl::RegisterBranchesDatastores(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchesDatastoresMessage* request, 
  rendezvous::RegisterBranchesDatastoresResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  const std::string& rid = request->rid();
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  // client specifies different regions for each datastores using the DatastoreBranching type
  if (request->branches().size() > 0){
    for (const auto& branches : request->branches()) {
      std::string bid = _server->registerBranch(rdv_request, branches.datastore(), branches.regions(), branches.tag());
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
      std::string bid = _server->registerBranch(rdv_request, datastore, regions, tags[i]);
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
  if (_num_replicas > 1) {
    // FIXME: adapt to register branches for datastores (SERVERLESS VERSION)
    /* rendezvous::RequestContext ctx = request->context();
    int version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
    ctx.mutable_versions()->insert({sid, version});
    response->mutable_context()->CopyFrom(ctx);
    _replica_client.sendRegisterBranchesDatastores(rdv_request->getRid(), bid, service, region, sid, version); */
  }

  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous::CloseBranchMessage* request, 
  rendezvous::Empty* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  const std::string& region = request->region();
  //spdlog::trace("> closing branch w/ full_bid '{}' on region={}", request->bid().c_str(), region.c_str());

  if (region.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_EMPTY_REGION);
  }
  // parse full_bid with format: <bid>:<rid>
  auto ids = _server->parseFullBid(request->bid());
  std::string bid = ids.first;
  std::string rid = ids.second;
  if (rid.empty() || bid.empty()) {
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_BID);
  }

  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  // required for branches that represent service executions (NOT datastore writes)
  // e.g. service A opens a branch for service B and the latter closes it in a different region
  if (CONTEXT_PROPAGATION && _num_replicas > 1) {
    rdv_request->getVersionsRegistry()->waitRemoteVersions(request->context());
  }

  int res = _server->closeBranch(rdv_request, bid, region);
  if (res == 0) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  }
  else if (res == -1) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }

  if (_num_replicas > 1) {
    if (CONTEXT_PROPAGATION) {
      rendezvous::RequestContext ctx = request->context();
      std::string sid = _server->getSid();
      int version = rdv_request->getVersionsRegistry()->getLocalVersion(sid);
      _replica_client.sendCloseBranch(request->bid(), region, sid, version);
    }
    else {
      _replica_client.sendCloseBranch(request->bid(), region);
    }
  }
  
  //spdlog::trace("< closed branch '{}' for request '{}' on region={}", bid.c_str(), rid.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::WaitRequest(grpc::ServerContext* context, 
  const rendezvous::WaitRequestMessage* request, 
  rendezvous::WaitRequestResponse* response) {
  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  const std::string& region = request->region();
  //bool async = request->async();
  int timeout = request->timeout();

  if (timeout < 0) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TIMEOUT);
  }

  if (!tag.empty() && service.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TAG_USAGE);
  }

  //spdlog::trace("> wait request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  if (CONTEXT_PROPAGATION && _num_replicas > 1) {
    rdv_request->getVersionsRegistry()->waitRemoteVersions(request->context());
  }
  
  int result = _server->waitRequest(rdv_request, service, region, tag, true, timeout);
  if (result == 1) {
    response->set_prevented_inconsistency(true);
  }
  else if (result == -1) {
    response->set_timed_out(true);
  }
  //spdlog::trace("< returning wait request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CheckRequest(grpc::ServerContext* context, 
  const rendezvous::CheckRequestMessage* request, 
  rendezvous::CheckRequestResponse* response) {
  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  //spdlog::trace("> check request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
  
  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  int result = _server->checkRequest(rdv_request, service, region);
  response->set_status(static_cast<rendezvous::RequestStatus>(result));
  //spdlog::trace("< returning check request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CheckRequestByRegions(grpc::ServerContext* context, 
  const rendezvous::CheckRequestByRegionsMessage* request, 
  rendezvous::CheckRequestByRegionsResponse* response) {
  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  //spdlog::trace("> check request by regions call for request '{}' on service='{}'", rid.c_str(), service.c_str());

  metadata::Request * rdv_request = _getRequest(rid);
  if (rdv_request == nullptr) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  std::map<std::string, int> result = _server->checkRequestByRegions(rdv_request, service);
  auto * statuses = response->mutable_statuses();
  for (auto pair = result.begin(); pair != result.end(); pair++) {
    std::string region = pair->first;
    int status = pair->second;
    (*statuses)[region] = static_cast<rendezvous::RequestStatus>(status);
  }

  //spdlog::trace("< returning check request by regions call for request '{}' on service='{}'", rid.c_str(), service.c_str());
  return grpc::Status::OK;
}