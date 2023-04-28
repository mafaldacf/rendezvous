+#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>

// ---------------

// TEST SERVER LOGIC

// ----------------- 


const int OK = 0;
const int OPENED = 0;
const int CLOSED = 1;
const int PREVENTED_INCONSISTENCY = 1;

const int INVALID_REQUEST = -1;
const int CONTEXT_NOT_FOUND = 2;
const int INVALID_BRANCH_SERVICE = -2;
const int INVALID_BRANCH_REGION = -3;

const std::string SID = "eu-central-1";
const std::string RID = "myrequestid";

std::string getRid(int id) {
  return SID + ':' + std::to_string(id);
}

std::string getBid(int id) {
  return SID + ':' + std::to_string(id);
}

TEST(ServerConcurrencyTest, CloseBranchBeforeRegister) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;

  metadata::Request * request = server.getOrRegisterRequest(RID);

  threads.emplace_back([&server, request] {
    sleep(0.1);
    bool found_region = server.closeBranch(request, getBid(0), "region");
    ASSERT_EQ(true, found_region);
  });
  

  std::string bid = server.registerBranch(request, "service", "region");
  ASSERT_EQ(getBid(0), bid);

  // sanity check - wait threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(ServerConcurrencyTest, WaitRequest_ContextNotFound) {
  rendezvous::Server server(SID);
  int status;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  status = server.waitRequest(request, "wrong_service", "");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.waitRequest(request, "", "wrong_region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.waitRequest(request, "wrong_service", "wrong_region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  std::string bid = server.registerBranch(request, "service", "region"); // bid = 0
  ASSERT_EQ(getBid(0), bid);

  status = server.waitRequest(request, "service", "wrong_region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.waitRequest(request, "wrong_service", "region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
}

TEST(ServerConcurrencyTest, WaitRequest) { 
  std::vector<std::thread> threads;
  rendezvous::Server server(SID);
  std::string bid;
  int status;
  bool found_region = false;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  bid =  server.registerBranch(request, "", ""); // bid = 0
  ASSERT_EQ(getBid(0), bid);

  bid =  server.registerBranch(request, "service1", ""); // bid = 1
  ASSERT_EQ(getBid(1), bid);

  bid =  server.registerBranch(request, "", "region1"); // bid = 2
  ASSERT_EQ(getBid(2), bid);

  bid =  server.registerBranch(request, "service2", "region2"); // bid = 3
  ASSERT_EQ(getBid(3), bid);

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "service1", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "region1");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "service2", "region2");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  

  sleep(0.5);
  found_region = server.closeBranch(request, getBid(0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(2), "region1"); // bid 2
  ASSERT_EQ(true, found_region);

  // Sanity Check - ensure that locks still work
  bid =  server.registerBranch(request, "storage", "EU"); // bid = 4
  ASSERT_EQ(getBid(4), bid);

  bid =  server.registerBranch(request, "storage", "US"); // bid = 5
  ASSERT_EQ(getBid(5), bid);

  bid =  server.registerBranch(request, "notification", "GLOBAL"); // bid = 6
  ASSERT_EQ(getBid(6), bid);

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "storage", "US");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "storage", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "GLOBAL");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  

  sleep(0.5);

  found_region = server.closeBranch(request, getBid(3), "region2"); // bid 3
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(4), "EU"); // bid 4
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(5), "US"); // bid 5
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(6), "GLOBAL"); // bid 6
  ASSERT_EQ(true, found_region);
  
  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }

  sleep(0.5);

  // validate number of prevented inconsistencies
  long value = server.getPreventedInconsistencies();
  ASSERT_EQ(8, value);
}