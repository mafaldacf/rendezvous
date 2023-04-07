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

TEST(ServerTest, GetRequest) {
  rendezvous::Server server(SID); 

  metadata::Request * request = server.getOrRegisterRequest(RID);
  metadata::Request * request2 = server.getRequest(RID);

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

  server.closeBranch(request, "s", "r", getBid(0));
}

TEST(ServerTest, CheckRequest_AllContexts) { 
  rendezvous::Server server(SID);
  int status;

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

  server.closeBranch(request, "", "", getBid(0)); // bid 0
  status = server.checkRequest(request, "", "");
  ASSERT_EQ(OPENED, status);

  /* Close branches with service 's' and verify request is closed for that service */

  server.closeBranch(request, "s", "", getBid(1)); // bid 1
  server.closeBranch(request, "s", "r", getBid(3)); // bid 3
  status = server.checkRequest(request, "s", "");
  ASSERT_EQ(CLOSED, status);

  /* Remaining checks */

  status = server.checkRequest(request, "", "r");
  ASSERT_EQ(OPENED, status);

  server.closeBranch(request, "", "r", getBid(2)); // bid 2
  status = server.checkRequest(request, "s", "r");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "r");
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
  server.closeBranch(request, "", "r2", getBid(0));
  server.closeBranch(request, "s2", "r2", getBid(2));

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
  server.closeBranch(request, "", "", getBid(0));
  server.closeBranch(request, "", "r1", getBid(0));
  server.closeBranch(request, "s1", "", getBid(1));
  server.closeBranch(request, "s1", "r1", getBid(1));
  server.closeBranch(request, "s2", "r1", getBid(2));
  server.closeBranch(request, "s2", "", getBid(2));

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
  server.closeBranch(request, "", "r2", getBid(4)); // bid 4
  server.closeBranch(request, "s2", "r2", getBid(7)); // bid 7

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
  server.closeBranch(request, "", "", getBid(0)); // bid 0
  server.closeBranch(request, "s1", "", getBid(1)); // bid 1
  server.closeBranch(request, "s2", "", getBid(2)); // bid 2
  server.closeBranch(request, "", "r1", getBid(3)); // bid 3
  server.closeBranch(request, "s1", "r1", getBid(5)); // bid 5
  server.closeBranch(request, "s2", "r1", getBid(6)); // bid 6

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
  server.closeBranch(request, "", "", getBid(0)); // bid 0
  server.closeBranch(request, "service1", "", getBid(1)); // bid 1
  server.closeBranch(request, "", "region1", getBid(2)); // bid 2

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

  server.closeBranch(request, "service2", "region2", getBid(3)); // bid 3
  server.closeBranch(request, "storage", "EU", getBid(4)); // bid 4
  server.closeBranch(request, "storage", "US", getBid(5)); // bid 5
  server.closeBranch(request, "notification", "GLOBAL", getBid(6)); // bid 6
  
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