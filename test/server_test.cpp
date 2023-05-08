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
const std::string TAG = "mytag";
const int _requests_collector_sleep_m = 30;
const int _subscribers_collector_sleep_m = 30;
const int _subscribers_max_wait_time_s = 60;
const int _wait_replica_timeout_s = 10;

std::string getRid(int id) {
  return SID + ':' + std::to_string(id);
}

std::string getBid(std::string rid, int id) {
  return SID + '_' + std::to_string(id) + ":" + rid;
}

TEST(ServerTest, getOrRegisterRequest_WithNoRid) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  metadata::Request * request = server.getOrRegisterRequest(getRid(0));
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ(getRid(0), request->getRid());

  metadata::Request * request2 = server.getRequest(getRid(0));
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, getOrRegisterRequest_WithRid) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ(RID, request->getRid());

  metadata::Request * request2 = server.getRequest(RID);
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request2->getRid());
}

TEST(ServerTest, GetRequest_InvalidRID) {
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s); 

  metadata::Request * request = server.getRequest("invalidRID");
  ASSERT_TRUE(request == nullptr);
}

TEST(ServerTest, GetOrRegisterAndGetRequest) {
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s); 

  metadata::Request * request = server.getOrRegisterRequest(RID);
  metadata::Request * request2 = server.getRequest(RID);

  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, GetOrRegisterTwiceRequest) {
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s); 

  metadata::Request * request = server.getOrRegisterRequest(RID);
  metadata::Request * request2 = server.getOrRegisterRequest(RID);

  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(ServerTest, RegisterBranch) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "", "", TAG);
  ASSERT_EQ(getBid(request->getRid(), 0), bid);
}

TEST(ServerTest, RegisterBranch_WithService) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "s", "", TAG);
  ASSERT_EQ(getBid(request->getRid(), 0), bid);
}

TEST(ServerTest, RegisterBranch_WithRegion) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "", "r", TAG);
  ASSERT_EQ(getBid(request->getRid(), 0), bid);
}

TEST(ServerTest, RegisterBranch_WithServiceAndRegion) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "s", "r", TAG);
  ASSERT_EQ(getBid(request->getRid(), 0), bid);
}

TEST(ServerTest, CloseBranch) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  std::string bid = server.registerBranch(request, "s", "r", TAG);

  bool found_region =server.closeBranch(request, getBid(request->getRid(), 0), "r");
  ASSERT_EQ(true, found_region);
}

TEST(ServerTest, CloseBranchInvalidRegion) {
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "service", "eu-central-1", TAG);

  int res = server.closeBranch(request, getBid(request->getRid(), 0), "us-east-1");
  ASSERT_EQ(-1, res);
}

TEST(ServerTest, CloseBranchInvalidBid) {
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  metadata::Request * request = server.getOrRegisterRequest(RID);

  std::string bid = server.registerBranch(request, "service", "eu-central-1", TAG);

  int res = server.closeBranch(request, "invalid bid", "us-east-1");
  ASSERT_EQ(0, res);
}

TEST(ServerTest, CheckRequest_AllContexts) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);
  int status;
  bool found_region = false;

  /* Register Request and Branches with Multiple Contexts */

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  server.registerBranch(request, "", "", TAG); // bid = 0
  server.registerBranch(request, "s", "", TAG); // bid = 1
  server.registerBranch(request, "", "r", TAG); // bid = 2
  server.registerBranch(request, "s", "r", TAG); // bid = 3

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

  found_region = server.closeBranch(request, getBid(request->getRid(), 0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "", "");
  ASSERT_EQ(OPENED, status);

  /* Close branches with service 's' and verify request is closed for that service */

  found_region = server.closeBranch(request, getBid(request->getRid(), 1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 3), "r"); // bid 3
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "s", "");
  ASSERT_EQ(CLOSED, status);

  /* Remaining checks */

  status = server.checkRequest(request, "", "r");
  ASSERT_EQ(OPENED, status);

  found_region = server.closeBranch(request, getBid(request->getRid(), 2), "r"); // bid 2
  ASSERT_EQ(true, found_region);
  status = server.checkRequest(request, "s", "r");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "r");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "", "");
  ASSERT_EQ(CLOSED, status);
}

TEST(ServerTest, CheckRequest_AllContexts_MultipleServices_SetsOfBranches) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);
  int status;
  bool found_region = false;
  std::string bid = "";

  /* Register Request and Branches with Multiple Contexts */

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  google::protobuf::RepeatedPtrField<std::string> regionsNoService;
  regionsNoService.Add("EU");
  regionsNoService.Add("US");
  bid = server.registerBranches(request, "notifications", regionsNoService, TAG);
  ASSERT_EQ(getBid(request->getRid(), 0), bid);

  google::protobuf::RepeatedPtrField<std::string> regionsService;
  regionsService.Add("EU");
  regionsService.Add("US");
  bid = server.registerBranches(request, "post-storage", regionsService, TAG);
  ASSERT_EQ(getBid(request->getRid(), 1), bid);

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

  found_region = server.closeBranch(request, getBid(request->getRid(), 0), "EU"); // post-storage
  ASSERT_EQ(true, found_region);

  status = server.checkRequest(request, "notifications", "EU");
  ASSERT_EQ(CLOSED, status);

  status = server.checkRequest(request, "notifications", "");
  ASSERT_EQ(OPENED, status);

  /* more branches for 'post-storage' service */

  google::protobuf::RepeatedPtrField<std::string> regionsService2;
  regionsService2.Add("CH");
  regionsService2.Add("EU");
  bid = server.registerBranches(request, "post-storage", regionsService2, TAG);
  ASSERT_EQ(getBid(request->getRid(), 2), bid);

  /* 'post-storage' service verifications */

  found_region = server.closeBranch(request, getBid(request->getRid(), 1), "EU"); // notifications
  ASSERT_EQ(true, found_region);

  found_region = server.closeBranch(request, getBid(request->getRid(), 1), "US"); // notifications
  ASSERT_EQ(true, found_region);

  status = server.checkRequest(request, "post-storage", "");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "post-storage", "EU");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "post-storage", "CH");
  ASSERT_EQ(OPENED, status);

  status = server.checkRequest(request, "post-storage", "US");
  ASSERT_EQ(CLOSED, status);

  found_region = server.closeBranch(request, getBid(request->getRid(), 2), "CH"); // notifications
  ASSERT_EQ(true, found_region);

  status = server.checkRequest(request, "post-storage", "CH");
  ASSERT_EQ(CLOSED, status);

  found_region = server.closeBranch(request, getBid(request->getRid(), 2), "EU"); // notifications
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

  found_region = server.closeBranch(request, getBid(request->getRid(), 0), "US"); // post-storage
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
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);
  int status;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  server.registerBranch(request, "", "", TAG); // bid = 0
  server.registerBranch(request, "s", "", TAG); // bid = 1
  server.registerBranch(request, "", "r", TAG); // bid = 2
  server.registerBranch(request, "s", "r", TAG); // bid = 3

  status = server.checkRequest(request, "s1", "");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.checkRequest(request, "", "r1");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);

  status = server.checkRequest(request, "s1", "r1");
  ASSERT_EQ(CONTEXT_NOT_FOUND, status);
}

TEST(ServerTest, CheckRequestByRegions_AllContexts_SetOfBranches) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);
  std::map<std::string, int> result;
  std::string bid;
  bool found_region = false;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  google::protobuf::RepeatedPtrField<std::string> regionsNoService;
  regionsNoService.Add("");
  regionsNoService.Add("r1");
  regionsNoService.Add("r2");
  bid = server.registerBranches(request, "", regionsNoService, TAG);
  ASSERT_EQ(getBid(request->getRid(), 0), bid);

  google::protobuf::RepeatedPtrField<std::string> regionsService1;
  regionsService1.Add("");
  regionsService1.Add("r1");
  bid = server.registerBranches(request, "s1", regionsService1, TAG);
  ASSERT_EQ(getBid(request->getRid(), 1), bid);

  google::protobuf::RepeatedPtrField<std::string> regionsService2;
  regionsService2.Add("");
  regionsService2.Add("r1");
  regionsService2.Add("r2");
  bid = server.registerBranches(request, "s2", regionsService2, TAG);
  ASSERT_EQ(getBid(request->getRid(), 2), bid);

  result = server.checkRequestByRegions(request, "");
  ASSERT_TRUE(result.size() == 2); // 2 opened regions: 'r1' and 'r2'
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(OPENED, result["r2"]);

  /* close all branches on 'r2' */
  found_region = server.closeBranch(request, getBid(request->getRid(), 0), "r2");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 2), "r2");
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
  found_region = server.closeBranch(request, getBid(request->getRid(), 0), "");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 0), "r1");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 1), "");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 1), "r1");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 2), "r1");
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 2), "");
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
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);
  std::map<std::string, int> result;
  std::string bid;
  bool found_region = false;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  bid = server.registerBranch(request, "", "", TAG); // bid = 0
  bid = server.registerBranch(request, "s1", "", TAG); // bid = 1
  bid = server.registerBranch(request, "s2", "", TAG); // bid = 2
  bid = server.registerBranch(request, "", "r1", TAG); // bid = 3
  bid = server.registerBranch(request, "", "r2", TAG); // bid = 4
  bid = server.registerBranch(request, "s1", "r1", TAG); // bid = 5
  bid = server.registerBranch(request, "s2", "r1", TAG); // bid = 6
  bid = server.registerBranch(request, "s2", "r2", TAG); // bid = 7

  result = server.checkRequestByRegions(request, "");
  ASSERT_TRUE(result.size() == 2); // 2 opened regions: 'r1' and 'r2'
  ASSERT_TRUE(result.count("r1"));
  ASSERT_TRUE(result.count("r2"));
  ASSERT_EQ(OPENED, result["r1"]);
  ASSERT_EQ(OPENED, result["r2"]);

  /* close all branches on 'r2' */
  found_region = server.closeBranch(request, getBid(request->getRid(), 4), "r2"); // bid 4
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 7), "r2"); // bid 7
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
  found_region = server.closeBranch(request, getBid(request->getRid(), 0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 2), ""); // bid 2
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 3), "r1"); // bid 3
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 5), "r1"); // bid 5
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, getBid(request->getRid(), 6), "r1"); // bid 6
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
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);
  std::map<std::string, int> result;

  metadata::Request * request = server.getOrRegisterRequest(RID);
  

  result = server.checkRequestByRegions(request, "");
  ASSERT_EQ(0, result.size());

  server.registerBranch(request, "", "", TAG); // bid = 0
  server.registerBranch(request, "s", "", TAG); // bid = 1

  result = server.checkRequestByRegions(request, "");
  ASSERT_EQ(0, result.size());

  server.registerBranch(request, "", "r", TAG); // bid = 2
  server.registerBranch(request, "s", "r", TAG); // bid = 3

  result = server.checkRequestByRegions(request, "s1");
}

// sanity check
TEST(ServerTest, PreventedInconsistencies_GetZeroValue) { 
  rendezvous::Server server(SID, _requests_collector_sleep_m, 
    _subscribers_collector_sleep_m, _subscribers_max_wait_time_s, _wait_replica_timeout_s);

  long value = server.getNumPreventedInconsistencies();
  ASSERT_EQ(0, value);
}