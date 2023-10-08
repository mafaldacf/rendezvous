#include "server_service_impl.h"

using namespace service;

ServerServiceImpl::ServerServiceImpl(std::shared_ptr<rendezvous::Server> server)
    : _server(server) {
}

void ServerServiceImpl::_waitReplicaVersions(metadata::Request * rdv_request, rendezvous_server::RequestContext rdv_context) {
  auto versions_registry = rdv_request->getVersionsRegistry();
  // pair: <sid, version>
  for (const auto& v : rdv_context.versions()) {
    versions_registry->updateRemoteVersion(v.first, v.second);
  }
}

grpc::Status ServerServiceImpl::RegisterRequest(grpc::ServerContext* context, const rendezvous_server::RegisterRequestMessage* request, rendezvous_server::Empty* response) {
  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;
  spdlog::trace("> [REPLICATED RR] register request '{}'", request->rid());

  metadata::Request * rdv_request;
  std::string rid = request->rid();
  _server->getOrRegisterRequest(rid);
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::RegisterBranch(grpc::ServerContext* context, 
  const rendezvous_server::RegisterBranchMessage* request, 
  rendezvous_server::Empty* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& service = request->service();
  const std::string& tag = request->tag();
  const std::string& rid = request->rid();
  const std::string& async_zone = request->async_zone();
  const std::string& core_bid = request->core_bid();
  const auto& regions = request->regions();
  bool monitor = request->monitor();
  int num = request->regions().size();

  spdlog::trace("> [REPLICATED RB] register #{} branches on service '{}' (monitor={}) for ids {}:{}:{}", num, service, monitor, core_bid, rid, async_zone);

  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::critical("< [REPLICATED CB] Error: invalid request for ids {}:{}:{}", core_bid, rid, async_zone);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }
  
  if (utils::ASYNC_REPLICATION) _waitReplicaVersions(rdv_request, request->context());
  _server->registerBranch(rdv_request, async_zone, service, regions, tag, request->context().parent_service(), core_bid, monitor);

  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::CloseBranch(grpc::ServerContext* context, 
  const rendezvous_server::CloseBranchMessage* request, 
  rendezvous_server::Empty* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& core_bid = request->core_bid();
  const std::string& region = request->region();
  
  spdlog::trace("> [REPLICATED CB] closing branch on region '{}' for ids {}:{}", region, core_bid, rid);

  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::critical("< [REPLICATED CB] Error: invalid request for ids {}:{}", core_bid, rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  if (utils::ASYNC_REPLICATION) _waitReplicaVersions(rdv_request, request->context());

  // always force close branch when dealing with replicated requests
  int res = _server->closeBranch(rdv_request, core_bid, region, true);

  if (res == 0) {
    spdlog::critical("< [REPLICATED CB] Error: branch not found for ids {}:{}", core_bid, rid);
    return grpc::Status(grpc::StatusCode::NOT_FOUND, utils::ERR_MSG_BRANCH_NOT_FOUND);
  } else if (res == -1) {
    spdlog::critical("< [REPLICATED CB] Error: region '{}' not found for branch for ids {}:{}", region, core_bid, rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REGION);
  }
  
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::AddWaitLog(grpc::ServerContext* context, 
  const rendezvous_server::AddWaitLogMessage* request, 
  rendezvous_server::Empty* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& async_zone = request->async_zone();
  
  spdlog::trace("> [BROADCASTED ADD WAIT] adding wait call for root rid '{}' on async zone '{}' to logs", rid, async_zone);

  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::critical("< [BROADCASTED ADD WAIT] Error: invalid root rid {}", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  if (utils::ASYNC_REPLICATION) _waitReplicaVersions(rdv_request, request->context());

  metadata::Request::AsyncZone * subrequest = rdv_request->_validateSubRid(async_zone);
  if (subrequest == nullptr) {
    spdlog::critical("< [BROADCASTED ADD WAIT] Error: invalid async zone {}", async_zone);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_ASYNC_ZONE);
  }

  rdv_request->_addToWaitLogs(subrequest);
  return grpc::Status::OK;
}

grpc::Status ServerServiceImpl::RemoveWaitLog(grpc::ServerContext* context, 
  const rendezvous_server::RemoveWaitLogMessage* request, 
  rendezvous_server::Empty* response) {

  if (!utils::CONSISTENCY_CHECKS) return grpc::Status::OK;

  const std::string& rid = request->rid();
  const std::string& async_zone = request->async_zone();
  
  spdlog::trace("> [BROADCASTED ADD WAIT] adding wait call for root rid '{}' on async zone '{}' to logs", rid, async_zone);

  metadata::Request * rdv_request = _server->getOrRegisterRequest(rid);
  if (rdv_request == nullptr) {
    spdlog::critical("< [BROADCASTED ADD WAIT] Error: invalid root rid {}", rid);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_REQUEST);
  }

  if (utils::ASYNC_REPLICATION) _waitReplicaVersions(rdv_request, request->context());

  metadata::Request::AsyncZone * subrequest = rdv_request->_validateSubRid(async_zone);
  if (subrequest == nullptr) {
    spdlog::critical("< [BROADCASTED ADD WAIT] Error: invalid async zone {}", async_zone);
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, utils::ERR_MSG_INVALID_ASYNC_ZONE);
  }

  rdv_request->_removeFromWaitLogs(subrequest);
  return grpc::Status::OK;
}