#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>
#include "utils.h"

// --------------
// WAIT LOGS TEST

/* --------------------------------------------
NOTICE (1): here we test the for async zones and their wait calls with the following call graph:
                r:a1:b1
              /
          r:a1
        /     \
      /        r:a1:b2
    /
  r 
    \
      r:c1
          \
            r:c1:d1

Wait calls on:
1. (r:a1:b2) need to detect (r:a1:b1) as preceding
2. (r:a1:b2) need to detect (r:a1) as preceding
3. (r:a1) cannot detect (r:a1:b1) as preceding
3. (r:c1) need to detect (r:a1) as preceding
4. (r:a1) cannot detect (r:c1) nor (r:c1-d1) as preceding
5. (r:c1-d1) need to detect (r:a1:b1) as preceding EVEN IF IT WAS REGISTERED BEFORE!!!! 
----------------------------------------------- */
TEST(WaitLogsTest, PrecedingDetection) { 
  std::vector<std::thread> threads;
  rendezvous::Server server("a");
  bool is_preceding;

  utils::ProtoVec regions_empty;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  // register all async zones
  std::string r_a1 = server.addNextSubRequest(request, "r:a1", false);
  ASSERT_EQ("r:a1", r_a1);
  std::string r_c1 = server.addNextSubRequest(request, "r:c1", false);
  ASSERT_EQ("r:c1", r_c1);
  std::string r_c1_d1 = server.addNextSubRequest(request, "r:c1:d1", false);
  ASSERT_EQ("r:c1:d1", r_c1_d1);
  std::string r_a1_b1 = server.addNextSubRequest(request, "r:a1:b1", false);
  ASSERT_EQ("r:a1:b1", r_a1_b1);
  std::string r_a1_b2 = server.addNextSubRequest(request, "r:a1:b2", false);
  ASSERT_EQ("r:a1:b2", r_a1_b2);

  // get root async zone
  metadata::Request::AsyncZone * subrequest_r = request->_validateSubRid(ROOT_SUB_RID);
  ASSERT_TRUE(subrequest_r != nullptr);

  // add to wait logs: async zone r:a1
  metadata::Request::AsyncZone * subrequest_r_a1 = request->_validateSubRid(r_a1);
  ASSERT_TRUE(subrequest_r_a1 != nullptr);
  request->_addToWaitLogs(subrequest_r_a1);

  // add to wait logs: async zone r:c1
  metadata::Request::AsyncZone * subrequest_r_c1 = request->_validateSubRid(r_c1);
  ASSERT_TRUE(subrequest_r_c1 != nullptr);
  request->_addToWaitLogs(subrequest_r_c1);

  // add to wait logs: async zone r:c1:d1
  metadata::Request::AsyncZone * subrequest_r_c1_d1 = request->_validateSubRid(r_c1_d1);
  ASSERT_TRUE(subrequest_r_c1_d1 != nullptr);
  request->_addToWaitLogs(subrequest_r_c1_d1);

  // add to wait logs: async zone r:a1:b1
  metadata::Request::AsyncZone * subrequest_r_a1_b1 = request->_validateSubRid(r_a1_b1);
  ASSERT_TRUE(subrequest_r_a1_b1 != nullptr);
  request->_addToWaitLogs(subrequest_r_a1_b1);

  // add to wait logs: async zone r:a1:b2
  metadata::Request::AsyncZone * subrequest_r_a1_b2 = request->_validateSubRid(r_a1_b2);
  ASSERT_TRUE(subrequest_r_a1_b2 != nullptr);
  request->_addToWaitLogs(subrequest_r_a1_b2);

  // r:a1 is preceding of r:a1:b1 since it is its parent
  is_preceding = request->_isPrecedingAsyncZone(subrequest_r_a1, subrequest_r_a1_b1);
  ASSERT_TRUE(is_preceding);

  // now we test the opposite
  is_preceding = request->_isPrecedingAsyncZone(subrequest_r_a1_b2, subrequest_r_a1);
  ASSERT_FALSE(is_preceding);

  // test preceeding between neighboors r:a1:b1 and r:a1:b2
  is_preceding = request->_isPrecedingAsyncZone(subrequest_r_a1_b1, subrequest_r_a1_b2);
  ASSERT_TRUE(is_preceding);

  // r:c1 is always after r:a1:b1 and r:a1:b2 even if it was registered after
  is_preceding = request->_isPrecedingAsyncZone(subrequest_r_a1_b1, subrequest_r_c1);
  ASSERT_TRUE(is_preceding);
  is_preceding = request->_isPrecedingAsyncZone(subrequest_r_a1_b2, subrequest_r_c1);
  ASSERT_TRUE(is_preceding);

  // r is always preceding of any other
  is_preceding = request->_isPrecedingAsyncZone(subrequest_r, subrequest_r_c1);
  ASSERT_TRUE(is_preceding);

}
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
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "compose-post", regions_empty, "", "");
  ASSERT_EQ(getBid(0), bid_0);

  // register post-storage async branch from compose-post
  std::string next_sub_rid = server.addNextSubRequest(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_0, next_sub_rid);
  std::string bid_1 = server.registerBranchGTest(request, SUB_RID_0, "post-storage", regions_empty, "", "");
  ASSERT_EQ(getBid(1), bid_1);

  // register post-storage (NOT ASYNC) branch for write post operation
  utils::ProtoVec regions_post_storage;
  regions_post_storage.Add("EU");
  regions_post_storage.Add("US");
  std::string bid_2 = server.registerBranchGTest(request, SUB_RID_0, "post-storage", regions_post_storage, "", "");
  ASSERT_EQ(getBid(2), bid_2);

  // register notifier async branch from compose-post
  next_sub_rid = server.addNextSubRequest(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_1, next_sub_rid);
  std::string bid_3 = server.registerBranchGTest(request, SUB_RID_1, "notifier", regions_empty, "", "");
  ASSERT_EQ(getBid(3), bid_3);

  // register post-storage (NOT ASYNC) branch for write post operation
  utils::ProtoVec regions_notifier;
  regions_notifier.Add("US");
  std::string bid_4 = server.registerBranchGTest(request, SUB_RID_1, "notifier", regions_notifier, "", "");
  ASSERT_EQ(getBid(4), bid_4);

  // do wait call on notifier
  // this is an edge case where the it is notified again 
  // and then disregards the post-storage wait call
  sleep(0.2);
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_1, "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  // do wait call on post-storage
  sleep(0.5);
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_0, "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  // do wait call on notifier again
  // but this time, it detects the cycle before performing the core wait logic
  // so it cannot prevent any inconsistency from the post-storage, as expected :)
  // but compose-post is still opened!!
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_1, "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  sleep(1);

  // close all branches for compose-post
  found_region = server.closeBranch(request, getBid(0), "");
  ASSERT_EQ(1, found_region);

  sleep(0.5);

  // now that compose-post is closed and we are able to detect wait cycle
  // this wait does not block
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_1, "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);
  });

  sleep(0.5);

  // close all branches for post-storage
  found_region = server.closeBranch(request, getBid(1), "");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(2), "EU");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(2), "US");
  ASSERT_EQ(1, found_region);

  // close all branches for notifier
  found_region = server.closeBranch(request, getBid(3), "");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(4), "US");
  ASSERT_EQ(1, found_region);

  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}