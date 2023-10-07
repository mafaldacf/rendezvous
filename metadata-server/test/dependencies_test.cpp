#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>
#include "utils.h"

// ---------------------------------------------
// DEPENDENCIES TEST w/ POST NOTIFICATION sample
// ---------------------------------------------

TEST(DependenciesTest, CheckStatus) {
  //          root
  //      /           \
  //  post_storage   notification_storage
  //    /               \
  //  analytics       media service

  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  regions_0.Add("EU");
  regions_0.Add("US");
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid_0);

  utils::ProtoVec regions_1;
  regions_1.Add("AP");
  std::string bid_1 = server.registerBranchGTest(request, ROOT_SUB_RID, "analytics", regions_1, "", "post_storage");
  ASSERT_EQ(getBid(1), bid_1);
  
  // ignore parents (analytics, post_storage)
  utils::Status r = server.checkStatus(request, ROOT_SUB_RID, "", "", "analytics");
  ASSERT_EQ(CLOSED, r.status);

  utils::ProtoVec regions_2;
  regions_2.Add("US");
  std::string bid_2 = server.registerBranchGTest(request, ROOT_SUB_RID, "notification_storage", regions_2, "", "post_storage");
  ASSERT_EQ(getBid(2), bid_2);

  utils::ProtoVec regions_3;
  regions_3.Add("US");
  std::string bid_3 = server.registerBranchGTest(request, ROOT_SUB_RID, "media_service", regions_3, "", "notification_storage");
  ASSERT_EQ(getFullBid(request->getRid(), 3), bid_3);
  
  // from the notification storage point of view
  // we are checking the status of post-storage -> analytics
  r = server.checkStatus(request, ROOT_SUB_RID, "", "", "notification_storage");
  ASSERT_EQ(OPENED, r.status);

  int found = server.closeBranch(request, bid_0, "EU");
  ASSERT_EQ(1, found);
  found = server.closeBranch(request, bid_0, "US");
  ASSERT_EQ(1, found);

  // analytics is still opened
  r = server.checkStatus(request, ROOT_SUB_RID, "", "", "notification_storage");
  ASSERT_EQ(OPENED, r.status);

  // OPENED since:
  // - media is still opened
  found = server.closeBranch(request, bid_1, "AP"); // post_storage
  ASSERT_EQ(1, found);
  r = server.checkStatus(request, ROOT_SUB_RID, "", "", "notification_storage");
  ASSERT_EQ(OPENED, r.status);

  // CLOSED since we ignore:
  // - notification storage (OPENED) (direct parent)
  // - media service (OPENED) (the current one)
  r = server.checkStatus(request, ROOT_SUB_RID, "", "", "media_service");
  ASSERT_EQ(CLOSED, r.status);

  // OPENED since:
  // - media storage is OPENED
  found = server.closeBranch(request, bid_2, "US"); // notification_storage
  ASSERT_EQ(1, found);
  r = server.checkStatus(request, ROOT_SUB_RID, "", "", "notification_storage");
  ASSERT_EQ(OPENED, r.status);

  // everything closed
  found = server.closeBranch(request, bid_3, "US"); // media_service
  ASSERT_EQ(1, found);
  r = server.checkStatus(request, ROOT_SUB_RID, "", "", "notification_storage");
  ASSERT_EQ(CLOSED, r.status);
}

TEST(DependenciesTest, FetchDependencies_Root) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  std::string bid;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid);

  utils::ProtoVec regions_1;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "notification_storage", regions_1, "", "");
  ASSERT_EQ(getBid(0), bid);

  // fetch dependencies from root
  auto result = server.fetchDependencies(request, "", "");

  ASSERT_EQ(OK, result.res);
  ASSERT_EQ(2, result.deps.size());
  auto found = std::find(std::begin(result.deps), std::end(result.deps), "post_storage");
  ASSERT_EQ(true, found != std::end(result.deps));
  found = std::find(std::begin(result.deps), std::end(result.deps), "notification_storage");
  ASSERT_EQ(true, found != std::end(result.deps));
}

TEST(DependenciesTest, FetchDependencies_PostStorage) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  std::string bid;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid);

  utils::ProtoVec regions_1;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "analytics", regions_1, "", "post_storage");
  ASSERT_EQ(getBid(0), bid);

  // fetch dependencies from root
  auto result = server.fetchDependencies(request, "post_storage", "");

  ASSERT_EQ(OK, result.res);
  ASSERT_EQ(1, result.deps.size());
  auto found = std::find(std::begin(result.deps), std::end(result.deps), "analytics");
  ASSERT_EQ(true, found != std::end(result.deps));
}

TEST(DependenciesTest, FetchDependencies_InvalidContext) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  std::string bid;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid);

  // fetch dependencies from root
  auto result = server.fetchDependencies(request, "", "invalid context service");
  ASSERT_EQ(INVALID_CONTEXT, result.res);
}

TEST(DependenciesTest, FetchDependencies_InvalidService) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  std::string bid;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid);

  // fetch dependencies from root
  auto result = server.fetchDependencies(request, "invalid service", "");
  ASSERT_EQ(INVALID_SERVICE, result.res);
}

TEST(DependenciesTest, Wait) {
  // total wait waits for all of its dependencies
  // - dependencies are root -> {post_storage -> {analytics}}
  // - parent_node = root
  // <=> waits for dependencies of root (direct dep. = post_storage | indirect dep. = analytics)

  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  std::string bid;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid);

  utils::ProtoVec regions_1;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "analytics", regions_1, "", "post_storage");
  ASSERT_EQ(getBid(0), bid);

  int r = server.closeBranch(request, getBid(0), ""); // post_storage
  ASSERT_EQ(1, r);

  sleep(1);

  // TOTAL WAIT
  threads.emplace_back([&server, request] {
    int status = server.wait(request, "", "", "");
    ASSERT_EQ(1, status);
  });

  sleep(1);

  r = server.closeBranch(request, getBid(1), ""); // post_storage (write post)
  ASSERT_EQ(1, r);

  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(DependenciesTest, WaitService_PostStorage) {
  // wait on service waits on all of its dependencies
  // i.e., waits for analytics
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  std::string bid;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid);

  utils::ProtoVec regions_1;
  regions_1.Add("EU");
  regions_1.Add("US");
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_1, "write_post", "");
  ASSERT_EQ(getBid(0), bid);

  utils::ProtoVec regions_2;
  regions_2.Add("AP");
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "analytics", regions_2, "", "post_storage");
  ASSERT_EQ(getBid(0), bid);

  int r = server.closeBranch(request, getBid(0), ""); // post_storage
  ASSERT_EQ(1, r);
  r = server.closeBranch(request, getBid(1), "EU"); // post_storage (write post)
  ASSERT_EQ(1, r);
  r = server.closeBranch(request, getBid(1), "US"); // post_storage (write post)
  ASSERT_EQ(1, r);

  sleep(1);

  // TOTAL WAIT
  threads.emplace_back([&server, request] {
    int status = server.wait(request, "post_storage", "", "");
    ASSERT_EQ(1, status);
  });

  sleep(1);

  r = server.closeBranch(request, getBid(2), "AP"); // analytics
  ASSERT_EQ(1, r);

  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(DependenciesTest, WaitServiceTag_PostStorage_WritePost) {
  // wait on service tag does not wait for its dependencies
  // i.e., in this case, does not wait for the analytics nor the post_storage global service branch

  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  std::string bid;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid);

  utils::ProtoVec regions_1;
  regions_1.Add("EU");
  regions_1.Add("US");
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_1, "write_post", "");
  ASSERT_EQ(getBid(0), bid);

  utils::ProtoVec regions_2;
  regions_2.Add("AP");
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "analytics", regions_2, "", "post_storage");
  ASSERT_EQ(getBid(0), bid);

  sleep(1);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "post_storage", "", "write_post");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  sleep(1);

  int r = server.closeBranch(request, getBid(1), "EU"); // post_storage (write post)
  ASSERT_EQ(1, r);
  r = server.closeBranch(request, getBid(1), "US"); // post_storage (write post)
  ASSERT_EQ(1, r);

  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(DependenciesTest, WaitRegion_EU_OnAnalyticsNode) {
  // wait on EU region, done on the analytics service
  // waits on the upper dependencies except direct parents
  // i.e. the wait is done on the analytics so it does not wait for anything!
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  std::string bid;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid);

  utils::ProtoVec regions_2;
  regions_2.Add("EU");
  bid = server.registerBranchGTest(request, ROOT_SUB_RID, "analytics", regions_2, "", "post_storage");
  ASSERT_EQ(getBid(0), bid);

  sleep(1);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "", "", "", "analytics");
    ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);
  });

  sleep(1);

  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}