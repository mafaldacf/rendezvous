#include "version_registry.h"

using namespace replicas;

VersionRegistry::VersionRegistry(int wait_replica_timeout_s) :
    _wait_replica_timeout_s(wait_replica_timeout_s) {}

int VersionRegistry::updateLocalVersion(const std::string& sid) {
    _mutex_versions.lock();
    _versions[sid] = _versions[sid] + 1;
    int version = _versions[sid];
    _mutex_versions.unlock();
    return version;
}

int VersionRegistry::getLocalVersion(const std::string& sid) {
    _mutex_versions.lock();
    int version = _versions[sid];
    _mutex_versions.unlock();
    return version;
}

void VersionRegistry::updateRemoteVersion(const std::string& sid, int version) {
    std::unique_lock<std::mutex> lock(_mutex_versions);
    // ensure replicated requests are processed in FIFO order
    while (version != _versions[sid] + 1) {
        //spdlog::debug("[Versioning] Waiting for remote version: {} -> {}", id, version);
        _cond_versions.wait_for(lock, std::chrono::seconds(_wait_replica_timeout_s));
    }
    _versions[sid] = version;
    //spdlog::debug("[Versioning] Update remote version: {} -> {}", id, version);
    _cond_versions.notify_all();
}

void VersionRegistry::waitRemoteVersion(const std::string& sid, int version) {
    std::unique_lock<std::mutex> lock(_mutex_versions);
    // wait until current replica is consistent to process the client's requests
    //spdlog::debug("[Versioning] Wait remote version: {} -> {}", pair.first, pair.second);
    while (version > _versions[sid]) {
        _cond_versions.wait_for(lock, std::chrono::seconds(_wait_replica_timeout_s));
    }
}