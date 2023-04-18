#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>

/* ---------------

TEST SERVER LOGIC

----------------- */


const int OK = 0;
const int OPENED = 0;
const int CLOSED = 1;
const int PREVENTED_INCONSISTENCY = 1;

const int INVALID_REQUEST = -1;
const int SERVICE_NOT_FOUND = -2;
const int REGION_NOT_FOUND = -3;
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

TEST(ServerTest, getOrRegisterRequest_WithNoRid) { 
  rendezvous::Server server(SID);

  metadata::Request * request = server.getOrRegisterRequest(getRid(0));
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ(getRid(0), request->getRid());

  metadata::Request * request2 = server.getRequest(getRid(0));
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, getOrRegisterRequest_WithRid) { 
  rendezvous::Server server(SID);

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ(RID, request->getRid());

  metadata::Request * request2 = server.getRequest(RID);
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request2->getRid());
}

TEST(ServerTest, GetRequest_InvalidRID) {
  rendezvous::Server server(SID); 

  metadata::Request * request = server.getRequest("invalidRID");
  ASSERT_TRUE(request == nullptr);
}

TEST(ServerTest, GetOrRegisterAndGetRequest) {
  rendezvous::Server server(SID); 

  metadata::Request * request = server.getOrRegisterRequest(RID);
  metadata::Request * request2 = server.getRequest(RID);

  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, GetOrRegisterTwiceRequest) {
  rendezvous::Server server(SID); 

  metadata::Request * request = server.getOrRegisterRequest(RID);
  metadata::Request * request2 = server.getOrRegisterRequest(RID);

  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, RegisterBranch) { 
  rendezvous::Server server(SID);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "", "");
  ASSERT_EQ(getBid(0), bid);
}

TEST(ServerTest, RegisterBranch_WithService) { 
  rendezvous::Server server(SID);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "s", "");
  ASSERT_EQ(getBid(0), bid);
}

TEST(ServerTest, RegisterBranch_WithRegion) { 
  rendezvous::Server server(SID);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "", "r");
  ASSERT_EQ(getBid(0), bid);
}

TEST(ServerTest, RegisterBranch_WithServiceAndRegion) { 
  rendezvous::Server server(SID);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "s", "r");
  ASSERT_EQ(getBid(0), bid);
}

TEST(ServerTest, CloseBranch) { 
  rendezvous::Server server(SID);

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  std::string bid = server.registerBranch(request, "s", "r");

  bool found_region =server.closeBranch(request, getBid(0), "r");
  ASSERT_EQ(true, found_region);
}

TEST(ServerTest, CloseBranchBeforeRegister) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;

  metadata::Request * request = server.getOrRegisterRequest(RID);

  threads.emplace_back([&server, request] {
    bool found_region = server.closeBranch(request, getBid(0), "region");
    ASSERT_EQ(true, found_region);
  });
  threads.back().detach();

  std::string bid = server.registerBranch(request, "service", "region");
  ASSERT_EQ(getBid(0), bid);

  // sanity check - wait threads
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}

TEST(ServerTest, CloseBranchInvalidRegion) {
  rendezvous::Server server(SID);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "service", "eu-central-1");

  bool found_region = server.closeBranch(request, getBid(0), "us-east-1");
  ASSERT_EQ(false, found_region);
}

TEST(ServerTest, CheckRequest_AllContexts) { 
  rendezvous::Server server(SID);
  int status;
  bool found_region = false;

  /* Register Request and Branches with Multiple Contexts */

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  server.registerBranch(request, "", ""); // bid = 0
  server.registerBranch(request, "s", ""); // bid = 1
  server.registerBranch(request, "", "r"); // bid = 2
  server.registerBranch(request, "s", "r"); // bid = 3

  /* Check Request for Multiple Contexts */

  status = server.checkRequest(request, "", "");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "s", "");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "", "r");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "s", "r");
  ASSERT_EQ(OPENED, status);

  /* Close branch with no context and verify request is still opened */

  found_region = server.closeBranch(request, getBid(0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "", "");
  ASSERT_EQ(OPENED, status);

  /* Close branches with service 's' and verify request is closed for that service */

  found_region = server.closeBranch(request, getBid(1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(3), "r"); // bid 3
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "s", "");
  ASSERT_EQ(CLOSED, status);

  /* Remaining checks */

  status = server.checkRequest(request, "", "r");
  ASSERT_EQ(OPENED, status);

  found_region = server.closeBranch(request, getBid(2), "r"); // bid 2
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "s", "r");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "r");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "");
  ASSERT_EQ(CLOSED, status);
}

TEST(ServerTest, CheckRequest_AllContexts_MultipleServices_SetsOfBranches) { 
  rendezvous::Server server(SID);
  int status;
  bool found_region = false;
  std::string bid = "";

  /* Register Request and Branches with Multiple Contexts */

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  google::protobuf::RepeatedPtrField<std::string> regionsNoService;
  regionsNoService.Add("EU");
  regionsNoService.Add("US");
  bid = server.registerBranches(request, "notifications", regionsNoService);
  ASSERT_EQ(getBid(0), bid);

  google::protobuf::RepeatedPtrField<std::string> regionsService;
  regionsService.Add("EU");
  regionsService.Add("US");
  bid = server.registerBranches(request, "post-storage", regionsService);
  ASSERT_EQ(getBid(1), bid);

  /* multiple services verifications */

  status = server.checkRequest(request, "post-storage", "");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "post-storage", "EU");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "post-storage", "US");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "notifications", "");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "notifications", "EU");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "notifications", "US");
  ASSERT_EQ(OPENED, status);

  /* 'notifications' service verifications */

  found_region = server.closeBranch(request, getBid(0), "EU"); // post-storage
  ASSERT_EQ(true, found_region);

  status = server.checkRequest(request, "notifications", "EU");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "notifications", "");
  ASSERT_EQ(OPENED, status);

  /* more branches for 'post-storage' service */

  google::protobuf::RepeatedPtrField<std::string> regionsService2;
  regionsService2.Add("CH");
  regionsService2.Add("EU");
  bid = server.registerBranches(request, "post-storage", regionsService2);
  ASSERT_EQ(getBid(2), bid);

  /* 'post-storage' service verifications */

  found_region = server.closeBranch(request, getBid(1), "EU"); // notifications
  ASSERT_EQ(true, found_region);

  found_region = server.closeBranch(request, getBid(1), "US"); // notifications
  ASSERT_EQ(true, found_region);

  status = server.checkRequest(request, "post-storage", "");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "post-storage", "EU");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "post-storage", "CH");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "post-storage", "US");
  ASSERT_EQ(CLOSED, status);

  found_region = server.closeBranch(request, getBid(2), "CH"); // notifications
  ASSERT_EQ(true, found_region);

  status = server.checkRequest(request, "post-storage", "CH");
  ASSERT_EQ(CLOSED, status);

  found_region = server.closeBranch(request, getBid(2), "EU"); // notifications
  ASSERT_EQ(true, found_region);

  status = server.checkRequest(request, "post-storage", "EU");
  ASSERT_EQ(CLOSED, status);

  /* global verifications */

  status = server.checkRequest(request, "post-notifications", "");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);

  status = server.checkRequest(request, "", "EU");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "US");
  ASSERT_EQ(OPENED, status);

  /* remaining 'notifications' service verifications */

  status = server.checkRequest(request, "notifications", "US");
  ASSERT_EQ(OPENED, status);

  found_region = server.closeBranch(request, getBid(0), "US"); // post-storage
  ASSERT_EQ(true, found_region);

  /* remaining global verifications */

  status = server.checkRequest(request, "", "EU");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "US");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "");
  ASSERT_EQ(CLOSED, status);
}

TEST(ServerTest, CheckRequest_ContextNotFound) { 
  rendezvous::Server server(SID);
  int status;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  server.registerBranch(request, "", ""); // bid = 0
  server.registerBranch(request, "s", ""); // bid = 1
  server.registerBranch(request, "", "r"); // bid = 2
  server.registerBranch(request, "s", "r"); // bid = 3

  status = server.checkRequest(request, "s1", "");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);

  status = server.checkRequest(request, "", "r1");
  ASSERT_EQ(REGION_NOT_FOUND, status);

  status = server.checkRequest(request, "s1", "r1");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);
}

TEST(ServerTest, CheckRequestByRegions_AllContexts_SetOfBranches) { 
  rendezvous::Server server(SID);
  std::map<std::string, int> result;
  std::string bid;
  int status;
  bool found_region = false;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  google::protobuf::RepeatedPtrField<std::string> regionsNoService;
  regionsNoService.Add("");
  regionsNoService.Add("r1");
  regionsNoService.Add("r2");
  bid = server.registerBranches(request, "", regionsNoService);
  ASSERT_EQ(getBid(0), bid);

  google::protobuf::RepeatedPtrField<std::string> regionsService1;
  regionsService1.Add("");
  regionsService1.Add("r1");
  bid = server.registerBranches(request, "s1", regionsService1);
  ASSERT_EQ(getBid(1), bid);

  google::protobuf::RepeatedPtrField<std::string> regionsService2;
  regionsService2.Add("");
  regionsService2.Add("r1");
  regionsService2.Add("r2");
  bid = server.registerBranches(request, "s2", regionsService2);
  ASSERT_EQ(getBid(2), bid);

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_TRUE(result.size() == 2); // 2 opened regions: 'r1' and 'r2'
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(OPENED, result["r2"]);

  /* close all branches on 'r2' */
  found_region = server.closeBranch(request, getBid(0), "r2");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(2), "r2");
  ASSERT_EQ(true, found_region);

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(2, result.size()); // 2 regions: 'r1' (opened) and 'r2' (closed)
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);

  result = server.checkRequestByRegions(request, "s1" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(1, result.size()); // service 's1' only has region 'r1' which is opened
  ASSERT_TRUE(result.count("r1"));
  ASSERT_FALSE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);

  /* close remaining branches */
  found_region = server.closeBranch(request, getBid(0), "");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(0), "r1");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(1), "");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(1), "r1");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(2), "r1");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(2), "");
  ASSERT_EQ(true, found_region);

  /* request is now closed for every region */
  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(2, result.size());
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(CLOSED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);
}

TEST(ServerTest, CheckRequestByRegions_AllContexts) { 
  rendezvous::Server server(SID);
  std::map<std::string, int> result;
  std::string bid;
  int status;
  bool found_region = false;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  bid = server.registerBranch(request, "", ""); // bid = 0
  bid = server.registerBranch(request, "s1", ""); // bid = 1
  bid = server.registerBranch(request, "s2", ""); // bid = 2
  bid = server.registerBranch(request, "", "r1"); // bid = 3
  bid = server.registerBranch(request, "", "r2"); // bid = 4
  bid = server.registerBranch(request, "s1", "r1"); // bid = 5
  bid = server.registerBranch(request, "s2", "r1"); // bid = 6
  bid = server.registerBranch(request, "s2", "r2"); // bid = 7

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_TRUE(result.size() == 2); // 2 opened regions: 'r1' and 'r2'
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(OPENED, result["r2"]);

  /* close all branches on 'r2' */
  found_region = server.closeBranch(request, getBid(4), "r2"); // bid 4
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(7), "r2"); // bid 7
  ASSERT_EQ(true, found_region);

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(2, result.size()); // 2 regions: 'r1' (opened) and 'r2' (closed)
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);

  result = server.checkRequestByRegions(request, "s1" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(1, result.size()); // service 's1' only has region 'r1' which is opened
  ASSERT_TRUE(result.count("r1"));
  ASSERT_FALSE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);

  /* close remaining branches */
  found_region = server.closeBranch(request, getBid(0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(2), ""); // bid 2
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(3), "r1"); // bid 3
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(5), "r1"); // bid 5
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(6), "r1"); // bid 6
  ASSERT_EQ(true, found_region);

  /* request is now closed for every region */
  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(2, result.size());
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(CLOSED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);
}

TEST(ServerTest, CheckRequestByRegions_RegionAndContextNotFound) { 
  rendezvous::Server server(SID);
  std::map<std::string, int> result;
  int status;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(0, result.size());

  server.registerBranch(request, "", ""); // bid = 0
  server.registerBranch(request, "s", ""); // bid = 1

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(0, result.size());

  server.registerBranch(request, "", "r"); // bid = 2
  server.registerBranch(request, "s", "r"); // bid = 3

  result = server.checkRequestByRegions(request, "s1" ,&status);
  ASSERT_EQ(SERVICE_NOT_FOUND, status);
}

// sanity check
TEST(ServerTest, PreventedInconsistencies_GetZeroValue) { 
  rendezvous::Server server(SID);

  long value = server.getPreventedInconsistencies();
  ASSERT_EQ(0, value);
}

TEST(gRPCTest, WaitRequest_ContextNotFound) {
  rendezvous::Server server(SID);
  int status;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  status = server.waitRequest(request, "wrong_service", "");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);

  status = server.waitRequest(request, "", "wrong_region");
  ASSERT_EQ(REGION_NOT_FOUND, status);

  status = server.waitRequest(request, "wrong_service", "wrong_region");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);

  std::string bid = server.registerBranch(request, "service", "region"); // bid = 0
  ASSERT_EQ(getBid(0), bid);

  status = server.waitRequest(request, "service", "wrong_region");
  ASSERT_EQ(REGION_NOT_FOUND, status);

  status = server.waitRequest(request, "wrong_service", "region");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);
}

TEST(gRPCTest, WaitRequest) { 
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
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "service1", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "region1");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "service2", "region2");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  sleep(0.5);
  found_region = server.closeBranch(request, getBid(0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(2), "region1"); // bid 2
  ASSERT_EQ(true, found_region);

  /* Sanity Check - ensure that locks still work */
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
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "storage", "US");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "storage", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request, "", "GLOBAL");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  sleep(0.5);

  found_region = server.closeBranch(request, getBid(3), "region2"); // bid 3
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(4), "EU"); // bid 4
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(5), "US"); // bid 5
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(6), "GLOBAL"); // bid 6
  ASSERT_EQ(true, found_region);
  
  /* wait for all threads */
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }

  sleep(0.5);

  /* validate number of prevented inconsistencies*/
  long value = server.getPreventedInconsistencies();
  ASSERT_EQ(8, value);
}