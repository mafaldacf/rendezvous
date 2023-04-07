#include "version_registry.h"

using namespace replicas;

VersionRegistry::VersionRegistry() {}

int VersionRegistry::updateLocalVersion(const std::string& id) {
    mutex_versions.lock();
    int version = ++versions[id];
    mutex_versions.unlock();
    return version;
}

void VersionRegistry::updateRemoteVersion(const std::string& id, const int& version) {
    std::unique_lock<std::mutex> lock(mutex_versions);

    while (version != versions[id] + 1) {
        cond_versions.wait(lock);
    }

    versions[id] = version;
    cond_versions.notify_all();
}

void VersionRegistry::waitRemoteVersions(const rendezvous::RequestContext& info) {
    if (DEBUG) {
        std::cout << "[INFO] waiting remote versions: ";
    }

    std::unique_lock<std::mutex> lock(mutex_versions);
    for (const auto & pair : info.versions()) {
        if (DEBUG) {
            std::cout << "[" << pair.first << ", " << pair.second << "]" << std::endl;
        }
        while (pair.second > versions[pair.first]) {
            cond_versions.wait(lock);
        }
    }

    if (DEBUG) {
        std::cout << "[INFO] returning from waiting remote versions" << std::endl;
    }
}