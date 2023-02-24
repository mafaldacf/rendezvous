#include "branch.h"

using namespace branch;

Branch::Branch(long bid, std::string service, std::string region)
    : bid(bid), service(service), region(region) {
        
    }

long Branch::getBid() {
    return bid;
}

std::string Branch::getService() {
    return service;
}

std::string Branch::getRegion() {
    return region;
}