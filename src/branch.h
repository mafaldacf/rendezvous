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
        Branch(long, std::string, std::string);

        long getBid();
        std::string getRegion();
        std::string getService();
    };
    
}

#endif