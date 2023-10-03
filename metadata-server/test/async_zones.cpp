#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>
#include "utils.h"

// ----------------
// ASYNC ZONES TEST
// ----------------

TEST(AsyncZonesTest, RidParsing) { 
  rendezvous::Server server(SID);
  std::string composed_id;
  std::pair<std::string, std::string> parsed_id;

  composed_id = server.composeFullId("1st", "2nd");
  ASSERT_EQ(composed_id, "1st:2nd");

  composed_id = server.composeFullId("", "2nd");
  ASSERT_EQ(composed_id, ":2nd");

  composed_id = server.composeFullId("1st", "");
  ASSERT_EQ(composed_id, "1st");

  composed_id = server.composeFullId("", "");
  ASSERT_EQ(composed_id, "");

  parsed_id = server.parseFullId("1st:2nd");
  ASSERT_EQ(parsed_id.first, "1st");
  ASSERT_EQ(parsed_id.second, "2nd");

  parsed_id = server.parseFullId("1st:");
  ASSERT_EQ(parsed_id.first, "1st");
  ASSERT_EQ(parsed_id.second, "");

  parsed_id = server.parseFullId(":2nd");
  ASSERT_EQ(parsed_id.first, "");
  ASSERT_EQ(parsed_id.second, "2nd");

  parsed_id = server.parseFullId(":");
  ASSERT_EQ(parsed_id.first, "");
  ASSERT_EQ(parsed_id.second, "");

  parsed_id = server.parseFullId("original_rid");
  ASSERT_EQ(parsed_id.first, "original_rid");
  ASSERT_EQ(parsed_id.second, "r");
}

TEST(AsyncZonesTest, RegisterWrongBranchId) { 
  rendezvous::Server server(SID);
  int closed;

  utils::ProtoVec regions;
  regions.Add("EU");

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  // register branch
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "post-storage", regions, "", "");
  ASSERT_EQ(getBid(0), bid_0);

  // attempt to close but bid does not exist
  closed = server.closeBranch(request, getBid(1), "EU");
  ASSERT_EQ(-1, closed);

  // attempt to close but region does not exist
  closed = server.closeBranch(request, getBid(0), "wrong-region");
  ASSERT_EQ(-1, closed);

  // close branch
  closed = server.closeBranch(request, getBid(0), "EU");
  ASSERT_EQ(1, closed);

  // attempt to close branch again
  closed = server.closeBranch(request, getBid(0), "EU");
  ASSERT_EQ(0, closed);
}

TEST(AsyncZonesTest, AddNextSubRids) { 
  rendezvous::Server server(SID);

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  std::string sub_rid_0 = server.addNextSubRequest(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_0, sub_rid_0);
  std::string sub_rid_1 = server.addNextSubRequest(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_1, sub_rid_1);
  std::string sub_rid_2 = server.addNextSubRequest(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_2, sub_rid_2);
  // -----------------
  std::string sub_rid_0_0 = server.addNextSubRequest(request, SUB_RID_0);
  ASSERT_EQ(SUB_RID_0_0, sub_rid_0_0);
  std::string sub_rid_0_1 = server.addNextSubRequest(request, SUB_RID_0);
  ASSERT_EQ(SUB_RID_0_1, sub_rid_0_1);
  // -----------------
  std::string sub_rid_0_0_0 = server.addNextSubRequest(request, SUB_RID_0_0);
  ASSERT_EQ(SUB_RID_0_0_0, sub_rid_0_0_0);
}


TEST(AsyncZonesTest, PostAnalyticsNotificationTotalWaitIgnoreCompose) { 
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
  std::string sub_rid_0 = server.addNextSubRequest(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_0, sub_rid_0);
  std::string bid_1 = server.registerBranch(request, SUB_RID_0, "post-storage", regions_empty, "", "");
  ASSERT_EQ(getBid(1), bid_1);

  // register post-storage async branch for write post operation
  std::string sub_rid_0_0 = server.addNextSubRequest(request, SUB_RID_0);
  ASSERT_EQ(SUB_RID_0_0, sub_rid_0_0);
  utils::ProtoVec regions_post_storage;
  regions_post_storage.Add("EU");
  regions_post_storage.Add("US");
  std::string bid_2 = server.registerBranch(request, SUB_RID_0_0, "post-storage", regions_post_storage, "", "");
  ASSERT_EQ(getBid(2), bid_2);

  // register notifier async branch from compose-post
  std::string sub_rid_1 = server.addNextSubRequest(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_1, sub_rid_1);
  std::string bid_3 = server.registerBranch(request, SUB_RID_1, "notifier", regions_empty, "", "");
  ASSERT_EQ(getBid(3), bid_3);

  // register post-storage async branch for write post operation
  std::string sub_rid_1_0 = server.addNextSubRequest(request, SUB_RID_1);
  ASSERT_EQ(SUB_RID_1_0, sub_rid_1_0);
  utils::ProtoVec regions_notifier;
  regions_notifier.Add("US");
  std::string bid_4 = server.registerBranch(request, SUB_RID_1, "notifier", regions_notifier, "", "");
  ASSERT_EQ(getBid(4), bid_4);

  // do wait call on notifier
  // it will ignore every branch from notifier
  // so we don't even need to close the notifier branches
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_1, "", "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  sleep(0.5);

  // close all branches for compose-post
  found_region = server.closeBranch(request, getBid(0), "");
  ASSERT_EQ(1, found_region);

  // close all branches for post-storage
  found_region = server.closeBranch(request, getBid(1), "");
  ASSERT_EQ(1, found_region);

  // at this point, no global branch is opened so 
  // we attempt to wait on a non existent region
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_1, "", "INVALID REGION", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);
  });
  sleep(0.5);

  // close all branches for write-post operation in post-storage
  found_region = server.closeBranch(request, getBid(2), "EU");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(2), "US");
  ASSERT_EQ(1, found_region);

  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}