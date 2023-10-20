#ifndef BRANCH_H
#define BRANCH_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include "client.grpc.pb.h"
#include "../utils/grpc_service.h"
#include "../utils/metadata.h"
#include "../utils/settings.h"
#include "mutex"
#include "atomic"

using namespace utils;

namespace metadata {

    class Branch {
        const std::string GLOBAL_REGION = "";

        private:
            const std::string _service;
            const std::string _tag;
            const std::string _sub_rid;

            // region status with key: <region>, value: <status>
            std::unordered_map<std::string, int> _regions;

            std::atomic<int> _num_opened_regions;
            std::mutex _mutex_regions;




        public:
            std::atomic<bool> replicated;
            Branch(std::string service, std::string tag, std::string async_zone_id, const utils::ProtoVec& vector_regions);
            Branch(std::string service, std::string tag, std::string async_zone_id);

            /**
             * Get the branch's async_zone_id
             * 
             * @return async_zone_id
             */
            std::string getAsyncZoneId();

            /**
             * Get the branch's tag
             * 
             * @return bid
             */
            std::string getTag();

            /**
             * Return whether a tag was assigned to this branch
             * 
             * @return true if tag exists and false otherwise
             */
            bool hasTag();

            /**
             * Get service of the object
             * 
             * @return service 
             */
            std::string getService();

            /**
             * Check if branch is closed for a given region or globally
             * 
             * @param region if empty, status is checked globally
            */
            bool isGloballyClosed(std::string region = "");

            /**
             * Return branch status for a given region or globally
             * 
             * @param region if empty, status is checked globally
            */
            int getStatus(std::string region = "");

            /**
             * Set status to close for a given region
             * 
             * @param region The region context
             * 
             * @returns -1 if region does not exist, 1 if if was successfully closed and 0 if it was already closed
             */
            int close(const std::string &region);

            /**
             * Set status to open for a given region: helper to re-open in case of error when closing
             * 
             * @param region The region context
             */
            void open(const std::string &region);
        };
    
}

#endif