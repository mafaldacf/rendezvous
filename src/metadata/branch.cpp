#include "branch.h"

using namespace metadata;

Branch::Branch(std::string bid, std::string service, std::string region)
    : bid(bid), service(service) {
        regions = std::unordered_map<std::string, int>();
        regions[region] = OPENED;
    }

Branch::Branch(std::string bid, std::string service, const utils::ProtoVec& regionsVec)
    : bid(bid), service(service) {
        regions = std::unordered_map<std::string, int>();
        for (const auto& region : regionsVec) {
            regions[region] = OPENED;
        }
    }

std::string Branch::getBid() {
    return bid;
}

std::string Branch::getService() {
    return service;
}

bool Branch::close(const std::string &region) {
    auto region_it = regions.find(region);

    // region not found
    if (region_it == regions.end()) {
        return false;
    }

    regions[region] = CLOSED;
    return true;
}

json Branch::toJson() const {
    json j;
    j[bid]["service"] = service;

    for (const auto& regions_it : regions) {
        j[bid]["regions"].push_back(regions_it.first);
    }
    return j;
}