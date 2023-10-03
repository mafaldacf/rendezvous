#ifndef BRANCH_H
#define BRANCH_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include "client.grpc.pb.h"
#include "../utils/grpc_service.h"
#include "../utils/metadata.h"

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

            int _num_opened_regions;




        public:
            Branch(std::string service, std::string tag, std::string sub_rid, const utils::ProtoVec& vector_regions);
            Branch(std::string service, std::string tag, std::string sub_rid);

            /**
             * Get the branch's sub_rid
             * 
             * @return sub_rid
             */
            std::string getSubRid();

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
            bool isClosed(std::string region = "");

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
        };
    
}

#endif