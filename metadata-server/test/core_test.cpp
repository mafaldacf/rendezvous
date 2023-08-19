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
const int CONTEXT_NOT_FOUND = 2;
const int INVALID_BRANCH_SERVICE = -2;
const int INVALID_BRANCH_REGION = -3;

const std::string SID = "eu-central-1";
const std::string RID = "myrequestid";
const std::string EMPTY_TAG = "";

std::string getRid(int id) {
  return SID + ':' + std::to_string(id);
}

// for closing branches
std::string getBid(int bid) {
  return SID + '_' + std::to_string(bid);
}
std::string parseFullBid(rendezvous::Server * server, metadata::Request * request, std::string bid, int bid_idx=-1) {
  // just a workaround for a quick sanity check!
  // getBid() computes the id which should match the one parsed by the server
  // we return "ERR" to force the test to fail if the following condition is not true (which should never happen if everything is ok)
  if (bid_idx != -1 && getBid(bid_idx) == server->parseFullBid(bid).first) {
    return server->parseFullBid(bid).first;
  }
  return "ERR";
}

// for register branches
std::string getFullBid(std::string rid, int id) {
  return SID + '_' + std::to_string(id) + ":" + rid;
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
  std::string bid = server.registerBranchRegion(request, "", "", EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
}

TEST(ServerTest, RegisterBranch_WithService) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "s", "", EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
}

TEST(ServerTest, RegisterBranch_WithRegion) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "", "r", EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
}

TEST(ServerTest, RegisterBranch_WithServiceAndRegion) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "s", "r", EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
}

TEST(ServerTest, RegisterBranch_DiffTags) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "service", "region", "tag_A");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
  bid = server.registerBranchRegion(request, "service", "region", "tag_B");
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid);
}

TEST(ServerTest, RegisterBranch_SameTags) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "service", "region", "tag_A");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
  bid = server.registerBranchRegion(request, "service", "region", "tag_A");
  // server returns empty when registration fails
  ASSERT_EQ("", bid);
}

TEST(ServerTest, CloseBranch) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "s", "r", EMPTY_TAG);
  int res = server.closeBranch(request, parseFullBid(&server, request, bid, 0), "r");
  ASSERT_EQ(1, res);
}

TEST(ServerTest, CloseBranchInvalidRegion) {
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "service", "eu-central-1", EMPTY_TAG);
  int res = server.closeBranch(request, parseFullBid(&server, request, bid, 0), "us-east-1");
  ASSERT_EQ(-1, res);
}

TEST(ServerTest, CloseBranchInvalidBid) {
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "service", "eu-central-1", EMPTY_TAG);
  int res = server.closeBranch(request, "invalid bid", "us-east-1");
  ASSERT_EQ(0, res);
}

TEST(ServerTest, CheckRequest_AllContexts) { 
  rendezvous::Server server(SID); 
  int status;
  bool found_region = false;

  /* Register Request and Branches with Multiple Contexts */
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid_0 = server.registerBranchRegion(request, "", "", EMPTY_TAG); // bid = 0
  std::string bid_1 = server.registerBranchRegion(request, "s", "", EMPTY_TAG); // bid = 1
  std::string bid_2 = server.registerBranchRegion(request, "", "r", EMPTY_TAG); // bid = 2
  std::string bid_3 = server.registerBranchRegion(request, "s", "r", EMPTY_TAG); // bid = 3

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
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "", "");
  ASSERT_EQ(OPENED, status);

  /* Close branches with service 's' and verify request is closed for that service */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_3, 3), "r"); // bid 3
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "s", "");
  ASSERT_EQ(CLOSED, status);

  /* Remaining checks */
  status = server.checkRequest(request, "", "r");
  ASSERT_EQ(OPENED, status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), "r"); // bid 2
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
  std::string bid_0 = server.registerBranch(request, "notifications", regionsNoService, EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid_0);

  google::protobuf::RepeatedPtrField<std::string> regionsService;
  regionsService.Add("EU");
  regionsService.Add("US");
  std::string bid_1 = server.registerBranch(request, "post-storage", regionsService, EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid_1);

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
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "EU"); // post-storage
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "notifications", "EU");
  ASSERT_EQ(CLOSED, status);
  status = server.checkRequest(request, "notifications", "");
  ASSERT_EQ(OPENED, status);

  /* more branches for 'post-storage' service */
  google::protobuf::RepeatedPtrField<std::string> regionsService2;
  regionsService2.Add("CH");
  regionsService2.Add("EU");
  std::string bid_2 = server.registerBranch(request, "post-storage", regionsService2, EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 2), bid_2);

  /* 'post-storage' service verifications */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), "EU"); // notifications
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), "US"); // notifications
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "post-storage", "");
  ASSERT_EQ(OPENED, status);
  status = server.checkRequest(request, "post-storage", "EU");
  ASSERT_EQ(OPENED, status);
  status = server.checkRequest(request, "post-storage", "CH");
  ASSERT_EQ(OPENED, status);
  status = server.checkRequest(request, "post-storage", "US");
  ASSERT_EQ(CLOSED, status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), "CH"); // notifications
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "post-storage", "CH");
  ASSERT_EQ(CLOSED, status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), "EU"); // notifications
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "post-storage", "EU");
  ASSERT_EQ(CLOSED, status);

  /* global verifications */
  status = server.checkRequest(request, "post-notifications", "");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
  status = server.checkRequest(request, "", "EU");
  ASSERT_EQ(CLOSED, status);
  status = server.checkRequest(request, "", "US");
  ASSERT_EQ(OPENED, status);

  /* remaining 'notifications' service verifications */
  status = server.checkRequest(request, "notifications", "US");
  ASSERT_EQ(OPENED, status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "US"); // post-storage
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
  
  server.registerBranchRegion(request, "", "", EMPTY_TAG); // bid = 0
  server.registerBranchRegion(request, "s", "", EMPTY_TAG); // bid = 1
  server.registerBranchRegion(request, "", "r", EMPTY_TAG); // bid = 2
  server.registerBranchRegion(request, "s", "r", EMPTY_TAG); // bid = 3

  status = server.checkRequest(request, "s1", "");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
  status = server.checkRequest(request, "", "r1");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
  status = server.checkRequest(request, "s1", "r1");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
}

TEST(ServerTest, CheckRequestByRegions_AllContexts_SetOfBranches) { 
  rendezvous::Server server(SID); 
  std::map<std::string, int> result;
  std::string bid;
  bool found_region = false;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  google::protobuf::RepeatedPtrField<std::string> regionsNoService;
  regionsNoService.Add("");
  regionsNoService.Add("r1");
  regionsNoService.Add("r2");
  std::string bid_0 = server.registerBranch(request, "", regionsNoService, EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid_0);

  google::protobuf::RepeatedPtrField<std::string> regionsService1;
  regionsService1.Add("");
  regionsService1.Add("r1");
  std::string bid_1 = server.registerBranch(request, "s1", regionsService1, EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid_1);

  google::protobuf::RepeatedPtrField<std::string> regionsService2;
  regionsService2.Add("");
  regionsService2.Add("r1");
  regionsService2.Add("r2");
  std::string bid_2 = server.registerBranch(request, "s2", regionsService2, EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 2), bid_2);

  result = server.checkRequestByRegions(request, "");
  ASSERT_TRUE(result.size() == 2); // 2 opened regions: 'r1' and 'r2'
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(OPENED, result["r2"]);

  /* close all branches on 'r2' */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "r2");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), "r2");
  ASSERT_EQ(true, found_region);

  result = server.checkRequestByRegions(request, "");
  ASSERT_EQ(2, result.size()); // 2 regions: 'r1' (opened) and 'r2' (closed)
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);

  result = server.checkRequestByRegions(request, "s1");
  ASSERT_EQ(1, result.size()); // service 's1' only has region 'r1' which is opened
  ASSERT_TRUE(result.count("r1"));
  ASSERT_FALSE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);

  /* close remaining branches */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "r1");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), "");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), "r1");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), "r1");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), "");
  ASSERT_EQ(true, found_region);

  /* request is now closed for every region */
  result = server.checkRequestByRegions(request, "");
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
  bool found_region = false;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  std::string bid_0 = server.registerBranchRegion(request, "", "", EMPTY_TAG); // bid = 0
  std::string bid_1 = server.registerBranchRegion(request, "s1", "", EMPTY_TAG); // bid = 1
  std::string bid_2 = server.registerBranchRegion(request, "s2", "", EMPTY_TAG); // bid = 2
  std::string bid_3 = server.registerBranchRegion(request, "", "r1", EMPTY_TAG); // bid = 3
  std::string bid_4 = server.registerBranchRegion(request, "", "r2", EMPTY_TAG); // bid = 4
  std::string bid_5 = server.registerBranchRegion(request, "s1", "r1", EMPTY_TAG); // bid = 5
  std::string bid_6 = server.registerBranchRegion(request, "s2", "r1", EMPTY_TAG); // bid = 6
  std::string bid_7 = server.registerBranchRegion(request, "s2", "r2", EMPTY_TAG); // bid = 7

  result = server.checkRequestByRegions(request, "");
  ASSERT_TRUE(result.size() == 2); // 2 opened regions: 'r1' and 'r2'
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(OPENED, result["r2"]);

  /* close all branches on 'r2' */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_4, 4), "r2"); // bid 4
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_7, 7), "r2"); // bid 7
  ASSERT_EQ(true, found_region);

  result = server.checkRequestByRegions(request, "");
  ASSERT_EQ(2, result.size()); // 2 regions: 'r1' (opened) and 'r2' (closed)
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);

  result = server.checkRequestByRegions(request, "s1");
  ASSERT_EQ(1, result.size()); // service 's1' only has region 'r1' which is opened
  ASSERT_TRUE(result.count("r1"));
  ASSERT_FALSE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);

  /* close remaining branches */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), ""); // bid 2
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_3, 3), "r1"); // bid 3
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_5, 5), "r1"); // bid 5
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_6, 6), "r1"); // bid 6
  ASSERT_EQ(true, found_region);

  /* request is now closed for every region */
  result = server.checkRequestByRegions(request, "");
  ASSERT_EQ(2, result.size());
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(CLOSED, result["r1"]);
  ASSERT_EQ(CLOSED, result["r2"]);
}

TEST(ServerTest, CheckRequestByRegions_RegionAndContextNotFound) { 
  rendezvous::Server server(SID); 
  std::map<std::string, int> result;
  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  result = server.checkRequestByRegions(request, "");
  ASSERT_EQ(0, result.size());

  server.registerBranchRegion(request, "", "", EMPTY_TAG); // bid = 0
  server.registerBranchRegion(request, "s", "", EMPTY_TAG); // bid = 1

  result = server.checkRequestByRegions(request, "");
  ASSERT_EQ(0, result.size());

  server.registerBranchRegion(request, "", "r", EMPTY_TAG); // bid = 2
  server.registerBranchRegion(request, "s", "r", EMPTY_TAG); // bid = 3
  result = server.checkRequestByRegions(request, "s1");
}

// sanity check
TEST(ServerTest, PreventedInconsistencies_GetZeroValue) { 
  rendezvous::Server server(SID); 
  long value = server._prevented_inconsistencies.load();
  ASSERT_EQ(0, value);
}