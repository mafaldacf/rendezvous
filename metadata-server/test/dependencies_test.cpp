#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>
#include "utils.h"

// ---------------------------------------------
// DEPENDENCIES TEST w/ POST NOTIFICATION sample
// ---------------------------------------------

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
  bid = server.registerBranch(request, "post_storage", regions_0, "", "");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  utils::ProtoVec regions_1;
  bid = server.registerBranch(request, "analytics", regions_1, "", "post_storage");
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid);

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
  bid = server.registerBranch(request, "post_storage", regions_0, "", "");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  utils::ProtoVec regions_1;
  regions_1.Add("EU");
  regions_1.Add("US");
  bid = server.registerBranch(request, "post_storage", regions_1, "write_post", "");
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid);

  utils::ProtoVec regions_2;
  regions_2.Add("AP");
  bid = server.registerBranch(request, "analytics", regions_2, "", "post_storage");
  ASSERT_EQ(getFullBid(request->getRid(), 2), bid);

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
  bid = server.registerBranch(request, "post_storage", regions_0, "", "");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  utils::ProtoVec regions_1;
  regions_1.Add("EU");
  regions_1.Add("US");
  bid = server.registerBranch(request, "post_storage", regions_1, "write_post", "");
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid);

  utils::ProtoVec regions_2;
  regions_2.Add("AP");
  bid = server.registerBranch(request, "analytics", regions_2, "", "post_storage");
  ASSERT_EQ(getFullBid(request->getRid(), 2), bid);

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
  bid = server.registerBranch(request, "post_storage", regions_0, "", "");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  utils::ProtoVec regions_2;
  regions_2.Add("EU");
  bid = server.registerBranch(request, "analytics", regions_2, "", "post_storage");
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid);

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