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
  utils::ProtoVec regions;
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "s", regions, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);
}

TEST(CoreTest, RegisterBranch_WithServiceAndRegion) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions;
  regions.Add("r");
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "s", regions, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);
}

TEST(CoreTest, RegisterBranch_DiffTags) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions;
  regions.Add("region");

  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "service", regions, "tag_A", "");
  ASSERT_EQ(getBid(0), bid_0);

  std::string bid_1 = server.registerBranch(request, ROOT_SUB_RID, "service", regions, "tag_B", "");
  ASSERT_EQ(getBid(1), bid_1);
}

TEST(CoreTest, RegisterBranch_SameTags) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions;
  regions.Add("region");
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "service", regions, "tag_A", "");
  ASSERT_EQ(getBid(0), bid_0);

  // in this version Rendezvous allows duplicate tags!!
  std::string bid_1 = server.registerBranch(request, ROOT_SUB_RID, "service", regions, "tag_A", "");
  ASSERT_EQ(getBid(1), bid_1);
}

TEST(CoreTest, CloseBranch) { 
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions;
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "s", regions, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);

  int res = server.closeBranch(request, bid_0, "");
  ASSERT_EQ(1, res);
}

TEST(CoreTest, CloseBranchInvalidRegion) {
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions;
  regions.Add("eu-central-1");
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "service", regions, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);

  int res = server.closeBranch(request, bid_0, "us-east-1");
  ASSERT_EQ(-1, res);
}

TEST(CoreTest, CloseBranchInvalidBid) {
  rendezvous::Server server(SID); 
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions;
  regions.Add("eu-central-1");
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "service", regions, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);

  int res = server.closeBranch(request, "invalid bid", "us-east-1");
  ASSERT_EQ(-1, res);
}

TEST(CoreTest, CheckRequest_AllContexts) { 
  rendezvous::Server server(SID); 
  utils::Status res;
  bool found_region = false;

  utils::ProtoVec regions_empty;
  utils::ProtoVec regions_r;
  utils::ProtoVec regions_region;
  regions_r.Add("r");
  regions_region.Add("region");

  /* Register Request and Branches with Multiple Contexts */
  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "s1", regions_empty, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);
  std::string bid_1 = server.registerBranch(request, ROOT_SUB_RID, "s2", regions_empty, EMPTY_TAG, "");
  ASSERT_EQ(getBid(1), bid_1);

  std::string bid_2 = server.registerBranch(request, ROOT_SUB_RID, "s1", regions_r, EMPTY_TAG, "");
  ASSERT_EQ(getBid(2), bid_2);
  std::string bid_3 = server.registerBranch(request, ROOT_SUB_RID, "s2", regions_r, EMPTY_TAG, "");
  ASSERT_EQ(getBid(3), bid_3);

  std::string bid_4 = server.registerBranch(request, ROOT_SUB_RID, "emplasto", regions_region, EMPTY_TAG, "");
  ASSERT_EQ(getBid(4), bid_4);

  /* Check Request for Multiple Contexts */
  res = server.checkStatus(request, ROOT_SUB_RID, "s1", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "s2", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "s1", "r");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "s1", "r");
  ASSERT_EQ(OPENED, res.status);

  /* Close branch with no context and verify request is still opened */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), ""); // bid 0
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, ROOT_SUB_RID, "s1", "");
  ASSERT_EQ(OPENED, res.status);

  /* Close branches with service 's' and verify request is closed for that service */
  found_region = server.closeBranch(request, bid_1, ""); // bid 1
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, bid_3, "r"); // bid 3
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, ROOT_SUB_RID, "s2", "");
  ASSERT_EQ(CLOSED, res.status);

  /* Remaining checks */
  res = server.checkStatus(request, ROOT_SUB_RID, "s1", "r");
  ASSERT_EQ(OPENED, res.status);
  found_region = server.closeBranch(request, bid_2, "r"); // bid 2
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, ROOT_SUB_RID, "s2", "r");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "s1", "r");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "s1", "");
  ASSERT_EQ(CLOSED, res.status);

  // ASYNC ZONE CORNER CASE: we ignore everything in the current subrequest (num > 1)
  // but in fact, regions are still opened!
  res = server.checkStatus(request, ROOT_SUB_RID, "", "");
  ASSERT_EQ(CLOSED, res.status);

  found_region = server.closeBranch(request, bid_4, "region"); // bid 4
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, ROOT_SUB_RID, "", "");
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
  
  utils::ProtoVec regionsNoService;
  regionsNoService.Add("EU");
  regionsNoService.Add("US");
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "notifications", regionsNoService, EMPTY_TAG, "");
  ASSERT_EQ(getBid(0), bid_0);

  utils::ProtoVec regionsService;
  regionsService.Add("EU");
  regionsService.Add("US");
  std::string bid_1 = server.registerBranch(request, ROOT_SUB_RID, "post-storage", regionsService, EMPTY_TAG, "");
  ASSERT_EQ(getBid(1), bid_1);

  /* multiple services verifications */
  res = server.checkStatus(request, ROOT_SUB_RID, "post-storage", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "post-storage", "EU");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "post-storage", "US");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "notifications", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "notifications", "EU");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "notifications", "US");
  ASSERT_EQ(OPENED, res.status);

  /* 'notifications' service verifications */
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "EU"); // post-storage
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, ROOT_SUB_RID, "notifications", "EU");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "notifications", "");
  ASSERT_EQ(OPENED, res.status);

  /* more branches for 'post-storage' service */
  utils::ProtoVec regionsService2;
  regionsService2.Add("CH");
  regionsService2.Add("EU");
  std::string bid_2 = server.registerBranch(request, ROOT_SUB_RID, "post-storage", regionsService2, EMPTY_TAG, "");
  ASSERT_EQ(getBid(2), bid_2);

  /* 'post-storage' service verifications */
  found_region = server.closeBranch(request, bid_1, "EU"); // notifications
  ASSERT_EQ(true, found_region);
  found_region = server.closeBranch(request, bid_1, "US"); // notifications
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, ROOT_SUB_RID, "post-storage", "");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "post-storage", "EU");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "post-storage", "CH");
  ASSERT_EQ(OPENED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "post-storage", "US");
  ASSERT_EQ(CLOSED, res.status);
  found_region = server.closeBranch(request, bid_2, "CH"); // notifications
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, ROOT_SUB_RID, "post-storage", "CH");
  ASSERT_EQ(CLOSED, res.status);
  found_region = server.closeBranch(request, bid_2, "EU"); // notifications
  ASSERT_EQ(true, found_region);
  res = server.checkStatus(request, ROOT_SUB_RID, "post-storage", "EU");
  ASSERT_EQ(CLOSED, res.status);

  /* global verifications */
  res = server.checkStatus(request, ROOT_SUB_RID, "post-notifications", "");
  ASSERT_EQ(UNKNOWN, res.status);

  // ASYNC ZONE CORNER CASE: we ignore everything in the current subrequest (num > 1)
  // but in fact, these regions are opened!
  res = server.checkStatus(request, ROOT_SUB_RID, "", "EU");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "", "US");
  ASSERT_EQ(CLOSED, res.status);

  /* remaining 'notifications' service verifications */
  res = server.checkStatus(request, ROOT_SUB_RID, "notifications", "US");
  ASSERT_EQ(OPENED, res.status);
  found_region = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "US"); // post-storage
  ASSERT_EQ(true, found_region);

  /* remaining global verifications */
  res = server.checkStatus(request, ROOT_SUB_RID, "", "EU");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "", "US");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "", "");
  ASSERT_EQ(CLOSED, res.status);

  /* remaining global verifications (FURTHER CHECK) */
  // ASYNC ZONE CORNER CASE: here, it only returns CLOSED if we have a branch that
  // encompasses the checkStatus, since it always considers num opened branches > 1
  utils::ProtoVec emptyRegion;
  std::string current_bid = server.registerBranch(request, ROOT_SUB_RID, "dummy-current-service", emptyRegion, EMPTY_TAG, "");
  res = server.checkStatus(request, ROOT_SUB_RID, "", "EU");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "", "US");
  ASSERT_EQ(CLOSED, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "", "");
  ASSERT_EQ(CLOSED, res.status);
}

TEST(CoreTest, CheckRequest_ContextNotFound) { 
  rendezvous::Server server(SID); 
  int status;
  utils::Status res;
  metadata::Request * request = server.getOrRegisterRequest(RID);
  
  utils::ProtoVec empty_region;
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "s1", empty_region, EMPTY_TAG, "");
  std::string bid_1 = server.registerBranch(request, ROOT_SUB_RID, "s1", empty_region, EMPTY_TAG, "");
  utils::ProtoVec region_r;
  region_r.Add("r");
  std::string bid_2 = server.registerBranch(request, ROOT_SUB_RID, "s1", empty_region, EMPTY_TAG, "");
  std::string bid_3 = server.registerBranch(request, ROOT_SUB_RID, "s2", empty_region, EMPTY_TAG, "");

  res = server.checkStatus(request, ROOT_SUB_RID, "invalid service", "");
  ASSERT_EQ(UNKNOWN, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "", "invalid region");
  ASSERT_EQ(UNKNOWN, res.status);
  res = server.checkStatus(request, ROOT_SUB_RID, "invalid service", "invalid region");
  ASSERT_EQ(UNKNOWN, res.status);

  bool found = server.closeBranch(request, parseFullBid(&server, request, bid_0, 0), "");
  ASSERT_EQ(true, found);
  found = server.closeBranch(request, bid_1, "");
  ASSERT_EQ(true, found);

  // now we can ensure it is unknown since all global branches are closed!
  res = server.checkStatus(request, ROOT_SUB_RID, "", "invalid region");
  ASSERT_EQ(UNKNOWN, res.status);
}

// sanity check
TEST(CoreTest, PreventedInconsistencies_GetZeroValue) { 
  rendezvous::Server server(SID); 
  long value = server._prevented_inconsistencies.load();
  ASSERT_EQ(0, value);
}