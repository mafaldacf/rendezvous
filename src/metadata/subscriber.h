#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include "branch.h"
#include "../utils.h"
#include <chrono>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "spdlog/fmt/ostr.h"

namespace metadata {

    class Subscriber {

        private:
            std::queue<std::string> _subscribed_branches;
            std::mutex _mutex;
            std::condition_variable _cond;
            std::chrono::time_point<std::chrono::system_clock> _last_ts;

            const int _subscribers_max_wait_time_s;

        public:
            Subscriber(int subscribers_max_wait_time_s);

            /**
             * Add branch to queue
             * 
             * @param branch The branch's bid
             */
            void pushBranch(const std::string& bid);

            /**
             * Remove the branch from the queue. Blocks until a branch is available
             * 
             * @return The bid for the removed branch
             */
            std::string popBranch();

            /**
             * Return timestamp of last active moment
             */
            std::chrono::time_point<std::chrono::system_clock> getLastTs();
    };
}

#endif