#include "branch.h"

using namespace metadata;

Branch::Branch(std::string service, std::string tag, std::string region)
    : _service(service), _tag(tag) {
        _regions = std::unordered_map<std::string, int>();
        _regions[region] = OPENED;
    }

Branch::Branch(std::string service, std::string tag, const utils::ProtoVec& vector_regions)
    : _service(service), _tag(tag) {
        _regions = std::unordered_map<std::string, int>();
        for (const auto& region : vector_regions) {
            _regions[region] = OPENED;
        }
    }

std::string Branch::getTag() {
    return _tag;
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

json Branch::toJson(const std::string& bid) const {
    json j;
    j[bid]["service"] = _service;

    for (const auto& regions_it : _regions) {
        j[bid]["regions"].push_back(regions_it.first);
    }
    return j;
}