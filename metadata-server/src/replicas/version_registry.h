#ifndef VERSION_REGISTRY_H
#define VERSION_REGISTRY_H

#include <mutex>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <condition_variable>
#include "client.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include "../utils/grpc_service.h"
#include "../utils/metadata.h"
#include "../utils/settings.h"
#include <chrono>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace replicas {

    class VersionRegistry {

        private:
            const int _wait_replica_timeout_s;

            // hash map: <server id, version>
            std::unordered_map<std::string, int> _versions;

            // concurrency control
            std::mutex _mutex_versions;
            std::condition_variable _cond_versions;

        public:
            VersionRegistry(int wait_replica_timeout_s);

            /**
             * Increment version for current server id
             * 
             * @param sid The local replica id
             * 
             * @return The new version after the update
             * 
             */
            int updateLocalVersion(const std::string& sid);

            /**
             * Get version for current server id
             * 
             * @param sid The local replica id
             * 
             * @return The current version
             * 
             */
            int getLocalVersion(const std::string& sid);

            /**
             * Update version of a remote replica
             * 
             * @param sid The remote replica id
             * @param version The remote replica version
             * 
             */
            void waitUpdateRemoteVersion(const std::string& sid, int version);

            /**
             * Update version of a remote replica
             * 
             * @param sid The remote replica id
             * @param version The remote replica version
             * 
             */
            void updateRemoteVersion(const std::string& sid, int version);

            /**
             * Wait until the version of the remote replica is applied
             * 
             * @param sid The remote replica id
             * @param version The remote replica version
             * 
             */
            void waitRemoteVersion(const std::string& sid, int version);
        };
    
}

#endif