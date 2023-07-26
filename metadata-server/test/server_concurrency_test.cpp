#include "../src/server.h"
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
const int INCONSISTENCY_PREVENTED = 1;
const int INCONSISTENCY_NOT_PREVENTED = 0;
const int TIMED_OUT = -1;

const int INVALID_REQUEST = -1;
const int CONTEXT_NOT_FOUND = -2;
const int INVALID_BRANCH_SERVICE = -2;
const int INVALID_BRANCH_REGION = -3;

const std::string _SID = "eu-central-1";
const std::string _RID = "myrequestid";
const std::string _TAG = "mytag";

// for closing branches
std::string _getBid(std::string rid, int id) {
  return _SID + '_' + std::to_string(id);
}

// for register branches
std::string _getFullBid(std::string rid, int id) {
  return _SID + '_' + std::to_string(id) + ":" + rid;
}

TEST(ServerConcurrencyTest, CloseBranchBeforeRegister) {
  rendezvous::Server server(_SID);
    
  std::vector<std::thread> threads;

  metadata::Request * request = server.getOrRegisterRequest(_RID);

  threads.emplace_back([&server, request] {
    sleep(0.1);
    bool found_region = server.closeBranch(request, _getBid(request->getRid(), 0), "region");
    ASSERT_EQ(true, found_region);
  });
  

  std::string bid = server.registerBranch(request, "service", "region", _TAG);
  ASSERT_EQ(_getFullBid(request->getRid(), 0), bid);

  // sanity check - wait threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(ServerConcurrencyTest, WaitRequest_ContextNotFound) {
  rendezvous::Server server(_SID);
  int status;

  metadata::Request * request = server.getOrRegisterRequest(_RID);
  ASSERT_EQ(_RID, request->getRid());

  status = server.waitRequest(request, "wrong_service", "");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.waitRequest(request, "", "wrong_region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.waitRequest(request, "wrong_service", "wrong_region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  std::string bid = server.registerBranch(request, "service", "region", _TAG); // bid = 0
  ASSERT_EQ(_getFullBid(request->getRid(), 0), bid);

  status = server.waitRequest(request, "service", "wrong_region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.waitRequest(request, "wrong_service", "region");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
}

TEST(ServerConcurrencyTest, WaitRequest_ForcedTimeout) {
  rendezvous::Server server(_SID);
  int status;

  metadata::Request * request = server.getOrRegisterRequest(_RID);
  ASSERT_EQ(_RID, request->getRid());

  std::string bid =  server.registerBranch(request, "service", "region", _TAG);
  ASSERT_EQ(_getFullBid(request->getRid(), 0), bid);

  status = server.waitRequest(request, "service", "region", false, 1);
  ASSERT_EQ(TIMED_OUT, status);

  status = server.waitRequest(request, "service2", "", true, 1);
  ASSERT_EQ(TIMED_OUT, status);
}

TEST(ServerConcurrencyTest, WaitRequest) { 
  std::vector<std::thread> threads;
  rendezvous::Server server(_SID);
  std::string bid;
  int status;
  bool found_region = false;

  metadata::Request * request = server.getOrRegisterRequest(_RID);
  ASSERT_EQ(_RID, request->getRid());

  status = server.waitRequest(request, "", "");
  ASSERT_EQ(INCONSISTENCY_NOT_PREVENTED, status);

  bid =  server.registerBranch(request, "", "", _TAG); // bid = 0
  ASSERT_EQ(_getFullBid(request->getRid(), 0), bid);

  bid =  server.registerBranch(request, "service1", "", _TAG); // bid = 1
  ASSERT_EQ(_getFullBid(request->getRid(), 1), bid);

  bid =  server.registerBranch(request, "", "region1", _TAG); // bid = 2
  ASSERT_EQ(_getFullBid(request->getRid(), 2), bid);

  bid =  server.registerBranch(request, "service2", "region2", _TAG); // bid = 3
  ASSERT_EQ(_getFullBid(request->getRid(), 3), bid);

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "service1", "");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "region1");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "service2", "region2");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  sleep(0.5);
  found_region = server.closeBranch(request, _getBid(request->getRid(), 0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, _getBid(request->getRid(), 1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, _getBid(request->getRid(), 2), "region1"); // bid 2
  ASSERT_EQ(true, found_region);

  // Sanity Check - ensure that locks still work
  bid =  server.registerBranch(request, "storage", "EU", _TAG); // bid = 4
  ASSERT_EQ(_getFullBid(request->getRid(), 4), bid);

  bid =  server.registerBranch(request, "storage", "US", _TAG); // bid = 5
  ASSERT_EQ(_getFullBid(request->getRid(), 5), bid);

  bid =  server.registerBranch(request, "notification", "GLOBAL", _TAG); // bid = 6
  ASSERT_EQ(_getFullBid(request->getRid(), 6), bid);

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "storage", "US");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "storage", "");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "GLOBAL");
    ASSERT_EQ(INCONSISTENCY_PREVENTED, status);
  });
  

  sleep(0.5);

  found_region = server.closeBranch(request, _getBid(request->getRid(), 3), "region2"); // bid 3
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, _getBid(request->getRid(), 4), "EU"); // bid 4
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, _getBid(request->getRid(), 5), "US"); // bid 5
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, _getBid(request->getRid(), 6), "GLOBAL"); // bid 6
  ASSERT_EQ(true, found_region);
  
  // wait for all threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }

  sleep(0.5);

  // validate number of prevented inconsistencies
  long value = server._prevented_inconsistencies.load();
  ASSERT_EQ(8, value);
}