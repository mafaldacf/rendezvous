#include "branch.h"

using namespace metadata;

Branch::Branch(std::string bid, std::string service, std::string region)
    : bid(bid), service(service), region(region), status(OPENED) {
        
    }

std::string Branch::getBid() {
    return bid;
}

std::string Branch::getService() {
    return service;
}

std::string Branch::getRegion() {
    return region;
}

void Branch::close() {
    status = CLOSED;
}

bool Branch::isClosed() {
    return status == CLOSED;
}