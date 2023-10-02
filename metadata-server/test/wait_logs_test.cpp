#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>
#include "utils.h"

// --------------
// WAIT LOGS TEST
// --------------


/* --------------------------------------------
NOTICE (1): here we force a HIDDEN EDGE CASE e.g.
  1. post-storage registers async branch
  2. notifier registers async branch
  3. notifier does wait call (implicitly waits for post-storage)
  4. post-storage does wait -- but is waiting on notifier, that doesn't now post-storage is waiting
  SOLUTION:
  > We have to signal the notifier service that we have a new entry on wait log!!
----------------------------------------------- */
TEST(WaitLogsTest, SyncComposeAsyncPostAsyncNotifierDoubleWait) { 
  std::vector<std::thread> threads;
  rendezvous::Server server(SID);
  std::string bid;
  int status;
  bool found_region;

  utils::ProtoVec regions_empty;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  // register compose-post branch
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "compose-post", regions_empty, "", "");
  ASSERT_EQ(getBid(0), bid_0);

  // register post-storage async branch from compose-post
  std::string next_sub_rid = server.addNextSubRequest(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_0, next_sub_rid);
  std::string bid_1 = server.registerBranch(request, SUB_RID_0, "post-storage", regions_empty, "", "");
  ASSERT_EQ(getBid(1), bid_1);

  // register post-storage (NOT ASYNC) branch for write post operation
  utils::ProtoVec regions_post_storage;
  regions_post_storage.Add("EU");
  regions_post_storage.Add("US");
  std::string bid_2 = server.registerBranch(request, SUB_RID_0, "post-storage", regions_post_storage, "", "");
  ASSERT_EQ(getBid(2), bid_2);

  // register notifier async branch from compose-post
  next_sub_rid = server.addNextSubRequest(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_1, next_sub_rid);
  std::string bid_3 = server.registerBranch(request, SUB_RID_1, "notifier", regions_empty, "", "");
  ASSERT_EQ(getBid(3), bid_3);

  // register post-storage (NOT ASYNC) branch for write post operation
  utils::ProtoVec regions_notifier;
  regions_notifier.Add("US");
  std::string bid_4 = server.registerBranch(request, SUB_RID_1, "notifier", regions_notifier, "", "");
  ASSERT_EQ(getBid(4), bid_4);

  // do wait call on notifier
  sleep(0.2);
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_1, "", "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  // do wait call on post-storage
  sleep(0.5);
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_0, "", "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  // do wait call on notifier again
  // but this time, it detects the cycle before performing the core wait logic
  // so it cannot prevent any inconsistency from the post-storage, as expected :)
  // but compose-post is still opened!!
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_1, "", "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  sleep(1);

  // close all branches for compose-post
  found_region = server.closeBranch(request, ROOT_SUB_RID, getBid(0), "");
  ASSERT_EQ(1, found_region);

  sleep(0.5);

  // now that compose-post is closed and we are able to detect wait cycle
  // this wait does not block
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_1, "", "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);
  });

  sleep(0.2);

  // close all branches for post-storage
  found_region = server.closeBranch(request, SUB_RID_0, getBid(1), "");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, SUB_RID_0, getBid(2), "EU");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, SUB_RID_0, getBid(2), "US");
  ASSERT_EQ(1, found_region);

  // close all branches for notifier
  found_region = server.closeBranch(request, SUB_RID_1, getBid(3), "");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, SUB_RID_1, getBid(4), "US");
  ASSERT_EQ(1, found_region);

  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}