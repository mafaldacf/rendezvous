#include "../src/server.h"
#include "gtest/gtest.h"


/* ------------------------------------

TEST BASIC MEMORY MANAGEMENT OF SERVER

--------------------------------------- */


/*
Max tested so far:
- NUM_REQUESTS = 100000
- NUM_BRANCHES = 1000
*/

const int NUM_REQUESTS = 1;
const int NUM_BRANCHES = 100;

const int OPENED = 0;
const int CLOSED = 1;
const int OK = 0;

const std::string RID = "myrequestid";

TEST(StressTest, MultipleStructures) { 
  server::Server server;

  std::cout << "-- registering " << NUM_REQUESTS << " requests and " << NUM_BRANCHES << " branches for each request --" << std::endl;
  for (int i = 0; i < NUM_REQUESTS; i++) {
    request::Request * request = server.registerRequest(RID);

    for (int j = 0; j < NUM_BRANCHES/4; j++) {
      server.registerBranch(request, "", "");
    }
    for (int j = NUM_BRANCHES/4; j < NUM_BRANCHES/2; j++) {
      server.registerBranch(request, "s", "");
    }
    for (int j = NUM_BRANCHES/2; j < 3*(NUM_BRANCHES/4); j++) {
      server.registerBranch(request, "", "r");
    }
    for (int j = 3*(NUM_BRANCHES/4); j < NUM_BRANCHES; j++) {
      server.registerBranch(request, "s", "r");
    }
  }

  std::cout << "-- closing " << NUM_BRANCHES/2 << " branches for each " << NUM_REQUESTS << " requests --" << std::endl;
  for (int i = 0; i < NUM_REQUESTS; i++) {
    request::Request * request = server.getRequest(RID);

    // close all branches in region 'r'
    for (int j = NUM_BRANCHES/2; j < NUM_BRANCHES; j++) {
      int status = server.closeBranch(request, j);
      ASSERT_EQ(OK, status);
    }
  }

  // sanity check
  std::cout << "-- checking status for " << NUM_REQUESTS << " requests --" << std::endl;
  for (int i = 0, status = 0; i < NUM_REQUESTS; i++) {
    request::Request * request = server.getRequest(RID);

    status = server.checkRequest(request, "", "");
    ASSERT_EQ(OPENED, status);

    status = server.checkRequest(request, "s", "");
    ASSERT_EQ(OPENED, status);

    status = server.checkRequest(request, "", "r");
    ASSERT_EQ(CLOSED, status);

    status = server.checkRequest(request, "s", "r");
    ASSERT_EQ(CLOSED, status);
  }
}