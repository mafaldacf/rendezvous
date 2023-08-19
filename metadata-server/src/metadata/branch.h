#ifndef BRANCH_H
#define BRANCH_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include "client.grpc.pb.h"
#include "../utils.h"

namespace metadata {

    class Branch {
        
        static const int OPENED = 0;
        static const int CLOSED = 1;
        static const int UNKNOWN = 2;

        private:
            const std::string _service;
            const std::string _tag;

            // region status with key: <region>, value: <status>
            std::unordered_map<std::string, int> _regions;
            int _opened_regions;

        public:
            Branch(std::string service, std::string tag, std::string region);
            Branch(std::string service, std::string tag, const utils::ProtoVec& vector_regions);

            /**
             * Get the branche's tag
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