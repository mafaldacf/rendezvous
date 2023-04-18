#ifndef BRANCH_H
#define BRANCH_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include "rendezvous.grpc.pb.h"
#include "../utils.h"

namespace metadata {

    class Branch {
        
        const int OPENED = 0;
        const int CLOSED = 1;

        private:
            const std::string bid;
            const std::string service;

            // <region, status>
            std::unordered_map<std::string, int> regions;

        public:
            Branch(std::string bid, std::string service, std::string region);
            Branch(std::string bid, std::string service, const utils::ProtoVec& regionsVec);

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
             */
            bool close(const std::string &region);
        };
    
}

#endif