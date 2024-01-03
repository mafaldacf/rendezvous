#include "client_service_impl.h"

using namespace service;

ClientServiceImpl::ClientServiceImpl(
  std::shared_ptr<rendezvous::Server> server, 
  std::vector<std::string> addrs, bool consistency_checks)
  : _server(server), _num_wait_calls(0), _replica_client(addrs),
  _num_replicas(addrs.size()+1) /* current replica is not part of the address vector */ ,
  _consistency_checks(consistency_checks)
  
  {

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

  //if (!_consistency_checks) return grpc::Status::OK;
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

  //if (!_consistency_checks) return grpc::Status::OK;
  std::string rid = request->rid();
  metadata::Request * rv_request;
  //spdlog::trace("> [RR] register request '{}'", rid.c_str());
  rv_request = _server->getOrRegisterRequest(rid);
  response->set_rid(rv_request->getRid());
  
  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    _replica_client.registerRequest(rv_request->getRid());
  }
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::RegisterBranch(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchMessage* request, 
  rendezvous::RegisterBranchResponse* response) {

  //if (!_consistency_checks) return grpc::Status::OK;
  std::string rid = request->rid();
  const std::string& bid = request->bid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  const std::string& acsl_id = request->acsl().empty() ? utils::ROOT_ACSL_ID : request->acsl();
  bool monitor = request->monitor();
  int num = request->regions().size();
  const auto& regions = request->regions();
  // TO BE PARSED!
  std::string current_service_bid = request->current_service_bid();

  if (bid.empty()) {
    //spdlog::trace("> [RB: {}] register #{} branches on service '{}:{}' @ acsl {} (monitor={})", rid, num, service, tag, acsl_id, monitor);
  }
  else {
    //spdlog::trace("> [RB: {}] register #{} branches with bid '{}' on service '{}:{}' @ acsl {} (monitor={})", rid, num, bid, service, tag, acsl_id, monitor);
  }
  
  if (service.empty()) {
    spdlog::error("< [RB: {}] Error: service empty", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_SERVICE_EMPTY);
  }

  metadata::Request * rv_request = _getRequest(rid);
  if (rv_request == nullptr) {
    spdlog::error("< [RB: {}] Error: invalid rid", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  std::string core_bid = "";

  if (bid.empty()) {
    core_bid = _server->genBid(rv_request);
  } else {
    core_bid = _server->parseFullId(bid).first;
  }

  if (!current_service_bid.empty()) {
    current_service_bid = _server->parseFullId(current_service_bid).first;
  }

  auto branch = _server->registerBranch(rv_request, acsl_id, service, regions, tag, current_service_bid, core_bid, monitor);

  // could not create branch (tag already exists)
  if (!branch) {
    spdlog::error("< [RB: {}] Error: could not register branch for core bid {}", rid, core_bid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_REGISTERING_BRANCH);
  }

  response->set_rid(rid);
  response->set_bid(_server->composeFullId(core_bid, rid));

  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    rendezvous_server::RequestContext ctx_replica;
    if (utils::ASYNC_REPLICATION) {
      const std::string& sid = _server->getSid();
      int new_version = rv_request->getVersionsRegistry()->updateLocalVersion(sid);
      rv_request->setBranchReplicationReady(branch);
      ctx_replica.set_sid(sid);
      ctx_replica.set_version(new_version);
    }
    //spdlog::debug("> [SENDING REPL RB: {}:{}:{}] sid: {}, version {}", rid, service, tag, ctx_replica.sid(), ctx_replica.version());
    _replica_client.registerBranch(rid, acsl_id, core_bid, service, tag, regions, monitor, ctx_replica);

  }

  //spdlog::trace("< [RB: {}] registered branch with bid {} on service '{}:{}' @ acsl {} and #{} regions (monitor={})", rid, core_bid, service, tag, acsl_id, num, monitor);
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::RegisterBranches(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchesMessage* request, 
  rendezvous::RegisterBranchesResponse* response) {

  //if (!_consistency_checks) return grpc::Status::OK;
  const std::string& rid = request->rid();
  const std::string& current_service_bid = request->current_service_bid();
  auto branches = request->branches();

  //spdlog::trace("> [RBs: {}] register #{} service branches for request '{}')", rid, branches.size());

  metadata::Request * rv_request = _getRequest(rid);
  if (rv_request == nullptr) {
    spdlog::error("< [RBs] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  for (const auto& branch: branches) {
    const std::string& core_bid = _server->genBid(rv_request);
    auto new_branch = _server->registerBranch(rv_request, branch.acsl(), branch.service(), branch.regions(), branch.tag(), current_service_bid, core_bid, branch.monitor());

    // could not create branch (tag already exists)
    if (!new_branch) {
      spdlog::error("< [RBs] Error: could not register branch for core bid {}", core_bid);
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_REGISTERING_BRANCH);
    }

    response->set_rid(rid);
    response->add_bids(_server->composeFullId(core_bid, rid));

    // replicate client request to remaining replicas
    if (_num_replicas > 1) {
      rendezvous_server::RequestContext ctx_replica;
      if (utils::ASYNC_REPLICATION) {
        const std::string& sid = _server->getSid();
        int new_version = rv_request->getVersionsRegistry()->updateLocalVersion(sid);
        rv_request->setBranchReplicationReady(new_branch);
        ctx_replica.set_sid(sid);
        ctx_replica.set_version(new_version);
      }
      _replica_client.registerBranch(rid, branch.acsl(), core_bid, branch.service(), branch.tag(), branch.regions(), branch.monitor(), ctx_replica);
    }
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

  //if (!_consistency_checks) return grpc::Status::OK;

  const std::string& region = request->region();
  const std::string& composed_bid = request->bid();
  const auto& visible_bids = request->visible_bids();


  // parse composed id into <bid, root_rid>
  auto ids = _server->parseFullId(composed_bid);
  const std::string& bid = ids.first;
  const std::string& root_rid = ids.second;
  if (bid.empty() || root_rid.empty()) {
    spdlog::error("< [CB] Error parsing composed bid '{}'", composed_bid);
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_BID);
  }

  //spdlog::trace("> [CB: {}] closing branch with bid '{}' on region '{}'", root_rid, bid, region);

  metadata::Request * rv_request = _getRequest(root_rid);
  if (rv_request == nullptr) {
    spdlog::error("< [CB: {}] Error: invalid request", root_rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  if (rv_request->isClosed()) {
    return grpc::Status::OK;
  }

  // clients wants to ensure that all provided bids (previously registered) are closed
  // this happens in case of previous async register branch calls in the same service
  if (utils::ASYNC_REPLICATION && visible_bids.size() != 0) {
    std::vector<std::string> visible_bids_vec = std::vector<std::string>();
    for (const auto& visible_bid: visible_bids) {
      visible_bids_vec.emplace_back(_server->parseFullId(visible_bid).first);
    }
    bool are_visible = rv_request->waitBranchesReplicationReady(visible_bids_vec);
    if (!are_visible) {
      spdlog::error("< [CB] Timedout waiting for #{} bids to be visible when closing branch: {}", visible_bids.size(), composed_bid);
      return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, utils::ERR_MSG_VISIBLE_BIDS_TIMEOUT);
    }
  }

  int res = _server->closeBranch(rv_request, bid, region);
  if (res == 0) {
    spdlog::error("< [CB: {}] Error: branch not found for composed bid: {}", root_rid, composed_bid);
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  } else if (res == -1) {
    spdlog::error("< [CB: {}] Error: region '{}' or branch not found after timeout for composed_bid '{}'", root_rid, region, composed_bid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }

  // replicate client request to remaining replicas
  if (_num_replicas > 1) {
    rendezvous_server::RequestContext ctx_replica;
    if (utils::ASYNC_REPLICATION) {
      const std::string& sid = _server->getSid();
      int version = rv_request->getVersionsRegistry()->getLocalVersion(sid);
      ctx_replica.set_sid(sid);
      ctx_replica.set_version(version);
    }
    //spdlog::debug("> [SENDING REPL CB: {}:{}] sid: {}, version {}", root_rid, bid, ctx_replica.sid(), ctx_replica.version());
    _replica_client.closeBranch(root_rid, bid, region, ctx_replica);
  }
  //spdlog::trace("< [CB: {}] closed branch with bid '{}' on region '{}'", root_rid, bid, region);
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::WaitRequest(grpc::ServerContext* context, 
  const rendezvous::WaitRequestMessage* request, 
  rendezvous::WaitRequestResponse* response) {
  //if (!_consistency_checks) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const auto& services = request->services();
  const std::string& tag = request->tag();
  const std::string& region = request->region();
  bool wait_deps = request->wait_deps();
  const std::string& acsl_id = request->acsl().empty() ? utils::ROOT_ACSL_ID : request->acsl();
  const std::string& current_service = request->current_service();
  //bool async = request->async();
  int timeout = request->timeout();

  //spdlog::trace("> [WR: {}] wait call targeting service '{}' and region '{}' @ acsl {}", rid, service, region, acsl_id);

  // validate parameters
  if (timeout < 0) {
    spdlog::error("< [WR: {}] Error: invalid timeout ({})", rid, timeout);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TIMEOUT);
  } if (!service.empty() && services.size() > 0) {
    spdlog::error("< [WR: {}] Error: cannot provide 'service' and 'services' parameters simultaneously", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_SERVICES_EXCLUSIVE);
  } if (!tag.empty() && service.empty()) {
    spdlog::error("< [WR: {}] Error: service not specified for tag '{}'", rid, tag);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TAG_USAGE);
  }

  // check if request exists
  metadata::Request * rv_request = _getRequest(rid);
  if (rv_request == nullptr) {
    spdlog::error("< [WR: {}] Error: invalid request", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  if (rv_request->isClosed()) {
    return grpc::Status::OK;
  }

  int result;
  replicas::ReplicaClient::AsyncRequestHelper * async_request_helper = _replica_client.addWaitLog(rid, acsl_id, service);

  // perform wait on a single service
  if (services.size() == 0) {
    result = _server->wait(rv_request, acsl_id, service, region, tag, utils::ASYNC_REPLICATION, timeout, current_service, wait_deps);
    if (result == 1) {
      response->set_prevented_inconsistency(true);
    }
  }
  // otherwise, perform wait on multiple provided services
  else {
    for (const auto& service: services) {
      result = _server->wait(rv_request, acsl_id, service, region, tag, utils::ASYNC_REPLICATION, timeout, current_service, wait_deps);
      if (result < 0) {
        break;
      }
    }
  }

  _replica_client.removeWaitLog(rid, acsl_id, service, async_request_helper);

  // parse errors
  if (result == -1) {
    response->set_timed_out(true);
  } else if (result == -2) {
    spdlog::error("< [WR: {}] Error: invalid context (service/region)", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_SERVICE_REGION);
  } else if (result == -3) {
    spdlog::error("< [WR: {}] Error: current service branch needs to be registered before any wait call", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_WAIT_CALL_NO_CURRENT_SERVICE);
  } else if (result == -4) {
    spdlog::error("< [WR: {}] Error: invalid tag", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TAG);
  }
  //spdlog::trace("< [WR: {}] returning call targeting service '{}' and region '{}' (r={})", rid, service, region, result);
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CheckStatus(grpc::ServerContext* context, 
  const rendezvous::CheckStatusMessage* request, 
  rendezvous::CheckStatusResponse* response) {
  //if (!_consistency_checks) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  const std::string& acsl_id = request->acsl().empty() ? utils::ROOT_ACSL_ID : request->acsl();
  bool detailed = request->detailed();

  //spdlog::trace("> [CS] query for request '{}' on service '{}' and region '{}' @ acsl {} (detailed={})", rid, service, region, acsl, detailed);

  // check if request exists
  metadata::Request * rv_request = _getRequest(rid);
  if (rv_request == nullptr) {
    spdlog::error("< [CS] Error: invalid request for composed rid '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  if (rv_request->isClosed()) {
    response->set_status(rendezvous::RequestStatus::CLOSED);
    return grpc::Status::OK;
  }

  const auto& result = _server->checkStatus(rv_request, acsl_id, service, region, detailed);

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
  //if (!_consistency_checks) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& acsl_id = request->acsl().empty() ? utils::ROOT_ACSL_ID : request->acsl();

  //spdlog::trace("> [FD] query for request '{}' on service '{}'", rid, service);
  
  // check if request exists
  metadata::Request * rv_request = _getRequest(rid);
  if (rv_request == nullptr) {
    spdlog::error("< [FD] Error: invalid request '{}'", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  if (rv_request->isClosed()) {
    return grpc::Status::OK;
  }

  const auto& result = _server->fetchDependencies(rv_request, service, acsl_id);

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
