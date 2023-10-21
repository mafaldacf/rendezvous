#include "branch.h"

using namespace metadata;

Branch::Branch(std::string service, std::string tag, std::string async_zone_id, const utils::ProtoVec& vector_regions, bool replicated)
    : _service(service), _tag(tag), _sub_rid(async_zone_id), _num_opened_regions(vector_regions.size()), replicated(replicated) {
        _regions = std::unordered_map<std::string, int>();
        for (const auto& region : vector_regions) {
            _regions[region] = OPENED;
        }
    }

Branch::Branch(std::string service, std::string tag, std::string async_zone_id, bool replicated)
    : _service(service), _tag(tag), _sub_rid(async_zone_id), _num_opened_regions(1), replicated(replicated) {
        _regions = std::unordered_map<std::string, int>();

        _regions[GLOBAL_REGION] = OPENED;
    }

std::string Branch::getAsyncZoneId() {
    return _sub_rid;
}

std::string Branch::getTag() {
    return _tag;
}

bool Branch::hasTag() {
    return !_tag.empty();
}

std::string Branch::getService() {
    return _service;
}

bool Branch::isGloballyClosed(std::string region) {
    if (region.empty()) {
        return _num_opened_regions.load() == 0;
    }
    std::unique_lock<std::mutex> lock(_mutex_regions);
    return _regions[region] == CLOSED;
}

int Branch::getStatus(std::string region) {
    if (region.empty()) {
        if (_num_opened_regions.load() == 0) {
            return CLOSED;
        }
        return OPENED;
    }
    std::unique_lock<std::mutex> lock(_mutex_regions);
    auto region_it = _regions.find(region);
    if (region_it == _regions.end()) {
        return UNKNOWN;
    }
    return _regions[region];
}

int Branch::close(const std::string &region) {
    std::unique_lock<std::mutex> lock(_mutex_regions);
    auto region_it = _regions.find(region);
    if (region_it == _regions.end()) {
        return -1;
    }
    if (_regions[region] == CLOSED) {
        return 0;
    }
    _regions[region] = CLOSED;
    _num_opened_regions.fetch_add(-1);
    return 1;
}

void Branch::open(const std::string &region) {
    std::unique_lock<std::mutex> lock(_mutex_regions);
    _regions[region] = OPENED;
    _num_opened_regions.fetch_add(1);
}