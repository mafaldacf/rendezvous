#include "subscriber.h"

using namespace metadata;

Subscriber::Subscriber(int subscribers_max_wait_time_s) 
    : _subscribers_max_wait_time_s(subscribers_max_wait_time_s) {
        _subscribed_branches = std::queue<std::string>();
        _last_ts = std::chrono::system_clock::now();
}

void Subscriber::pushBranch(const std::string& bid) {
    _mutex.lock();
    //spdlog::debug("adding subscribed branch {}", bid.c_str());
    _subscribed_branches.push(bid);
    _cond.notify_all();
    _mutex.unlock();
}

std::string Subscriber::popBranch() {
    // refresh timestamp of last time moment
    _last_ts = std::chrono::system_clock::now();

    std::cv_status status = std::cv_status::no_timeout;
    std::unique_lock<std::mutex> lock(_mutex);
    while (_subscribed_branches.size() == 0) {
        //spdlog::debug("waiting for subscribed branches...");
        status = _cond.wait_for(lock, std::chrono::seconds(_subscribers_max_wait_time_s));
    }

    if (status == std::cv_status::no_timeout) {
        std::string bid = _subscribed_branches.front();
        _subscribed_branches.pop();
        //spdlog::debug("getting subscribed request {}", bid.c_str());
        
        return bid;
    }

    return "";
}

std::chrono::time_point<std::chrono::system_clock> Subscriber::getLastTs() {
    return _last_ts;
}