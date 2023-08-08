#include "branch.h"

using namespace metadata;

Branch::Branch(std::string service, std::string tag, std::string region)
    : _service(service), _tag(tag) {
        _regions = std::unordered_map<std::string, int>();
        _regions[region] = OPENED;
        _opened_regions = 1;
    }

Branch::Branch(std::string service, std::string tag, const utils::ProtoVec& vector_regions)
    : _service(service), _tag(tag) {
        _regions = std::unordered_map<std::string, int>();
        for (const auto& region : vector_regions) {
            _regions[region] = OPENED;
        }
        _opened_regions = vector_regions.size();
    }

std::string Branch::getTag() {
    return _tag;
}

std::string Branch::getService() {
    return _service;
}

bool Branch::isClosed(std::string region) {
    if (region.empty()) {
        return _opened_regions == 0;
    }
    return _regions[region] == CLOSED;
}

int Branch::close(const std::string &region) {
    auto region_it = _regions.find(region);
    if (region_it == _regions.end()) {
        return -1;
    }
    if (_regions[region] == CLOSED) {
        return 0;
    }
    _regions[region] = CLOSED;
    _opened_regions -= 1;
    return 1;
}