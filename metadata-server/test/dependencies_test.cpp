#include "../src/server.h"
#include "../src/metadata/request.h"
#include "gtest/gtest.h"
#include <thread>
#include <string>
#include "utils.h"

// -----------------
// DEPENDENCIES TEST
// -----------------

TEST(ServerConcurrencyTest, WaitServicePostNotificationSample) {
  rendezvous::Server server(SID);
  std::vector<std::thread> threads;
  std::string bid;
  metadata::Request * request = server.getOrRegisterRequest(RID);

  utils::ProtoVec regions_0;
  bid = server.registerBranch(request, "post_notification", regions_0, "", "");
  ASSERT_EQ(getFullBid(request->getRid(), 0), bid);

  utils::ProtoVec regions_1;
  regions_1.Add("EU");
  regions_1.Add("US");
  bid = server.registerBranch(request, "post_notification", regions_1, "write_post", "");
  ASSERT_EQ(getFullBid(request->getRid(), 1), bid);

  utils::ProtoVec regions_2;
  regions_2.Add("AP");
  bid = server.registerBranch(request, "analytics", regions_2, "", "post_notification");
  ASSERT_EQ(getFullBid(request->getRid(), 2), bid);

  sleep(1);

  // TOTAL WAIT
  threads.emplace_back([&server, request] {
    int status = server.wait(request, "post_notification", "", "");
    ASSERT_EQ(1, status);
  });

  sleep(1);

  int r = server.closeBranch(request, getBid(0), ""); // post_notification
  ASSERT_EQ(1, r);
  r = server.closeBranch(request, getBid(1), "EU"); // post_notification (write post)
  ASSERT_EQ(1, r);
  r = server.closeBranch(request, getBid(1), "US"); // post_notification (write post)
  ASSERT_EQ(1, r);
  r = server.closeBranch(request, getBid(2), "AP"); // post_notification (write post)
  ASSERT_EQ(1, r);

  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }
}
