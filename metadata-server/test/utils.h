#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <string>
#include "../src/server.h"
#include "../src/metadata/request.h"

// success and error codes
static const int OK = 0;
static const int INCONSISTENCY_PREVENTED = 1;
static const int INCONSISTENCY_NOT_PREVENTED = 0;
static const int TIMED_OUT = -1;
static const int INVALID_REQUEST = -1;
static const int CONTEXT_NOT_FOUND = -2;
static const int INVALID_BRANCH_SERVICE = -2;
static const int INVALID_BRANCH_REGION = -3;

// helpers for building identifiers
static const std::string RENDEZVOUS_PREFIX = "rv";
static const std::string SID = "eu";
static const std::string RID = "myrequestid";
static const std::string TAG = "mytag";
static const std::string EMPTY_TAG = "";

// examples of identifiers
static const std::string ROOT_SUB_RID = "";
static const std::string SUB_RID_0 = "root:eu0";
static const std::string SUB_RID_1 = "root:eu1";
static const std::string SUB_RID_2 = "root:eu2";
static const std::string SUB_RID_0_0 = "root:eu0:eu0";
static const std::string SUB_RID_0_1 = "root:eu0:eu1";
static const std::string SUB_RID_1_0 = "root:eu1:eu0";
static const std::string SUB_RID_0_0_0 = "root:eu0:eu0:eu0";
static const std::string DUMMY_ACSL = "dummy_acsl";

// force test failure
static const std::string ERROR_PARSING_FULL_BID = "ERROR_PARSING_FULL_BID";

static std::string getRid(int id) {
  return SID + ':' + std::to_string(id);
}

// for closing branches
static std::string getBid(int bid) {
  return RENDEZVOUS_PREFIX + '_' + SID + '_' + std::to_string(bid);
}

// for register branches
static std::string getFullBid(std::string rid, int id) {
  return SID + '_' + std::to_string(id+1) + ":" + rid;
}

static std::string parseFullBid(rendezvous::Server * server, metadata::Request * request, std::string bid, int bid_idx=-1) {
  // just a workaround for a quick sanity check!
  // getBid() computes the id which should match the one parsed by the server
  // we return "ERR" to force the test to fail if the following condition is not true (which should never happen if everything is ok)
  if (bid_idx != -1 && getBid(bid_idx) == server->parseFullId(bid).first) {
    return server->parseFullId(bid).first;
  }
  return "ERROR_PARSING_FULL_BID";
}

#endif
