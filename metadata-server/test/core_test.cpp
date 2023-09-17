#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>
#include "utils.h"

// ---------
// CORE TEST
// ---------

TEST(CoreTest, getOrRegisterRequest_WithNoRid) { 
  rendezvous::Server server(SID);
  metadata::Request * request = server.getOrRegisterRequest(getRid(0));
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ(getRid(0), request->getRid());
  metadata::Request * request2 = server.getRequest(getRid(0));
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(CoreTest, getOrRegisterRequest_WithRid) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  ASSERT_TRUE(request != nullptr);
  ASSERT_EQ(RID, request->getRid());
  metadata::Request * request2 = server.getRequest(RID);
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request2->getRid());
}

TEST(CoreTest, GetRequest_InvalidRID) {
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getRequest("invalidRID");
  ASSERT_TRUE(request == nullptr);
}

TEST(CoreTest, GetOrRegisterAndGetRequest) {
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  metadata::Request * request2 = server.getRequest(RID);
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(CoreTest, GetOrRegisterTwiceRequest) {
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  metadata::Request * request2 = server.getOrRegisterRequest(RID);
  ASSERT_TRUE(request2 != nullptr);
  ASSERT_TRUE(request == request2);
  ASSERT_EQ(request->getRid(), request->getRid());
}

TEST(CoreTest, RegisterBranch_WithService) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "s", "", EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
}

TEST(CoreTest, RegisterBranch_WithServiceAndRegion) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "s", "r", EMPTY_TAG);
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
}

TEST(CoreTest, RegisterBranch_DiffTags) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "service", "region", "tag_A");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
  bid = server.registerBranchRegion(request, "service", "region", "tag_B");
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid);
}

TEST(CoreTest, RegisterBranch_SameTags) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "service", "region", "tag_A");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);
  bid = server.registerBranchRegion(request, "service", "region", "tag_A");
  // server returns empty when registration fails
  ASSERT_EQ("", bid);
}

TEST(CoreTest, CloseBranch) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "s", "r", EMPTY_TAG);
  int res = server.closeBranch(request, parseFullBid(&server, request, bid, 0), "r");
  ASSERT_EQ(1, res);
}

TEST(CoreTest, CloseBranchInvalidRegion) {
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "service", "eu-central-1", EMPTY_TAG);
  int res = server.closeBranch(request, parseFullBid(&server, request, bid, 0), "us-east-1");
  ASSERT_EQ(-1, res);
}

TEST(CoreTest, CloseBranchInvalidBid) {
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid = server.registerBranchRegion(request, "service", "eu-central-1", EMPTY_TAG);
  int res = server.closeBranch(request, "invalid bid", "us-east-1");
  ASSERT_EQ(0, res);
}

TEST(CoreTest, CheckRequest_AllContexts) { 
  rendezvous::Server server(SID); 
  utils::Status res;
  bool found_region = false;

  /* Register Request and Branches with Multiple Contexts */
  metadata::Request * request = server.getOrRegisterRequest(RID);
  std::string bid_0 = server.registerBranchRegion(request, "s1", "", EMPTY_TAG); // bid = 0
  std::string bid_1 = server.registerBranchRegion(request, "s2", "", EMPTY_TAG); // bid = 1
  std::string bid_2 = server.registerBranchRegion(request, "s1", "r", EMPTY_TAG); // bid = 2
  std::string bid_3 = server.registerBranchRegion(request, "s2", "r", EMPTY_TAG); // bid = 3
  std::string bid_4 = server.registerBranchRegion(request, "emplastro", "region", EMPTY_TAG); // bid = 4

  /* Check Request for Multiple Contexts */
  res = server.checkStatus(request, "s1", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "s2", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "s1", "r");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "s1", "r");
  ASSERT_EQ(OPENED, res.status);

  /* Close branch with no context and verify request is still opened */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, "s1", "");
  ASSERT_EQ(OPENED, res.status);

  /* Close branches with service 's' and verify request is closed for that service */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_3, 3), "r"); // bid 3
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, "s2", "");
  ASSERT_EQ(CLOSED, res.status);

  /* Remaining checks */
  res = server.checkStatus(request, "s1", "r");
  ASSERT_EQ(OPENED, res.status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), "r"); // bid 2
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, "s2", "r");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, "s1", "r");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, "s1", "");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, "", "");
  ASSERT_EQ(OPENED, res.status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_4, 4), "region"); // bid 4
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, "", "");
  ASSERT_EQ(CLOSED, res.status);
}

TEST(CoreTest, CheckRequest_AllContexts_MultipleServices_SetsOfBranches) { 
  rendezvous::Server server(SID); 
    
  int status;
  bool found_region = false;
  std::string bid = "";
  utils::Status res;

  /* Register Request and Branches with Multiple Contexts */
  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  google::protobuf::RepeatedPtrField<std::string> regionsNoService;
  regionsNoService.Add("EU");
  regionsNoService.Add("US");
  std::string bid_0 = server.registerBranch(request, "notifications", regionsNoService, EMPTY_TAG, "");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid_0);

  google::protobuf::RepeatedPtrField<std::string> regionsService;
  regionsService.Add("EU");
  regionsService.Add("US");
  std::string bid_1 = server.registerBranch(request, "post-storage", regionsService, EMPTY_TAG, "");
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid_1);

  /* multiple services verifications */
  res = server.checkStatus(request, "post-storage", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "post-storage", "EU");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "post-storage", "US");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "notifications", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "notifications", "EU");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "notifications", "US");
  ASSERT_EQ(OPENED, res.status);

  /* 'notifications' service verifications */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "EU"); // post-storage
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, "notifications", "EU");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, "notifications", "");
  ASSERT_EQ(OPENED, res.status);

  /* more branches for 'post-storage' service */
  google::protobuf::RepeatedPtrField<std::string> regionsService2;
  regionsService2.Add("CH");
  regionsService2.Add("EU");
  std::string bid_2 = server.registerBranch(request, "post-storage", regionsService2, EMPTY_TAG, "");
  ASSERT_EQ(getFullBid(request->getRid(), 2), bid_2);

  /* 'post-storage' service verifications */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), "EU"); // notifications
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), "US"); // notifications
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, "post-storage", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "post-storage", "EU");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "post-storage", "CH");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "post-storage", "US");
  ASSERT_EQ(CLOSED, res.status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), "CH"); // notifications
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, "post-storage", "CH");
  ASSERT_EQ(CLOSED, res.status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_2, 2), "EU"); // notifications
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, "post-storage", "EU");
  ASSERT_EQ(CLOSED, res.status);

  /* global verifications */
  res = server.checkStatus(request, "post-notifications", "");
  ASSERT_EQ(UNKNOWN, res.status);
  res = server.checkStatus(request, "", "EU");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, "", "US");
  ASSERT_EQ(OPENED, res.status);

  /* remaining 'notifications' service verifications */
  res = server.checkStatus(request, "notifications", "US");
  ASSERT_EQ(OPENED, res.status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "US"); // post-storage
  ASSERT_EQ(true, found_region);

  /* remaining global verifications */
  res = server.checkStatus(request, "", "EU");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, "", "US");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, "", "");
  ASSERT_EQ(CLOSED, res.status);
}

TEST(CoreTest, CheckRequest_ContextNotFound) { 
  rendezvous::Server server(SID); 
  int status;
  utils::Status res;
  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  google::protobuf::RepeatedPtrField<std::string> regions;
  std::string bid_0 = server.registerBranch(request, "s1", regions, EMPTY_TAG, ""); // bid = 0
  std::string bid_1 = server.registerBranch(request, "s1", regions, EMPTY_TAG, ""); // bid = 1
  server.registerBranchRegion(request, "s1", "r", EMPTY_TAG); // bid = 2
  server.registerBranchRegion(request, "s2", "r", EMPTY_TAG); // bid = 3

  res = server.checkStatus(request, "invalid service", "");
  ASSERT_EQ(UNKNOWN, res.status);
  // EXCEPTION: although r1 does not exist, global branches are still opened!
  res = server.checkStatus(request, "", "invalid region");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, "invalid service", "invalid region");
  ASSERT_EQ(UNKNOWN, res.status);

  bool found = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "");
  ASSERT_EQ(true, found);
  found = server.closeBranch(request, parseFullBid(&server, request, bid_1, 1), "");
  ASSERT_EQ(true, found);

  // now we can ensure it is unknown since all global branches are closed!
  res = server.checkStatus(request, "", "invalid region");
  ASSERT_EQ(UNKNOWN, res.status);
}

// sanity check
TEST(CoreTest, PreventedInconsistencies_GetZeroValue) { 
  rendezvous::Server server(SID); 
  long value = server._prevented_inconsistencies.load();
  ASSERT_EQ(0, value);
}