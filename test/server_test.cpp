//#include "monitor.grpc.pb.h"
#include "../src/server.h"
#include "../src/request.h"
#include "gtest/gtest.h"

/* ---------------

TEST SERVER LOGIC

----------------- */

const int OK = 0;
const int OPENED = 0;
const int CLOSED = 1;
const int CONTEXT_NOT_FOUND = -1;
const int REGION_NOT_FOUND = -2;
const int INVALID_BRANCH = -1;

const std::string RID = "myrequestid";

TEST(ServerTest, RegisterRequest_WithNoRid) { 
  server::Server server;

  request::Request * request = server.registerRequest("");
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ("rendezvous-0", request->getRid());

  request::Request * request2 = server.getRequest("rendezvous-0");
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, RegisterRequest_WithRid) { 
  server::Server server;

  request::Request * request = server.registerRequest(RID);
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ(RID, request->getRid());

  request::Request * request2 = server.getRequest(RID);
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, GetRequest_InvalidRID) {
  server::Server server; 

  request::Request * request = server.getRequest("invalidRID");
  ASSERT_TRUE(request == nullptr);
}

TEST(ServerTest, GetRequest) {
  server::Server server; 

  request::Request * request = server.registerRequest(RID);
  request::Request * request2 = server.getRequest(RID);

  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, RegisterBranch) { 
  server::Server server;

  request::Request * request = server.registerRequest(RID);

  long bid = server.registerBranch(request, "", "");
  ASSERT_EQ(0, bid);
}

TEST(ServerTest, RegisterBranch_WithService) { 
  server::Server server;

  request::Request * request = server.registerRequest(RID);

  long bid = server.registerBranch(request, "s", "");
  ASSERT_EQ(0, bid);
}

TEST(ServerTest, RegisterBranch_WithRegion) { 
  server::Server server;

  request::Request * request = server.registerRequest(RID);

  long bid = server.registerBranch(request, "", "r");
  ASSERT_EQ(0, bid);
}

TEST(ServerTest, RegisterBranch_WithServiceAndRegion) { 
  server::Server server;

  request::Request * request = server.registerRequest(RID);

  long bid = server.registerBranch(request, "s", "r");
  ASSERT_EQ(0, bid);
}

TEST(ServerTest, CloseBranch) { 
  server::Server server;

  request::Request * request = server.registerRequest(RID);
  
  long bid = server.registerBranch(request, "s", "r");

  int status = server.closeBranch(request, bid);
  ASSERT_EQ(OK, status);
}

TEST(ServerTest, CloseBranch_WithInvalidBid) { 
  server::Server server;

  request::Request * request = server.registerRequest(RID);
  
  long bid = server.registerBranch(request, "s", "r");

  int status = server.closeBranch(request, 9);
  ASSERT_EQ(INVALID_BRANCH, status);
}

TEST(ServerTest, CheckRequest_AllContexts) { 
  server::Server server;
  int status;

  /* Register Request and Branches with Multiple Contexts */

  request::Request * request = server.registerRequest(RID);
  
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

  server.closeBranch(request, 0);
  status = server.checkRequest(request, "", "");
  ASSERT_EQ(OPENED, status);

  /* Close branches with service 's' and verify request is closed for that service */

  server.closeBranch(request, 1);
  server.closeBranch(request, 3);
  status = server.checkRequest(request, "s", "");
  ASSERT_EQ(CLOSED, status);

  /* Remaining checks */

  status = server.checkRequest(request, "", "r");
  ASSERT_EQ(OPENED, status);

  server.closeBranch(request, 2);
  status = server.checkRequest(request, "s", "r");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "r");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "");
  ASSERT_EQ(CLOSED, status);
}

TEST(ServerTest, CheckRequest_ContextNotFound) { 
  server::Server server;
  int status;

  request::Request * request = server.registerRequest(RID);
  
  server.registerBranch(request, "", ""); // bid = 0
  server.registerBranch(request, "s", ""); // bid = 1
  server.registerBranch(request, "", "r"); // bid = 2
  server.registerBranch(request, "s", "r"); // bid = 3

  status = server.checkRequest(request, "s1", "");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.checkRequest(request, "", "r1");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.checkRequest(request, "s1", "r1");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
}

TEST(ServerTest, CheckRequestByRegions_AllContexts) { 
  server::Server server;
  std::map<std::string, int> result;
  int status;

  request::Request * request = server.registerRequest(RID);
  
  server.registerBranch(request, "", ""); // bid = 0
  server.registerBranch(request, "s1", ""); // bid = 1
  server.registerBranch(request, "s2", ""); // bid = 2
  server.registerBranch(request, "", "r1"); // bid = 3
  server.registerBranch(request, "", "r2"); // bid = 4
  server.registerBranch(request, "s1", "r1"); // bid = 5
  server.registerBranch(request, "s2", "r1"); // bid = 6
  server.registerBranch(request, "s2", "r2"); // bid = 7

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_TRUE(result.size() == 2); // 2 opened regions: 'r1' and 'r2'
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(OPENED, result["r2"]);

  /* close all branches on 'r2' */
  server.closeBranch(request, 4);
  server.closeBranch(request, 7);

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_TRUE(result.size() == 2); // 2 regions: 'r1' (opened) and 'r2' (closed)
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);

  result = server.checkRequestByRegions(request, "s1" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_TRUE(result.size() == 1); // service 's1' only has region 'r1' which is opened
  ASSERT_TRUE(result.count("r1"));
  ASSERT_FALSE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);

  /* close remaining branches */
  server.closeBranch(request, 0);
  server.closeBranch(request, 1);
  server.closeBranch(request, 2);
  server.closeBranch(request, 3);
  server.closeBranch(request, 5);
  server.closeBranch(request, 6);

  /* request is now closed for every region */
  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(OK, status);
  ASSERT_TRUE(result.size() == 2);
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(CLOSED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);
  
}

TEST(ServerTest, CheckRequestByRegions_RegionAndContextNotFound) { 
  server::Server server;
  std::map<std::string, int> result;
  int status;

  request::Request * request = server.registerRequest(RID);
  

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(REGION_NOT_FOUND, status);

  server.registerBranch(request, "", ""); // bid = 0
  server.registerBranch(request, "s", ""); // bid = 1

  result = server.checkRequestByRegions(request, "" ,&status);
  ASSERT_EQ(REGION_NOT_FOUND, status);

  server.registerBranch(request, "", "r"); // bid = 2
  server.registerBranch(request, "s", "r"); // bid = 3

  result = server.checkRequestByRegions(request, "s1" ,&status);
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
}

// sanity check
TEST(ServerTest, PreventedInconsistencies_GetZeroValue) { 
  server::Server server;

  long value = server.getPreventedInconsistencies();
  ASSERT_EQ(0, value);
}