#include "client_service_impl.h"

using namespace service;

ClientServiceImpl::ClientServiceImpl(
  std::shared_ptr<rendezvous::Server> server, 
  std::vector<std::string> addrs)
  : server(server), _num_wait_calls(0), _replica_client(addrs) {
}

grpc::Status ClientServiceImpl::SubscribeBranches(grpc::ServerContext * context,
  const rendezvous::SubscribeBranchesMessage * request,
  grpc::ServerWriter<rendezvous::SubscribeBranchesResponse> * writer) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;

  metadata::Subscriber * subscriber = server->getSubscriber(request->service(), request->tag(), request->region());
  rendezvous::SubscribeBranchesResponse response;

  while (true) {
    std::string bid = subscriber->popBranch();
    response.set_bid(bid);
    spdlog::debug(">> subscribe requests: sending rid: {}", bid.c_str());
    writer->Write(response);
  }

  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CloseBranches(grpc::ServerContext* context, 
  grpc::ServerReader<rendezvous::CloseBranchMessage>* reader, 
  rendezvous::Empty* response) {

    if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;

    spdlog::debug("> init closing branches");
    
    std::vector<rendezvous::CloseBranchMessage> replica_requests;

    rendezvous::CloseBranchMessage request;
    while (reader->Read(&request)) {


      spdlog::debug("> closing branches: closing branch '{}' on region={}", request.bid().c_str(), request.region().c_str());

      if (request.region().empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_EMPTY_REGION);
      }

      const std::string& rid = server->parseRid(request.bid());
      if (rid.empty()) {
        return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_RID);
      }

      metadata::Request * rdv_request = server->getOrRegisterRequest(rid);
      bool region_found = server->closeBranch(rdv_request, request.bid(), request.region(), true);

      if (!region_found) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_INVALID_REGION);
      }

      //TODO finish
      _replica_client.sendCloseBranch(request.bid(), request.region());
      
      spdlog::debug("< closed branches: closed branch '{}' on region={}", request.bid().c_str(), request.region().c_str());
      return grpc::Status::OK;
    }

    spdlog::debug("< end closing branches");

    return grpc::Status::OK; 
  }

grpc::Status ClientServiceImpl::RegisterRequest(grpc::ServerContext* context, 
  const rendezvous::RegisterRequestMessage* request, 
  rendezvous::RegisterRequestResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  std::string rid = request->rid();
  metadata::Request * rdv_request;

  spdlog::debug("> registering request '{}'", rid.c_str());

  rdv_request = server->getOrRegisterRequest(rid);
  response->set_rid(rdv_request->getRid());

  // initialize empty metadata
  rendezvous::RequestContext ctx;
  response->mutable_context()->CopyFrom(ctx);


  _replica_client.sendRegisterRequest(rdv_request->getRid());

  spdlog::debug("< registered request '{}'", rdv_request->getRid().c_str());
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

  spdlog::debug("> registering branch for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());

  /* if (region.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_EMPTY_REGION);
  } */

  rdv_request = server->getOrRegisterRequest(rid);
  std::string bid = server->registerBranch(rdv_request, service, region, tag);

  // sanity check - must never happen
  if (bid.empty()) {
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, utils::ERR_MSG_BRANCH_ALREADY_EXISTS);
  }
  
  std::string sid = server->getSid();
  int version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
  ctx.mutable_versions()->insert({sid, version});

  response->set_rid(rdv_request->getRid());
  response->set_bid(bid);
  response->mutable_context()->CopyFrom(ctx);


  _replica_client.sendRegisterBranch(rdv_request->getRid(), bid, service, region, sid, version);

  spdlog::debug("< registered branch '{}' for request '{}' on service='{}' and region='{}'", bid.c_str(), rdv_request->getRid().c_str(), service.c_str(), region.c_str());
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

  // workaround of spdlog::debugging
  if (DEBUG) {
    std::cout << "[RegisterBranches] > registering " << num << " branches for request '" << rid << "' on service '" << service << "' and regions ";
    for (const std::string& region : request->regions()) {
      std::cout << "'" << region << "' ";
    }
    std::cout << std::endl;
  }

  if (num == 0) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_EMPTY_REGION);
  }

  const auto& regions = request->regions();
  rdv_request = server->getOrRegisterRequest(rid);
  std::string bid = server->registerBranches(rdv_request, service, regions, tag);

  // sanity check - must never happen
  if (bid.empty()) {
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, utils::ERR_MSG_BRANCH_ALREADY_EXISTS);
  }

  std::string sid = server->getSid();
  int version = rdv_request->getVersionsRegistry()->updateLocalVersion(sid);
  ctx.mutable_versions()->insert({sid, version});
  
  response->set_rid(rdv_request->getRid());
  response->set_bid(bid);
  response->mutable_context()->CopyFrom(ctx);


  _replica_client.sendRegisterBranches(rdv_request->getRid(), bid, service, regions, sid, version);

  // workaround of spdlog::debugging
  if (DEBUG) {
    std::cout << "[RegisterBranches] < registered " << num << " branches '" << bid << "' for request '" << rdv_request->getRid() << "' on service '" << service << "' and regions ";
    for (const std::string& region : request->regions()) {
      std::cout << "'" << region << "' ";
    }
    std::cout << std::endl;
  }

  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous::CloseBranchMessage* request, 
  rendezvous::Empty* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& bid = request->bid();
  const std::string& region = request->region();

  spdlog::debug("> closing branch '{}' for request on region={}", bid.c_str(), region.c_str());

  if (region.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_EMPTY_REGION);
  }

  const std::string& rid = server->parseRid(bid);
  if (rid.empty()) {
    return grpc::Status(grpc::StatusCode::INTERNAL, utils::ERR_PARSING_RID);
  }


  metadata::Request * rdv_request = server->getOrRegisterRequest(rid);
  int res = server->closeBranch(rdv_request, bid, region, true);

  if (res == 0) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  }
  else if (res == -1) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }
  
  _replica_client.sendCloseBranch(bid, region);
  
  spdlog::debug("< closed branch '{}' for request '{}' on region={}", bid.c_str(), rid.c_str(), region.c_str());

  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::WaitRequest(grpc::ServerContext* context, 
  const rendezvous::WaitRequestMessage* request, 
  rendezvous::WaitRequestResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  const std::string& region = request->region();
  rendezvous::RequestContext ctx = request->context();

  spdlog::debug("> wait request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());

  metadata::Request * rdv_request = server->getOrRegisterRequest(rid);
  rdv_request->getVersionsRegistry()->waitRemoteVersions(ctx);

  int result = server->waitRequest(rdv_request, service, region);

  // inconsistency was prevented
  if (result == 1) {
    response->set_prevented_inconsistency(true);
    if (DEBUG) {
      spdlog::info("prevented inconsistencies: %ld", server->getNumInconsistencies());
    }
  }

  if (DEBUG) {
    int n = _num_wait_calls.fetch_add(1) + 1;
    spdlog::debug("wait request calls = {}", n);
  }

  spdlog::debug("< returning wait request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
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

  spdlog::debug("> check request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());

  metadata::Request * rdv_request = server->getOrRegisterRequest(rid);
  //rdv_request->getVersionsRegistry()->waitRemoteVersions(ctx);

  int result = server->checkRequest(rdv_request, service, region);

  response->set_status(static_cast<rendezvous::RequestStatus>(result));

  spdlog::debug("< returning check request call for request '{}' on service='{}' and region='{}'", rid.c_str(), service.c_str(), region.c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::CheckRequestByRegions(grpc::ServerContext* context, 
  const rendezvous::CheckRequestByRegionsMessage* request, 
  rendezvous::CheckRequestByRegionsResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  const std::string& rid = request->rid();
  const std::string& service = request->service();
  rendezvous::RequestContext ctx = request->context();

  spdlog::debug("> check request by regions call for request '{}' on service='{}'", rid.c_str(), service.c_str());

  metadata::Request * rdv_request = server->getOrRegisterRequest(rid);
  //rdv_request->getVersionsRegistry()->waitRemoteVersions(ctx);

  std::map<std::string, int> result = server->checkRequestByRegions(rdv_request, service);

  auto * statuses = response->mutable_statuses();
  for (auto pair = result.begin(); pair != result.end(); pair++) {
    std::string region = pair->first;
    int status = pair->second;
    (*statuses)[region] = static_cast<rendezvous::RequestStatus>(status);
  }

  spdlog::debug("< returning check request by regions call for request '{}' on service='{}'", rid.c_str(), service.c_str());
  return grpc::Status::OK;
}

grpc::Status ClientServiceImpl::GetNumPreventedInconsistencies(grpc::ServerContext* context, 
  const rendezvous::Empty* request, 
  rendezvous::GetNumPreventedInconsistenciesResponse* response) {

  if (SKIP_CONSISTENCY_CHECKS) return grpc::Status::OK;
  
  long value = server->getNumPreventedInconsistencies();
  response->set_inconsistencies(value);

  spdlog::debug("number of prevented inconsistencies: %ld", value);
  return grpc::Status::OK;
}