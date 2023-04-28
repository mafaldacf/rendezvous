#include "branch.h"

using namespace metadata;

Branch::Branch(std::string bid, std::string service, std::string region)
    : _bid(bid), _service(service) {
        _regions = std::unordered_map<std::string, int>();
        _regions[region] = OPENED;
    }

Branch::Branch(std::string bid, std::string service, const utils::ProtoVec& vector_regions)
    : _bid(bid), _service(service) {
        _regions = std::unordered_map<std::string, int>();
        for (const auto& region : vector_regions) {
            _regions[region] = OPENED;
        }
    }

std::string Branch::getBid() {
    return _bid;
}

std::string Branch::getService() {
    return _service;
}

int Branch::close(const std::string &region) {
    auto region_it = _regions.find(region);

    // region not found
    if (region_it == _regions.end()) {
        return -1;
    }

    // already closed
    if (_regions[region] == CLOSED) {
        return 0;
    }

    _regions[region] = CLOSED;
    return 1;
}

json Branch::toJson() const {
    json j;
    j[_bid]["service"] = _service;

    for (const auto& regions_it : _regions) {
        j[_bid]["regions"].push_back(regions_it.first);
    }
    return j;
}