#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>
#include "utils.h"

// -----------------
// SERVICE TAGS TEST
// -----------------

TEST(ServiceTagsTest, CheckStatusDetailed_Tags) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid_0);

  utils::ProtoVec regions_1;
  regions_1.Add("EU");
  regions_1.Add("US");

  std::string bid_1 = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_1, "write_post", "");
  ASSERT_EQ(getBid(1), bid_1);

  request->insertACSL(DUMMY_ACSL);
  
  utils::Status r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);

  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "EU", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);

  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "US", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);

  int found = server.closeBranch(request, bid_1, "EU");
  ASSERT_EQ(1, found);

  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "EU", true);
  ASSERT_EQ(CLOSED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(CLOSED, r.tagged["write_post"]);

  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "US", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);
}

TEST(ServiceTagsTest, CheckStatusDetailedDuplicateTag) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  request->insertACSL(DUMMY_ACSL);

  utils::ProtoVec regions_0;
  std::string bid_0 = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid_0);

  utils::ProtoVec regions_1;
  regions_1.Add("EU");
  regions_1.Add("AP");

  std::string bid_1 = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_1, "write_post", "");
  ASSERT_EQ(getBid(1), bid_1);

  utils::ProtoVec regions_2;
  regions_2.Add("AP");
  std::string bid_2 = server.registerBranchGTest(request, ROOT_SUB_RID, "post_storage", regions_2, "write_post", "");
  ASSERT_EQ(getBid(2), bid_2);
  
  utils::Status r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);

  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "EU", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);

  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "AP", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);

  int found = server.closeBranch(request, bid_1, "EU");
  ASSERT_EQ(1, found);

  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "EU", true);
  ASSERT_EQ(CLOSED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(CLOSED, r.tagged["write_post"]);

  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "AP", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);

  found = server.closeBranch(request, bid_1, "AP");
  ASSERT_EQ(1, found);

  // we still have one branch opened for the same tag!!!!
  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "AP", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);

  found = server.closeBranch(request, bid_2, "AP");
  ASSERT_EQ(1, found);

  // we still have one branch opened for the same tag!!!!
  r = server.checkStatus(request, DUMMY_ACSL, "post_storage", "AP", true);
  ASSERT_EQ(CLOSED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(CLOSED, r.tagged["write_post"]);
}
