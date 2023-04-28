#include "version_registry.h"

using namespace replicas;

VersionRegistry::VersionRegistry() {}

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
    if (DEBUG) {
        std::cout << "[INFO] waiting remote versions: ";
    }

    std::unique_lock<std::mutex> lock(_mutex_versions);
    for (const auto & pair : info.versions()) {
        if (DEBUG) {
            std::cout << "[" << pair.first << ", " << pair.second << "]" << std::endl;
        }
        while (pair.second > _versions[pair.first]) {
            _cond_versions.wait(lock);
        }
    }

    if (DEBUG) {
        std::cout << "[INFO] returning from waiting remote versions" << std::endl;
    }
}