//#include "monitor.grpc.pb.h"
#include "../src/server.h"
#include "../src/request.h"
#include "gtest/gtest.h"
#include <thread>

/* ---------------

TEST SERVER LOGIC

----------------- */

const std::string SID = "eu";

const int OK = 0;
const int OPENED = 0;
const int CLOSED = 1;
const int PREVENTED_INCONSISTENCY = 1;

const int INVALID_REQUEST = -1;
const int SERVICE_NOT_FOUND = -2;
const int REGION_NOT_FOUND = -3;
const int INVALID_BRANCH_SERVICE = -2;
const int INVALID_BRANCH_REGION = -3;

const std::string RID = "myrequestid";

std::string getBid(int id) {
  return SID + ":" + std::to_string(id);
}

TEST(ServerTest, RegisterRequest_WithNoRid) { 
  server::Server server(SID);

  metadata::Request * request = server.registerRequest("");
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ("rendezvous-0", request->getRid());

  metadata::Request * request2 = server.getRequest("rendezvous-0");
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, RegisterRequest_WithRid) { 
  server::Server server(SID);

  metadata::Request * request = server.registerRequest(RID);
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ(RID, request->getRid());

  metadata::Request * request2 = server.getRequest(RID);
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request2->getRid());
}

TEST(ServerTest, RegisterRequest_AlreadyExists) { 
  server::Server server(SID);

  metadata::Request * request = server.registerRequest(RID);
  request = server.registerRequest(RID);
  ASSERT_EQ(nullptr, request);
}

TEST(ServerTest, GetRequest_InvalidRID) {
  server::Server server(SID); 

  metadata::Request * request = server.getRequest("invalidRID");
  ASSERT_TRUE(request == nullptr);
}

TEST(ServerTest, GetRequest) {
  server::Server server(SID); 

  metadata::Request * request = server.registerRequest(RID);
  metadata::Request * request2 = server.getRequest(RID);

  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, RegisterBranch) { 
  server::Server server(SID);

  metadata::Request * request = server.registerRequest(RID);

  std::string bid = server.registerBranch(request, "", "");
  ASSERT_EQ(getBid(0), bid);
}

TEST(ServerTest, RegisterBranch_WithService) { 
  server::Server server(SID);

  metadata::Request * request = server.registerRequest(RID);

  std::string bid = server.registerBranch(request, "s", "");
  ASSERT_EQ(getBid(0), bid);
}

TEST(ServerTest, RegisterBranch_WithRegion) { 
  server::Server server(SID);

  metadata::Request * request = server.registerRequest(RID);

  std::string bid = server.registerBranch(request, "", "r");
  ASSERT_EQ(getBid(0), bid);
}

TEST(ServerTest, RegisterBranch_WithServiceAndRegion) { 
  server::Server server(SID);

  metadata::Request * request = server.registerRequest(RID);

  std::string bid = server.registerBranch(request, "s", "r");
  ASSERT_EQ(getBid(0), bid);
}

TEST(ServerTest, CloseBranch) { 
  server::Server server(SID);

  metadata::Request * request = server.registerRequest(RID);
  
  std::string bid = server.registerBranch(request, "s", "r");

  int status = server.closeBranch(request->getRid(), "s", "r", getBid(0));
  ASSERT_EQ(OK, status);
}

TEST(ServerTest, CloseBranch_WithInvalidRid) {
  server::Server server(SID);

  metadata::Request * request = server.registerRequest(RID);
  std::string bid = server.registerBranch(request, "s", "r");

  int status = server.closeBranch("Invalid RID", "s", "r", getBid(0));
  ASSERT_EQ(INVALID_REQUEST, status);
}

TEST(ServerTest, CloseBranch_WithInvalidContext) { 
  server::Server server(SID);

  metadata::Request * request = server.registerRequest(RID);
  
  std::string bid = server.registerBranch(request, "service", "region");

  int status = server.closeBranch(request->getRid(), "wrong_service", "region", getBid(0));
  ASSERT_EQ(INVALID_BRANCH_SERVICE, status);
  status = server.closeBranch(request->getRid(), "service", "wrong_region", getBid(0));
  ASSERT_EQ(INVALID_BRANCH_REGION, status);
}

TEST(ServerTest, CheckRequest_WithInvalidRid) { 
  server::Server server(SID);
  metadata::Request * request = server.registerRequest(RID);
  int status = server.checkRequest("Invalid RID", "", "");
  ASSERT_EQ(INVALID_REQUEST, status);
}

TEST(ServerTest, CheckRequest_AllContexts) { 
  server::Server server(SID);
  int status;

  /* Register Request and Branches with Multiple Contexts */

  metadata::Request * request = server.registerRequest(RID);
  
  server.registerBranch(request, "", ""); // bid = 0
  server.registerBranch(request, "s", ""); // bid = 1
  server.registerBranch(request, "", "r"); // bid = 2
  server.registerBranch(request, "s", "r"); // bid = 3

  /* Check Request for Multiple Contexts */

  status = server.checkRequest(request->getRid(), "", "");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request->getRid(), "s", "");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request->getRid(), "", "r");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request->getRid(), "s", "r");
  ASSERT_EQ(OPENED, status);

  /* Close branch with no context and verify request is still opened */

  status = server.closeBranch(request->getRid(), "", "", getBid(0)); // bid 0
  ASSERT_EQ(OK, status);
  status = server.checkRequest(request->getRid(), "", "");
  ASSERT_EQ(OPENED, status);

  /* Close branches with service 's' and verify request is closed for that service */

  status = server.closeBranch(request->getRid(), "s", "", getBid(1)); // bid 1
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s", "r", getBid(3)); // bid 3
  ASSERT_EQ(OK, status);
  status = server.checkRequest(request->getRid(), "s", "");
  ASSERT_EQ(CLOSED, status);

  /* Remaining checks */

  status = server.checkRequest(request->getRid(), "", "r");
  ASSERT_EQ(OPENED, status);

  server.closeBranch(request->getRid(), "", "r", getBid(2)); // bid 2
  status = server.checkRequest(request->getRid(), "s", "r");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request->getRid(), "", "r");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request->getRid(), "", "");
  ASSERT_EQ(CLOSED, status);
}

TEST(ServerTest, CheckRequest_ContextNotFound) { 
  server::Server server(SID);
  int status;

  metadata::Request * request = server.registerRequest(RID);
  
  server.registerBranch(request, "", ""); // bid = 0
  server.registerBranch(request, "s", ""); // bid = 1
  server.registerBranch(request, "", "r"); // bid = 2
  server.registerBranch(request, "s", "r"); // bid = 3

  status = server.checkRequest(request->getRid(), "s1", "");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);

  status = server.checkRequest(request->getRid(), "", "r1");
  ASSERT_EQ(REGION_NOT_FOUND, status);

  status = server.checkRequest(request->getRid(), "s1", "r1");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);
}

TEST(ServerTest, CheckRequestByRegions_WithInvalidRid) { 
  server::Server server(SID);
  std::map<std::string, int> result;
  int status = 0;
  metadata::Request * request = server.registerRequest(RID);
  result = server.checkRequestByRegions("Invalid RID", "", &status);
  ASSERT_EQ(INVALID_REQUEST, status);
}

TEST(ServerTest, CheckRequestByRegions_AllContexts_SetOfBranches) { 
  server::Server server(SID);
  std::map<std::string, int> result;
  std::string bid;
  int status;

  metadata::Request * request = server.registerRequest(RID);
  
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

  result = server.checkRequestByRegions(request->getRid(), "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_TRUE(result.size() == 2); // 2 opened regions: 'r1' and 'r2'
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(OPENED, result["r2"]);

  /* close all branches on 'r2' */
  status = server.closeBranch(request->getRid(), "", "r2", getBid(0));
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s2", "r2", getBid(2));
  ASSERT_EQ(OK, status);

  result = server.checkRequestByRegions(request->getRid(), "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(2, result.size()); // 2 regions: 'r1' (opened) and 'r2' (closed)
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);

  result = server.checkRequestByRegions(request->getRid(), "s1" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(1, result.size()); // service 's1' only has region 'r1' which is opened
  ASSERT_TRUE(result.count("r1"));
  ASSERT_FALSE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);

  /* close remaining branches */
  status = server.closeBranch(request->getRid(), "", "", getBid(0));
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s1", "", getBid(1));
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s2", "", getBid(2));
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "", "r1", getBid(0));
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s1", "r1", getBid(1));
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s2", "r1", getBid(2));
  ASSERT_EQ(OK, status);

  /* request is now closed for every region */
  result = server.checkRequestByRegions(request->getRid(), "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(2, result.size());
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(CLOSED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);
}

TEST(ServerTest, CheckRequestByRegions_AllContexts) { 
  server::Server server(SID);
  std::map<std::string, int> result;
  std::string bid;
  int status;

  metadata::Request * request = server.registerRequest(RID);
  
  bid = server.registerBranch(request, "", ""); // bid = 0
  bid = server.registerBranch(request, "s1", ""); // bid = 1
  bid = server.registerBranch(request, "s2", ""); // bid = 2
  bid = server.registerBranch(request, "", "r1"); // bid = 3
  bid = server.registerBranch(request, "", "r2"); // bid = 4
  bid = server.registerBranch(request, "s1", "r1"); // bid = 5
  bid = server.registerBranch(request, "s2", "r1"); // bid = 6
  bid = server.registerBranch(request, "s2", "r2"); // bid = 7

  result = server.checkRequestByRegions(request->getRid(), "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_TRUE(result.size() == 2); // 2 opened regions: 'r1' and 'r2'
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(OPENED, result["r2"]);

  /* close all branches on 'r2' */
  status = server.closeBranch(request->getRid(), "", "r2", getBid(4)); // bid 4
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s2", "r2", getBid(7)); // bid 7
  ASSERT_EQ(OK, status);

  result = server.checkRequestByRegions(request->getRid(), "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(2, result.size()); // 2 regions: 'r1' (opened) and 'r2' (closed)
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);

  result = server.checkRequestByRegions(request->getRid(), "s1" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(1, result.size()); // service 's1' only has region 'r1' which is opened
  ASSERT_TRUE(result.count("r1"));
  ASSERT_FALSE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);

  /* close remaining branches */
  status = server.closeBranch(request->getRid(), "", "", getBid(0)); // bid 0
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s1", "", getBid(1)); // bid 1
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s2", "", getBid(2)); // bid 2
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "", "r1", getBid(3)); // bid 3
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s1", "r1", getBid(5)); // bid 5
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "s2", "r1", getBid(6)); // bid 6
  ASSERT_EQ(OK, status);

  /* request is now closed for every region */
  result = server.checkRequestByRegions(request->getRid(), "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_EQ(2, result.size());
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(CLOSED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);
}

TEST(ServerTest, CheckRequestByRegions_RegionAndContextNotFound) { 
  server::Server server(SID);
  std::map<std::string, int> result;
  int status;

  metadata::Request * request = server.registerRequest(RID);
  

  result = server.checkRequestByRegions(request->getRid(), "" ,&status);
  ASSERT_EQ(0, result.size());

  server.registerBranch(request, "", ""); // bid = 0
  server.registerBranch(request, "s", ""); // bid = 1

  result = server.checkRequestByRegions(request->getRid(), "" ,&status);
  ASSERT_EQ(0, result.size());

  server.registerBranch(request, "", "r"); // bid = 2
  server.registerBranch(request, "s", "r"); // bid = 3

  result = server.checkRequestByRegions(request->getRid(), "s1" ,&status);
  ASSERT_EQ(SERVICE_NOT_FOUND, status);
}

// sanity check
TEST(ServerTest, PreventedInconsistencies_GetZeroValue) { 
  server::Server server(SID);

  long value = server.getPreventedInconsistencies();
  ASSERT_EQ(0, value);
}

TEST(gRPCTest, WaitRequest_InvalidRid) {
  server::Server server(SID);
  metadata::Request * request = server.registerRequest(RID);
  int status = server.waitRequest("Invalid RID", "", "");
  ASSERT_EQ(INVALID_REQUEST, status);
}

TEST(gRPCTest, WaitRequest_ContextNotFound) {
  server::Server server(SID);
  int status;

  metadata::Request * request = server.registerRequest(RID);
  ASSERT_EQ(RID, request->getRid());

  status = server.waitRequest(request->getRid(), "wrong_service", "");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);

  status = server.waitRequest(request->getRid(), "", "wrong_region");
  ASSERT_EQ(REGION_NOT_FOUND, status);

  status = server.waitRequest(request->getRid(), "wrong_service", "wrong_region");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);

  std::string bid = server.registerBranch(request, "service", "region"); // bid = 0
  ASSERT_EQ(getBid(0), bid);

  status = server.waitRequest(request->getRid(), "service", "wrong_region");
  ASSERT_EQ(REGION_NOT_FOUND, status);

  status = server.waitRequest(request->getRid(), "wrong_service", "region");
  ASSERT_EQ(SERVICE_NOT_FOUND, status);
}

TEST(gRPCTest, WaitRequest) { 
  std::vector<std::thread> threads;
  server::Server server(SID);
  std::string bid;
  int status;

  metadata::Request * request = server.registerRequest(RID);
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
    int status = server.waitRequest(request->getRid(), "", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request->getRid(), "service1", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request->getRid(), "", "region1");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request->getRid(), "service2", "region2");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  sleep(0.5);
  status = server.closeBranch(request->getRid(), "", "", getBid(0)); // bid 0
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "service1", "", getBid(1)); // bid 1
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "", "region1", getBid(2)); // bid 2
  ASSERT_EQ(OK, status);

  /* Sanity Check - ensure that locks still work */
  bid =  server.registerBranch(request, "storage", "EU"); // bid = 4
  ASSERT_EQ(getBid(4), bid);

  bid =  server.registerBranch(request, "storage", "US"); // bid = 5
  ASSERT_EQ(getBid(5), bid);

  bid =  server.registerBranch(request, "notification", "GLOBAL"); // bid = 6
  ASSERT_EQ(getBid(6), bid);

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request->getRid(), "", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request->getRid(), "storage", "US");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request->getRid(), "storage", "");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  threads.emplace_back([&server, request] {
    int status = server.waitRequest(request->getRid(), "", "GLOBAL");
    ASSERT_EQ(PREVENTED_INCONSISTENCY, status);
  });
  threads.back().detach();

  sleep(0.5);

  status = server.closeBranch(request->getRid(), "service2", "region2", getBid(3)); // bid 3
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "storage", "EU", getBid(4)); // bid 4
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "storage", "US", getBid(5)); // bid 5
  ASSERT_EQ(OK, status);
  status = server.closeBranch(request->getRid(), "notification", "GLOBAL", getBid(6)); // bid 6
  ASSERT_EQ(OK, status);
  
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