#include "version_registry.h"

using namespace replicas;

VersionRegistry::VersionRegistry(int wait_replica_timeout_s) :
    _wait_replica_timeout_s(wait_replica_timeout_s) {}

int VersionRegistry::updateLocalVersion(const std::string& id) {
    _mutex_versions.lock();
    _versions[id] = _versions[id] + 1;
    int version = _versions[id];
    _mutex_versions.unlock();
    return version;
}

int VersionRegistry::getLocalVersion(const std::string& id) {
    _mutex_versions.lock();
    int version = _versions[id];
    _mutex_versions.unlock();
    return version;
}

void VersionRegistry::updateRemoteVersion(const std::string& id, const int& version) {
    std::unique_lock<std::mutex> lock(_mutex_versions);
    // ensure replicated requests are processed in FIFO order
    while (version != _versions[id] + 1) {
        _cond_versions.wait_for(lock, std::chrono::seconds(_wait_replica_timeout_s));
    }
    _versions[id] = version;
    //spdlog::debug("[Versioning] Update remote version: {} -> {}", id, version);
    _cond_versions.notify_all();
}

void VersionRegistry::waitRemoteVersions(const rendezvous::RequestContext& info) {
    std::unique_lock<std::mutex> lock(_mutex_versions);
    // wait until current replica is consistent to process the client's requests
    for (const auto & pair : info.versions()) {
        const std::string& sid = pair.first;
        while (pair.second != _versions[sid]) {
            //spdlog::debug("[Versioning] Wait remote version: {} -> {}", pair.first, pair.second);
            _cond_versions.wait_for(lock, std::chrono::seconds(_wait_replica_timeout_s));
        }
    }
}