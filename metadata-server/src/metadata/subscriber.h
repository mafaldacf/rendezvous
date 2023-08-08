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
#include "spdlog/fmt/ostr.h"

namespace metadata {

    class Subscriber {

        typedef struct SubscribedBranchStruct {
            std::string bid;
            std::string tag;
        } SubscribedBranch;

        private:
            std::queue<SubscribedBranch> _subscribed_branches;
            std::mutex _mutex;
            std::condition_variable _cond;
            std::chrono::time_point<std::chrono::system_clock> _last_ts;

            const int _subscribers_refresh_interval_s;

        public:
            Subscriber(int subscribers_refresh_interval_s);

            /**
             * Add branch to queue
             * 
             * @param branch The branch's bid
             * @param tag
             */
            void push(const std::string& bid, const std::string& tag);

            /**
             * Remove the branch from the queue. Blocks until a branch is available
             * @param context The grpc context for the current connection
             * @return The bid for the removed branch
             */
            SubscribedBranch pop(grpc::ServerContext * context);

            /**
             * Return timestamp of last active moment
             */
            std::chrono::time_point<std::chrono::system_clock> getLastTs();
    };
}

#endif