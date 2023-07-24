#include "client_service_impl.h"

using namespace service;

ClientServiceImpl::ClientServiceImpl(
  std::shared_ptr<rendezvous::Server> server, 
  std::vector<std::string> addrs)
  : _server(server), _num_wait_calls(0), _replica_client(addrs) {
}

grpc::Status ClientServiceImpl::SubscribeBranches(grpc::ServerContext * context,
  const rendezvous::SubscribeBranchesMessage * request,
  grpc::ServerWriter<rendezvous::SubscribeBranchesResponse> * writer) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;

  spdlog::debug("[SUBSCRIBER] loading subscriber for service '{}', tag '{}' and region '{}'", request->service().c_str(), request->tag().c_str(), request->region().c_str());
  metadata::Subscriber * subscriber = _server->getSubscriber(request->service(), request->tag(), request->region());
  rendezvous::SubscribeBranchesResponse response;

  while (!context->IsCancelled()) {
    std::string bid = subscriber->popBranch(context);
    if (!bid.empty()) {
      response.set_bid(bid);
      spdlog::debug("[SUBSCRIBER] sending bid -->  {}", bid.c_str());
      writer->Write(response);
    }
  }

  spdlog::debug("[SUBSCRIBER] context CANCELLED for service '{}', tag '{}' and region '{}'", request->service().c_str(), request->tag().c_str(), request->region().c_str());

  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CloseBranches(grpc::ServerContext* context, 
  grpc::ServerReader<rendezvous::CloseBranchMessage>* reader, 
  rendezvous::Empty* response) {

    if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;

    spdlog::debug("> [CLOSE STREAM] init closing branches");
    std::vector<rendezvous::CloseBranchMessage> replica_requests;
    rendezvous::CloseBranchMessage request;
    while (reader->Read(&request)) {
      spdlog::trace("> [CLOSE STREAM] -> closing branch w/ full bid'{}' on region={}", request.bid().c_str(), request.region().c_str());
      if (request.region().empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_EMPTY_REGION);
      }
      // full_bid format: <bid>:<rid>
      auto ids = _server->parseFullBid(request.bid());
      std::string bid = ids.first;
      std::string rid = ids.second;
      if (rid.empty() || bid.empty()) {
        return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_BID);
      }

      metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
      bool region_found = _server->closeBranch(rdv_request, bid, request.region());
      if (!region_found) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_INVALID_REGION);
      }

      _replica_client.sendCloseBranch(request.bid(), request.region());
      spdlog::trace("< [CLOSE STREAM] close branches stream -> closed branch '{}' on region={}", request.bid().c_str(), request.region().c_str());
      return grpc::Status::OK;
    }

    spdlog::trace("< [CLOSE STREAM] end closing branches");
    return grpc::Status::OK; 
  }

grpc::Status ClientServiceImpl::RegisterRequest(grpc::ServerContext* context, 
  const rendezvous::RegisterRequestMessage* request, 
  rendezvous::RegisterRequestResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  std::string rid = request->rid();
  metadata::Request * rdv_request;

  spdlog::trace("> registering request '{}'", rid.c_str());

  rdv_request = _server->getOrRegisterRequest(rid);
  response->set_rid(rdv_request->getRid());

  // initialize empty metadata
  rendezvous::RequestContext ctx;
  response->mutable_context()->CopyFrom(ctx);
  _replica_client.sendRegisterRequest(rdv_request->getRid());

  spdlog::trace("< registered request '{}'", rdv_request->getRid().c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::RegisterBranch(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchMessage* request, 
  rendezvous::RegisterBranchResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;

  std::string rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  const std::string& tag = request->tag();
  rendezvous::RequestContext ctx = request->context();
  metadata::Request * rdv_request;

  spdlog::trace("> registering branch for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());

  rdv_request = _server->getOrRegisterRequest(rid);
  std::string bid = _server->registerBranch(rdv_request, service, region, tag);
  std::string sid = _server->getSid();
  int version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
  ctx.mutable_versions()->insert({sid, version});
  response->set_rid(rdv_request->getRid());
  response->set_bid(bid);
  response->mutable_context()->CopyFrom(ctx);

  _replica_client.sendRegisterBranch(rdv_request->getRid(), bid, service, region, sid, version);
  spdlog::trace("< registered branch '{}' for request '{}' on service='{}' and region='{}'", bid.c_str(), rdv_request->getRid().c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::RegisterBranches(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchesMessage* request, 
  rendezvous::RegisterBranchesResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& tag = request->tag();
  rendezvous::RequestContext ctx = request->context();
  metadata::Request * rdv_request;
  int num = request->regions().size();

  spdlog::trace("> registering {} branches for request '{}' on service '{}'", num, rid, service);

  if (num == 0) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_EMPTY_REGION);
  }

  const auto& regions = request->regions();
  rdv_request = _server->getOrRegisterRequest(rid);
  std::string bid = _server->registerBranches(rdv_request, service, regions, tag);
  std::string sid = _server->getSid();
  int version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
  ctx.mutable_versions()->insert({sid, version});
  
  response->set_rid(rdv_request->getRid());
  response->set_bid(bid);
  response->mutable_context()->CopyFrom(ctx);

  _replica_client.sendRegisterBranches(rdv_request->getRid(), bid, service, regions, sid, version);
  spdlog::trace("< registered {} branches '{}' for request '{}' on service '{}'", num, bid, rid, service);
  return grpc::Status::OK;
}


grpc::Status ClientServiceImpl::RegisterBranchesDatastores(grpc::ServerContext* context, 
  const rendezvous::RegisterBranchesDatastoresMessage* request, 
  rendezvous::RegisterBranchesDatastoresResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& rid = request->rid();
  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);

  // client specifies different regions for each datastores using the DatastoreBranching type
  if (request->branches().size() > 0){
    for (const auto& branches : request->branches()) {
      const std::string& datastore = branches.datastore();
      const auto& regions = branches.regions();
      std::string bid = _server->registerBranches(rdv_request, datastore, regions, "");
      if (bid.empty()) {
        return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, utils::ERR_MSG_BRANCH_ALREADY_EXISTS);
      }
      response->add_bids(bid);
    }
  }

  // client uses same set of regions for all datastores
  else if (request->datastores().size() > 0 && request->regions().size() > 0) {
    const auto& regions = request->regions();
    for (const auto& datastore: request->datastores()) {
      std::string bid = _server->registerBranches(rdv_request, datastore, regions, "");
      if (bid.empty()) {
        return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, utils::ERR_MSG_BRANCH_ALREADY_EXISTS);
      }
      response->add_bids(bid);
    }
  }

  else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_REGISTER_BRANCHES_INVALID_DATASTORES);
  }

  response->set_rid(rdv_request->getRid());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous::CloseBranchMessage* request, 
  rendezvous::Empty* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  spdlog::trace("> closing branch w/ full_bid '{}' on region={}", request->bid().c_str(), request->region().c_str());

  if (request->region().empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_EMPTY_REGION);
  }
  // full_bid format: <bid>:<rid>
  auto ids = _server->parseFullBid(request->bid());
  std::string bid = ids.first;
  std::string rid = ids.second;
  if (rid.empty() || bid.empty()) {
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_BID);
  }

  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  int res = _server->closeBranch(rdv_request, bid, request->region());
  if (res == 0) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  }
  else if (res == -1) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }
  _replica_client.sendCloseBranch(request->bid(), request->region());
  spdlog::trace("< closed branch '{}' for request '{}' on region={}", bid.c_str(), rid.c_str(), request->region().c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::WaitRequest(grpc::ServerContext* context, 
  const rendezvous::WaitRequestMessage* request, 
  rendezvous::WaitRequestResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  int timeout = request->timeout();
  rendezvous::RequestContext ctx = request->context();

  if (timeout < 0) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_TIMEOUT);
  }
  spdlog::trace("> wait request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  rdv_request->getVersionsRegistry()->waitRemoteVersions(ctx);
  int result = _server->waitRequest(rdv_request, service, region, timeout);

  spdlog::trace("< returning wait request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CheckRequest(grpc::ServerContext* context, 
  const rendezvous::CheckRequestMessage* request, 
  rendezvous::CheckRequestResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  rendezvous::RequestContext ctx = request->context();

  spdlog::trace("> check request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  int result = _server->checkRequest(rdv_request, service, region);
  response->set_status(static_cast<rendezvous::RequestStatus>(result));
  spdlog::trace("< returning check request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CheckRequestByRegions(grpc::ServerContext* context, 
  const rendezvous::CheckRequestByRegionsMessage* request, 
  rendezvous::CheckRequestByRegionsResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  rendezvous::RequestContext ctx = request->context();

  spdlog::trace("> check request by regions call for request '{}' on service='{}'", rid.c_str(), service.c_str());
  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  std::map<std::string, int> result = _server->checkRequestByRegions(rdv_request, service);

  auto * statuses = response->mutable_statuses();
  for (auto pair = result.begin(); pair != result.end(); pair++) {
    std::string region = pair->first;
    int status = pair->second;
    (*statuses)[region] = static_cast<rendezvous::RequestStatus>(status);
  }

  spdlog::trace("< returning check request by regions call for request '{}' on service='{}'", rid.c_str(), service.c_str());
  return grpc::Status::OK;
}