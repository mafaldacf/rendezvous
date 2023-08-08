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
    while (version != _versions[id] + 1) {
        _cond_versions.wait_for(lock, std::chrono::seconds(_wait_replica_timeout_s));
    }
    _versions[id] = version;
    _cond_versions.notify_all();
}

void VersionRegistry::waitRemoteVersions(const rendezvous::RequestContext& info) {
    std::unique_lock<std::mutex> lock(_mutex_versions);
    for (const auto & pair : info.versions()) {
        //spdlog::debug("Waiting remote version -> replica {}: remote = {}, local = {}", pair.first, pair.second, _versions[pair.first]);
        const std::string& sid = pair.first;
        while (pair.second != _versions[sid]) {
            _cond_versions.wait_for(lock, std::chrono::seconds(_wait_replica_timeout_s));
        }
    }
}