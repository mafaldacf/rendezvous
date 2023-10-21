#include "server_service_impl.h"

using namespace service;

ServerServiceImpl::ServerServiceImpl(std::shared_ptr<rendezvous::Server> server, bool consistency_checks)
    : _server(server), _consistency_checks(consistency_checks) {

}

grpc::Status ServerServiceImpl::RegisterRequest(grpc::ServerContext* context, const rendezvous_server::RegisterRequestMessage* request, rendezvous_server::Empty* response) {
  //if (!_consistency_checks) return grpc::Status::OK;
  //spdlog::trace("> [REPLICATED RR] register request '{}'", request->rid());

  metadata::Request * rv_request;
  std::string rid = request->rid();
  _server->getOrRegisterRequest(rid);
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::RegisterBranch(grpc::ServerContext* context, 
  const rendezvous_server::RegisterBranchMessage* request, 
  rendezvous_server::Empty* response) {

  //if (!_consistency_checks) return grpc::Status::OK;

  const std::string& service = request->service();
  const std::string& tag = request->tag();
  const std::string& rid = request->rid();
  const std::string& async_zone = request->async_zone();
  const std::string& core_bid = request->core_bid();
  const auto& regions = request->regions();
  bool monitor = request->monitor();
  int num = request->regions().size();

  //spdlog::trace("> [REPLICATED RB: {}] register #{} branches on service '{}' (monitor={}) for ids {}:{}:{}", rid, num, service, monitor, core_bid, rid, async_zone);

  metadata::Request * rv_request = _server->getOrRegisterRequest(rid);
  if (rv_request == nullptr) {
    spdlog::critical("< [REPLICATED RB: {}] Error: invalid request for ids {}:{}:{}", rid, core_bid, rid, async_zone);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  
  if (utils::ASYNC_REPLICATION) {
    auto replica_ctx = request->context();
    auto version_registry = rv_request->getVersionsRegistry();

    //spdlog::debug("> [RECEIVED REPL RB: {}:{}:{}] sid: {}, version {}", rid, service, tag, replica_ctx.sid(), replica_ctx.version());
    version_registry->waitRemoteVersion(replica_ctx.sid(), replica_ctx.version()-1);

    _server->registerBranch(rv_request, async_zone, service, regions, tag, request->context().current_service(), core_bid, monitor);

    version_registry->updateRemoteVersion(replica_ctx.sid(), replica_ctx.version());
    //spdlog::debug("> [APPLIED REPL RB: {}:{}:{}] sid: {}, version {}", rid, service, tag, replica_ctx.sid(), replica_ctx.version());
  }
  else {
    _server->registerBranch(rv_request, async_zone, service, regions, tag, request->context().current_service(), core_bid, monitor, true);
  }

  //spdlog::trace("< [REPLICATED RB: {}] registered #{} branches on service '{}' (monitor={}) for ids {}:{}:{}", rid, num, service, monitor, core_bid, rid, async_zone);

  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous_server::CloseBranchMessage* request, 
  rendezvous_server::Empty* response) {

  //if (!_consistency_checks) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& core_bid = request->core_bid();
  const std::string& region = request->region();
  
  //spdlog::trace("> [REPLICATED CB: {}] closing branch on region '{}' for ids {}:{}", rid, region, core_bid, rid);

  metadata::Request * rv_request = _server->getOrRegisterRequest(rid);
  if (rv_request == nullptr) {
    spdlog::critical("< [REPLICATED CB: {}] Error: invalid request for ids {}:{}", rid, core_bid, rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  if (utils::ASYNC_REPLICATION) {
    replicas::VersionRegistry * version_registry;
    auto replica_ctx = request->context();
    rv_request->getVersionsRegistry()->waitRemoteVersion(replica_ctx.sid(), replica_ctx.version());
  }

  // always force close branch when dealing with replicated requests
  int res = _server->closeBranch(rv_request, core_bid, region);

  if (res == 0) {
    spdlog::critical("< [REPLICATED CB: {}] Error: branch not found for ids {}:{}", rid, core_bid, rid);
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  } else if (res == -1) {
    spdlog::critical("< [REPLICATED CB: {}] Error: region '{}' not found for branch for ids {}:{}", rid, region, core_bid, rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }
  //spdlog::trace("< [REPLICATED CB: {}] closed branch on region '{}' for ids {}:{}", rid, region, core_bid, rid);
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::AddWaitLog(grpc::ServerContext* context, 
  const rendezvous_server::AddWaitLogMessage* request, 
  rendezvous_server::Empty* response) {

  //if (!_consistency_checks) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& async_zone_id = request->async_zone();
  const std::string& target_service = request->target_service();
  
  //spdlog::trace("> [BROADCASTED ADD WAIT] adding wait call for root rid '{}' on async zone '{}' to logs", rid, async_zone_id);

  metadata::Request * rv_request = _server->getOrRegisterRequest(rid);
  if (rv_request == nullptr) {
    spdlog::critical("< [BROADCASTED ADD WAIT] Error: invalid root rid {}", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  metadata::Request::AsyncZone * async_zone = rv_request->_validateAsyncZone(async_zone_id);
  if (async_zone == nullptr) {
    spdlog::critical("< [BROADCASTED ADD WAIT] Error: invalid async zone {}", async_zone_id);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_ASYNC_ZONE);
  }

  if (target_service.empty()) {
    rv_request->_addToWaitLogs(async_zone);
  }
  else {
    metadata::Request::ServiceNode * service_node = rv_request->validateServiceNode(target_service);
    //FIXME: we should actually wait for replication
    if (service_node != nullptr) {
      rv_request->_addToServiceWaitLogs(service_node, target_service);
    }
  }
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::RemoveWaitLog(grpc::ServerContext* context, 
  const rendezvous_server::RemoveWaitLogMessage* request, 
  rendezvous_server::Empty* response) {

  //if (!_consistency_checks) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& async_zone_id = request->async_zone();
  const std::string& current_service = request->context().current_service();
  const std::string& target_service = request->target_service();
  
  //spdlog::trace("> [BROADCASTED REMOVE WAIT] remove wait call for root rid '{}' on async zone '{}' to logs", rid, async_zone_id);

  metadata::Request * rv_request = _server->getOrRegisterRequest(rid);
  if (rv_request == nullptr) {
    spdlog::critical("< [BROADCASTED REMOVE WAIT] Error: invalid root rid {}", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  metadata::Request::AsyncZone * async_zone = rv_request->_validateAsyncZone(async_zone_id);
  if (async_zone == nullptr) {
    spdlog::critical("< [BROADCASTED REMOVE WAIT] Error: invalid async zone {}", async_zone_id);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_ASYNC_ZONE);
  }

  if (target_service.empty()) {
    rv_request->_removeFromWaitLogs(async_zone);
  }
  else {
    metadata::Request::ServiceNode * service_node = rv_request->validateServiceNode(target_service);
    //FIXME: we should actually wait for replication
    if (service_node != nullptr) {
      rv_request->_removeFromServiceWaitLogs(service_node, target_service);
    }
  }
  return grpc::Status::OK;
}