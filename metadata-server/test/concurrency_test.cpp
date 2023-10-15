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

  utils::ProtoVec regions;
  regions.Add("region");
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "service", regions, TAG, "");
  ASSERT_EQ(getBid(0), bid_0);

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

  status = server.wait(request, ROOT_SUB_RID, "wrong_service", "", "", false, 5);
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.wait(request, ROOT_SUB_RID, "", "wrong_region", "", false, 5);
  ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);

  status = server.wait(request, ROOT_SUB_RID, "wrong_service", "wrong_region", "", false, 5);
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  utils::ProtoVec regions;
  regions.Add("region");
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "service", regions, TAG, "");
  ASSERT_EQ(getBid(0), bid_0);

  status = server.wait(request, ROOT_SUB_RID, "service", "wrong_region", "", false, 5);
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.wait(request, ROOT_SUB_RID, "wrong_service", "region", "", false, 5);
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
}

TEST(ConcurrencyTest, WaitRequest_ForcedTimeout) {
  rendezvous::Server server(SID);
  int status;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  utils::ProtoVec regions;
  regions.Add("region");
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "service", regions, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);

  status = server.wait(request, ROOT_SUB_RID, "service", "region", EMPTY_TAG, "", 1);
  ASSERT_EQ(TIMED_OUT, status);

  status = server.wait(request, ROOT_SUB_RID, "service2", "", EMPTY_TAG, "", 1);
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

  status = server.wait(request, ROOT_SUB_RID, "", "");
  ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);

  utils::ProtoVec regions;
  regions.Add("EU");
  regions.Add("US");
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "service", regions, "tag", "");
  ASSERT_EQ(getBid(0), bid_0);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "");
    ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);
  });

  sleep(0.2);
  found_region = server.closeBranch(request, getBid(0), "EU"); // bid 0
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(0), "US"); // bid 0
  ASSERT_EQ(1, found_region);

  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }

  // now we do the same for for an async branch
  std::string next_sub_rid = server.addNextAsyncZone(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_0, next_sub_rid);
  utils::ProtoVec regions_empty;
  std::string bid_1 = server.registerBranchGTest(request, ROOT_SUB_RID, "dummy-service", regions_empty, "", "");
  ASSERT_EQ(getBid(1), bid_1);
  // -----------------------

  utils::ProtoVec regions2;
  regions2.Add("AP");
  std::string bid_2 = server.registerBranchGTest(request, SUB_RID_0, "new-service", regions2, "tag", "");
  ASSERT_EQ(getBid(2), bid_2);
  
  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "AP", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    // we put a wrong region
    int status = server.wait(request, ROOT_SUB_RID, "", "WRONG REGION", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);
  });

  sleep(0.2);

  found_region = server.closeBranch(request, getBid(2), "AP");
  ASSERT_EQ(1, found_region);

  // try for GLOBAL branch
  utils::ProtoVec regions_empty2;
  std::string bid_3 = server.registerBranchGTest(request, SUB_RID_0, "new-service", regions_empty2, "tag2", "");
  ASSERT_EQ(getBid(3), bid_3);

  // we try for global region
  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "EU", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    // we put wrong region
    // in this case, even with a global branch opened, we cannot prevent an inconsistency
    int status = server.wait(request, ROOT_SUB_RID, "", "WRONG REGION", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);
  });

  sleep(0.2);

  found_region = server.closeBranch(request, getBid(3), "");
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

  status = server.wait(request, ROOT_SUB_RID, "", "", "", false, 5);
  ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);

  utils::ProtoVec region;
  region.Add("region1");
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "service", region, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);

  // we cannot wait for branches within the same async_zone_id
  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);
  });

  sleep(1);

  // -----------------------------------------------------
  // ASYNC ZONE
  // -----------------------------------------------------
  // but we can wait if we are in an async zone
  std::string next_sub_rid = server.addNextAsyncZone(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_0, next_sub_rid);
  utils::ProtoVec empty_region;
  // this branch is ignored 
  std::string bid_1 = server.registerBranchGTest(request, SUB_RID_0, "dummy-service", empty_region, EMPTY_TAG, "");
  ASSERT_EQ(getBid(1), bid_1);
  // we do a global wait (implicitly on the ROOT_SUB_RID)
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_0, "", "", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  // we do a wait on region1 (implicitly on the ROOT_SUB_RID)
  threads.emplace_back([&server, request] {
    int status = server.wait(request, SUB_RID_0, "", "region1", "", false, 5);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  // -----------------------------------------------------
  
  sleep(1);
  found_region = server.closeBranch(request, getBid(0), "region1"); // bid 0
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(1), ""); // bid 0
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
  int status;
  int found_region;
  

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  // ASYNC ZONES:
  std::string next_sub_rid = server.addNextAsyncZone(request, ROOT_SUB_RID);
  ASSERT_EQ(SUB_RID_0, next_sub_rid);
  utils::ProtoVec empty_region;
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "dummy-service", empty_region, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);
  // -------------

  // we ignore any branch in current request
  status = server.wait(request, ROOT_SUB_RID, "", "");
  ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);

  // we timeout
  status = server.wait(request, SUB_RID_0, "", "", "", false, 1);
  ASSERT_EQ(TIMED_OUT, status);

  utils::ProtoVec emptyRegion;
  std::string bid_1 = server.registerBranchGTest(request, SUB_RID_0, "service0", emptyRegion, EMPTY_TAG, "");
  ASSERT_EQ(getBid(1), bid_1);

  std::string bid_2 = server.registerBranchGTest(request, SUB_RID_0, "service1", emptyRegion, EMPTY_TAG, "");
  ASSERT_EQ(getBid(2), bid_2);

  utils::ProtoVec region1;
  region1.Add("region1");
  std::string bid_3 = server.registerBranchGTest(request, SUB_RID_0, "service0", region1, EMPTY_TAG, "");
  ASSERT_EQ(getBid(3), bid_3);

  utils::ProtoVec region2;
  region2.Add("region2");
  std::string bid_4 = server.registerBranchGTest(request, SUB_RID_0, "service2", region2, EMPTY_TAG, "");
  ASSERT_EQ(getBid(4), bid_4);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "service1", "", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "region1", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "service2", "region2", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  sleep(1);
  found_region = server.closeBranch(request, getBid(1), ""); // bid 0
  ASSERT_EQ(1, found_region);

  // must only check previous service (service0) which is closed
  // must ignore it current service (service1) which is opened
  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "region1", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  // here we force async because we only register it later?
  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "GLOBAL", "", true, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  sleep(1);

  found_region = server.closeBranch(request, getBid(2), ""); // bid 1
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(3), "region1"); // bid 2
  ASSERT_EQ(1, found_region);

  // Sanity Check - ensure that locks still work
  utils::ProtoVec region_EU;
  region_EU.Add("EU");
  std::string bid_5 = server.registerBranchGTest(request, SUB_RID_0, "storage", region_EU, EMPTY_TAG, "");
  ASSERT_EQ(getBid(5), bid_5);

  utils::ProtoVec region_US;
  region_US.Add("US");
  std::string bid_6 = server.registerBranchGTest(request, SUB_RID_0, "storage", region_US, EMPTY_TAG, "");
  ASSERT_EQ(getBid(6), bid_6);

  utils::ProtoVec region_GLOBAL;
  region_GLOBAL.Add("GLOBAL");
  std::string bid_7 = server.registerBranchGTest(request, SUB_RID_0, "notification", region_GLOBAL, "tagC", "");
  ASSERT_EQ(getBid(7), bid_7);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "storage", "US", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "storage", "", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "", "GLOBAL", "", false, 7);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  sleep(1);

  found_region = server.closeBranch(request, getBid(4), "region2");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(5), "EU");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(6), "US");
  ASSERT_EQ(1, found_region);
  found_region = server.closeBranch(request, getBid(7), "GLOBAL");
  ASSERT_EQ(1, found_region);
  
  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }

  // validate number of prevented inconsistencies
  long value = server._prevented_inconsistencies.load();
  ASSERT_EQ(11, value);
}

TEST(ConcurrencyTest, WaitServiceTag) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions;
  regions.Add("EU");
  regions.Add("US");
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "service", regions, "tag", "");
  ASSERT_EQ(getBid(0), bid_0);

  sleep(1);

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "service", "EU", "tag");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "service", "US", "tag");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "service", "", "tag");
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
    int status = server.wait(request, ROOT_SUB_RID, "service", "EU", "tag", true);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "service", "US", "tag", true);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  threads.emplace_back([&server, request] {
    int status = server.wait(request, ROOT_SUB_RID, "service", "", "tag", true);
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });

  sleep(1);

  utils::ProtoVec regions;
  regions.Add("EU");
  regions.Add("US");
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "service", regions, "tag", "");
  ASSERT_EQ(getBid(0), bid_0);

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