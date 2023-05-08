#include "version_registry.h"

using namespace replicas;

VersionRegistry::VersionRegistry(int wait_replica_timeout_s) :
    _wait_replica_timeout_s(wait_replica_timeout_s) {}

int VersionRegistry::updateLocalVersion(const std::string& id) {
    _mutex_versions.lock();
    int version = ++_versions[id];
    _mutex_versions.unlock();
    return version;
}

void VersionRegistry::updateRemoteVersion(const std::string& id, const int& version) {
    std::unique_lock<std::mutex> lock(_mutex_versions);

    while (version != _versions[id] + 1) {
        _cond_versions.wait(lock);
    }

    _versions[id] = version;
    _cond_versions.notify_all();
}

void VersionRegistry::waitRemoteVersions(const rendezvous::RequestContext& info) {
    std::unique_lock<std::mutex> lock(_mutex_versions);
    for (const auto & pair : info.versions()) {
        spdlog::debug("Waiting remote version -> replica {}: remote = {}, local = {}", pair.first, pair.second, _versions[pair.first]);
    }
}