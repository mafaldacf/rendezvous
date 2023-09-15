#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>
#include "utils.h"

// ----------------
// CONCURRENCY TEST
// ----------------

TEST(ConcurrencyTest, CloseBranchBeforeRegister) {
  rendezvous::Server server(SID);
    
  std::vector<std::thread> threads;

  metadata::Request * request = server.getOrRegisterRequest(RID);

  threads.emplace_back([&server, request] {
    sleep(0.1);
    bool found_region = server.closeBranch(request, getBid(0), "region");
    ASSERT_EQ(true, found_region);
  });

  std::string bid = server.registerBranchRegion(request, "service", "region", TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  // sanity check - wait threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(ConcurrencyTest, WaitRequest_ContextNotFound) {
  rendezvous::Server server(SID);
  int status;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  status = server.wait(request, "wrong_service", "");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.wait(request, "", "wrong_region");
  ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);

  status = server.wait(request, "wrong_service", "wrong_region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  std::string bid = server.registerBranchRegion(request, "service", "region", TAG); // bid = 0
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  status = server.wait(request, "service", "wrong_region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.wait(request, "wrong_service", "region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
}

TEST(ConcurrencyTest, WaitRequest_ForcedTimeout) {
  rendezvous::Server server(SID);
  int status;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  std::string bid =  server.registerBranchRegion(request, "service", "region", EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  status = server.wait(request, "service", "region", EMPTY_TAG, "", false, 1);
  ASSERT_EQ(TIMED_OUT, status);

  status = server.wait(request, "service2", "", EMPTY_TAG, "", true, 1);
  ASSERT_EQ(TIMED_OUT, status);
}

TEST(ConcurrencyTest, SimpleWaitRequest) { 
  std::vector<std::thread> threads;
  rendezvous::Server server(SID);
  std::string bid;
  int status;
  bool found_region;;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  status = server.wait(request, "", "");
  ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);

  utils::ProtoVec regions;
  regions.Add("EU");
  regions.Add("US");
  std::string bid_0 = server.registerBranch(request, "service", regions, "tag", "");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid_0);

  threads.emplace_back([&server, request] {
    server.wait(request, "", "");
  });

  sleep(0.5);
  found_region = server.closeBranch(request, getBid(0), "EU"); // bid 0
  found_region = server.closeBranch(request, getBid(0), "US"); // bid 0
  ASSERT_EQ(1, found_region);

  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(ConcurrencyTest, SimpleWaitRequestTwo) { 
  std::vector<std::thread> threads;
  rendezvous::Server server(SID);
  std::string bid;
  int status;
  bool found_region;;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  status = server.wait(request, "", "");
  ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);

  bid =  server.registerBranchRegion(request, "", "region1", TAG); // bid = 0
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  threads.emplace_back([&server, request] {
    server.wait(request, "", "");
  });

  sleep(0.5);
  found_region = server.closeBranch(request, getBid(0), "region1"); // bid 0
  ASSERT_EQ(1, found_region);

  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(ConcurrencyTest, WaitRequest) { 
  std::vector<std::thread> threads;
  rendezvous::Server server(SID);
  std::string bid;
  int status;
  int found_region;;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  status = server.wait(request, "", "");
  ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);

  bid =  server.registerBranchRegion(request, "", "", TAG); // bid = 0
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  bid =  server.registerBranchRegion(request, "service1", "", TAG); // bid = 1
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid);

  bid =  server.registerBranchRegion(request, "", "region1", TAG); // bid = 2
  ASSERT_EQ(getFullBid(request->getRid(), 2), bid);

  bid =  server.registerBranchRegion(request, "service2", "region2", TAG); // bid = 3
  ASSERT_EQ(getFullBid(request->getRid(), 3), bid);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "", "");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "service1", "");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  // MUST NOT wait for itself
  threads.emplace_back([&server, request] {
    int status = server.wait(request, "", "region1");
    ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "service2", "region2");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  sleep(1.5);
  found_region = server.closeBranch(request, getBid(0), ""); // bid 0
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(1), ""); // bid 1
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(2), "region1"); // bid 2
  ASSERT_EQ(1, found_region);

  // Sanity Check - ensure that locks still work
  bid =  server.registerBranchRegion(request, "storage", "EU", "tagA"); // bid = 4
  ASSERT_EQ(getFullBid(request->getRid(), 4), bid);

  bid =  server.registerBranchRegion(request, "storage", "US", "tagB"); // bid = 5
  ASSERT_EQ(getFullBid(request->getRid(), 5), bid);

  bid =  server.registerBranchRegion(request, "notification", "GLOBAL", "tagC"); // bid = 6
  ASSERT_EQ(getFullBid(request->getRid(), 6), bid);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "", "");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "storage", "US");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "storage", "");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "", "GLOBAL");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  sleep(1.5);

  found_region = server.closeBranch(request, getBid(3), "region2"); // bid 3
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(4), "EU"); // bid 4
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(5), "US"); // bid 5
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(6), "GLOBAL"); // bid 6
  ASSERT_EQ(1, found_region);
  
  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }

  sleep(1.5);

  // validate number of prevented inconsistencies
  long value = server._prevented_inconsistencies.load();
  ASSERT_EQ(7, value);
}

TEST(ConcurrencyTest, WaitServiceTag) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions;
  regions.Add("EU");
  regions.Add("US");
  std::string bid_0 = server.registerBranch(request, "service", regions, "tag", "");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid_0);

  sleep(1);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "service", "EU", "tag");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "service", "US", "tag");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "service", "", "tag");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  sleep(1);

  int r = server.closeBranch(request, getBid(0), "EU");
  ASSERT_EQ(1, r);
  r = server.closeBranch(request, getBid(0), "US");
  ASSERT_EQ(1, r);

  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(ConcurrencyTest, WaitServiceTagForceAsync) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "service", "EU", "tag", "", true);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "service", "US", "tag", "", true);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, "service", "", "tag", "", true);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  sleep(1);

  utils::ProtoVec regions;
  regions.Add("EU");
  regions.Add("US");
  std::string bid_0 = server.registerBranch(request, "service", regions, "tag", "");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid_0);

  sleep(1);

  int r = server.closeBranch(request, getBid(0), "EU");
  ASSERT_EQ(1, r);
  r = server.closeBranch(request, getBid(0), "US");
  ASSERT_EQ(1, r);

  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}