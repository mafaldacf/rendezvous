#ifndef VERSION_REGISTRY_H
#define VERSION_REGISTRY_H

#include <mutex>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <condition_variable>
#include "rendezvous.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include "../utils.h"

namespace replicas {

    class VersionRegistry {

        private:
            // hash map: <server id, version>
            std::unordered_map<std::string, int> versions;

            // concurrency control
            std::mutex mutex_versions;
            std::condition_variable cond_versions;

        public:
            VersionRegistry();

            /**
             * Increment version for current server id
             * 
             * @param id The local replica id
             * 
             * @return The new version after the update
             * 
             */
            int updateLocalVersion(const std::string& id);

            /**
             * Update version of a remote replica
             * 
             * @param id The remote replica id
             * @param version The remote replica version
             * 
             */
            void updateRemoteVersion(const std::string& id, const int& version);

            /**
             * Wait until all remote versions is available
             * 
             * @param info Map with all versions for every replica id: <replica id, version>
             */
            void waitRemoteVersions(const rendezvous::RequestContext& info);
        };
    
}

#endif