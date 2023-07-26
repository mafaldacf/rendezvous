#include "subscriber.h"

using namespace metadata;

Subscriber::Subscriber(int subscribers_refresh_interval_s) 
    : _subscribers_refresh_interval_s(subscribers_refresh_interval_s) {
        _subscribed_branches = std::queue<SubscribedBranch>();
        _last_ts = std::chrono::system_clock::now();
}

void Subscriber::pushBranch(const std::string& bid, const std::string& tag) {
    _mutex.lock();
    spdlog::debug("adding subscribed branch {}", bid.c_str());
    _subscribed_branches.push(SubscribedBranch{bid, tag});
    _cond.notify_all();
    _mutex.unlock();
}

metadata::Subscriber::SubscribedBranch Subscriber::popBranch(grpc::ServerContext * context) {
    // refresh timestamp of last time moment
    _last_ts = std::chrono::system_clock::now();
    std::unique_lock<std::mutex> lock(_mutex);
    while (_subscribed_branches.size() == 0) {
        _last_ts = std::chrono::system_clock::now();
        _cond.wait_for(lock, std::chrono::seconds(_subscribers_refresh_interval_s));
        if (context->IsCancelled()) {
            return SubscribedBranch{};
        }
    }
    auto subscribedBranch = _subscribed_branches.front();
    _subscribed_branches.pop();
    return subscribedBranch;
}

std::chrono::time_point<std::chrono::system_clock> Subscriber::getLastTs() {
    return _last_ts;
}