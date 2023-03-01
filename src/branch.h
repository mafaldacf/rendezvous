#ifndef BRANCH_H
#define BRANCH_H

#include <iostream>
#include <unordered_map>

namespace branch {

    class Branch {

    private:
        long bid;
        std::string service;
        std::string region;

    public:
        Branch(long bid, std::string service, std::string region);

        /**
         * Get identifier (bid) of the object
         * 
         * @return bid
         */
        long getBid();

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
    };
    
}

#endif