#ifndef BRANCH_H
#define BRANCH_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include "client.grpc.pb.h"
#include "../utils.h"
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "spdlog/fmt/ostr.h"

using json = nlohmann::json;

namespace metadata {

    class Branch {
        
        static const int OPENED = 0;
        static const int CLOSED = 1;

        private:
            const std::string _service;
            const std::string _tag;

            // <region, status>
            std::unordered_map<std::string, int> _regions;

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
             * Get service of the object
             * 
             * @return service 
             */
            std::string getService();

            /**
             * Set status to close for a given region
             * 
             * @param region The region context
             * 
             * @returns -1 if region does not exist, 1 if if was successfully closed and 0 if it was already closed
             */
            int close(const std::string &region);
            
            /**
             * Stores branch info in json format
             * 
             * @param bid
             * 
             * @return json 
             */
            json toJson(const std::string& bid) const;
        };
    
}

#endif