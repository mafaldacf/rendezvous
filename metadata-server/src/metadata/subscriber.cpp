#include "subscriber.h"

using namespace metadata;

Subscriber::Subscriber(int subscribers_refresh_interval_s) 
    : _subscribers_refresh_interval_s(subscribers_refresh_interval_s) {
        _subscribed_branches = std::queue<std::string>();
        _last_ts = std::chrono::system_clock::now();
}

void Subscriber::pushBranch(const std::string& bid) {
    _mutex.lock();
    spdlog::debug("adding subscribed branch {}", bid.c_str());
    _subscribed_branches.push(bid);
    _cond.notify_all();
    _mutex.unlock();
}

std::string Subscriber::popBranch(grpc::ServerContext * context) {
    // refresh timestamp of last time moment
    _last_ts = std::chrono::system_clock::now();
    std::unique_lock<std::mutex> lock(_mutex);
    while (_subscribed_branches.size() == 0) {
        _last_ts = std::chrono::system_clock::now();
        _cond.wait_for(lock, std::chrono::seconds(_subscribers_refresh_interval_s));
        if (context->IsCancelled()) {
            return "";
        }
    }
    std::string bid = _subscribed_branches.front();
    _subscribed_branches.pop();
    return bid;
}

std::chrono::time_point<std::chrono::system_clock> Subscriber::getLastTs() {
    return _last_ts;
}