#ifndef BRANCH_H
#define BRANCH_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include "rendezvous.grpc.pb.h"
#include "../utils.h"

using json = nlohmann::json;

namespace metadata {

    class Branch {
        
        static const int OPENED = 0;
        static const int CLOSED = 1;

        private:
            const std::string _bid;
            const std::string _service;

            // <region, status>
            std::unordered_map<std::string, int> _regions;

        public:
            Branch(std::string bid, std::string service, std::string region);
            Branch(std::string bid, std::string service, const utils::ProtoVec& vector_regions);

            /**
             * Get identifier (bid) of the object
             * 
             * @return bid
             */
            std::string getBid();

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
             * @return json 
             */
            json toJson() const;
        };
    
}

#endif