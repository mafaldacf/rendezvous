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
  std::string bid_0 = server.registerBranch(request, ROOT_SUB_RID, "post_storage", regions_0, "", "");
  ASSERT_EQ(getBid(0), bid_0);

  utils::ProtoVec regions_1;
  regions_1.Add("EU");
  regions_1.Add("US");

  std::string bid_1 = server.registerBranch(request, ROOT_SUB_RID, "post_storage", regions_1, "write_post", "");
  ASSERT_EQ(getBid(1), bid_1);
  
  utils::Status r = server.checkStatus(request, ROOT_SUB_RID, "post_storage", "", "", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);

  int found = server.closeBranch(request, bid_1, "EU");
  ASSERT_EQ(1, found);

  r = server.checkStatus(request, ROOT_SUB_RID, "post_storage", "EU", "", true);
  ASSERT_EQ(CLOSED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(CLOSED, r.tagged["write_post"]);

  r = server.checkStatus(request, ROOT_SUB_RID, "post_storage", "US", "", true);
  ASSERT_EQ(OPENED, r.status);
  ASSERT_EQ(1, r.tagged.size());
  ASSERT_EQ(1, r.tagged.count("write_post"));
  ASSERT_EQ(OPENED, r.tagged["write_post"]);
}