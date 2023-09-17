#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <string>
#include "../src/server.h"
#include "../src/metadata/request.h"

static const int OK = 0;
static const int INCONSISTENCY_PREVENTED = 1;
static const int INCONSISTENCY_NOT_PREVENTED = 0;
static const int TIMED_OUT = -1;

static const int INVALID_REQUEST = -1;
static const int CONTEXT_NOT_FOUND = -2;
static const int INVALID_BRANCH_SERVICE = -2;
static const int INVALID_BRANCH_REGION = -3;

static const std::string SID = "eu-central-1";
static const std::string RID = "myrequestid";
static const std::string TAG = "mytag";
static const std::string EMPTY_TAG = "";

// force test failure
static const std::string ERROR_PARSING_FULL_BID = "ERROR_PARSING_FULL_BID";

static std::string getRid(int id) {
  return SID + ':' + std::to_string(id);
}

// for closing branches
static std::string getBid(int bid) {
  return SID + '_' + std::to_string(bid);
}

// for register branches
static std::string getFullBid(std::string rid, int id) {
  return SID + '_' + std::to_string(id) + ":" + rid;
}

static std::string parseFullBid(rendezvous::Server * server, metadata::Request * request, std::string bid, int bid_idx=-1) {
  // just a workaround for a quick sanity check!
  // getBid() computes the id which should match the one parsed by the server
  // we return "ERR" to force the test to fail if the following condition is not true (which should never happen if everything is ok)
  if (bid_idx != -1 && getBid(bid_idx) == server->parseFullBid(bid).first) {
    return server->parseFullBid(bid).first;
  }
  return "ERROR_PARSING_FULL_BID";
}

#endif