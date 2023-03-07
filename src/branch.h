#ifndef BRANCH_H
#define BRANCH_H

#include <iostream>
#include <unordered_map>

namespace metadata {

    class Branch {
        
        const int OPENED = 0;
        const int CLOSED = 1;

        private:
            // format: <server id>:<id of the branches' set>
            std::string bid;
            std::string service;
            std::string region;
            int status;

        public:
            Branch(std::string bid, std::string service, std::string region);

            /**
             * Get identifier (bid) of the object
             * 
             * @return bid
             */
            std::string getBid();

            /**
             * Get region of the object
             * 
             * @return region 
             */
            std::string getRegion();

            /**
             * Get service of the object
             * 
             * @return service 
             */
            std::string getService();

            /**
             * Set status to close
             */
            void close();

            /**
             * Set status to close
             * 
             * @return true if branch is closed and false otherwise
             */
            bool isClosed();
        };
    
}

#endif