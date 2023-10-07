#include "server.grpc.pb.h"
#include "../examples/cpp/utils.h"
#include "../src/server.h"
#include "gtest/gtest.h"
#include <grpcpp/grpcpp.h>
#include "utils.h"
#include <thread>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

// -------------
// GRPS API TEST
// -------------

/* ------------

Helper Methods

--------------- */

void registerRequestAndAssert(std::string * rid) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  grpc::ClientContext context;
  rendezvous::RegisterRequestMessage request;
  rendezvous::RegisterRequestResponse response;
  request.set_rid(*rid);

  auto status = stub->registerRequest(&context, request, &response);
  ASSERT_TRUE(status.ok());
  *rid = response.rid();
}

void registerBranchGTestAndAssert(std::string rid, long bid, std::string service, std::string region) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  grpc::ClientContext context;
  rendezvous::registerBranchGTestMessage request;
  rendezvous::registerBranchGTestResponse response;
  request.set_rid(rid);
  request.set_service(service);
  request.set_region(region);

  auto status = stub->registerBranchGTestRegion(&context, request, &response);
  ASSERT_TRUE(status.ok());
  ASSERT_EQ(bid, response.bid());
}

void registerBranchGTestesAndAssert(std::string rid, long num, std::string service, std::string region) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  grpc::ClientContext context;
  rendezvous::registerBranchGTestesMessage request;
  rendezvous::registerBranchGTestesResponse response;
  request.set_rid(rid);
  request.set_num(num);
  request.set_service(service);
  request.set_region(region);

  auto status = stub->registerBranchGTest(&context, request, &response);
  ASSERT_TRUE(status.ok());
  ASSERT_EQ(num, response.bid().size());
}

void closeBranchAndAssert(std::string rid, long bid) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  grpc::ClientContext context;
  rendezvous::CloseBranchMessage request;
  rendezvous::Empty response;
  request.set_rid(rid);
  request.set_bid(bid);

  auto status = stub->closeBranch(&context, request, &response);

  ASSERT_TRUE(status.ok());
}

void checkRequestAndAssert(std::string rid, std::string service, std::string region, int expectedStatus) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  grpc::ClientContext context;
  rendezvous::CheckRequestMessage request;
  rendezvous::CheckRequestResponse response;
  request.set_rid(rid);
  request.set_service(service);
  request.set_region(region);

  auto status = stub->checkRequest(&context, request, &response);
  ASSERT_TRUE(status.ok());
  ASSERT_EQ(expectedStatus, response.status());
}

void checkRequestAndAssertError(std::string rid, std::string service, std::string region, grpc::StatusCode expectedStatus) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  grpc::ClientContext context;
  rendezvous::CheckRequestMessage request;
  rendezvous::CheckRequestResponse response;
  request.set_rid(rid);
  request.set_service(service);
  request.set_region(region);

  auto status = stub->checkRequest(&context, request, &response);
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(expectedStatus, status.error_code());
}

void checkRequestByRegionsAndAssertError(std::string rid, std::string service, grpc::StatusCode expectedStatus) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  grpc::ClientContext context;
  rendezvous::CheckRequestByRegionsMessage request;
  rendezvous::CheckRequestByRegionsResponse response;
  request.set_rid(rid);
  request.set_service(service);

  auto status = stub->checkRequestByRegions(&context, request, &response);
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(expectedStatus, status.error_code());
}

void waitRequest(std::string rid, std::string service, std::string region) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  grpc::ClientContext context;
  rendezvous::WaitRequestMessage request;
  rendezvous::Empty response;
  request.set_rid(rid);

  auto status = stub->wait(&context, request, &response);
  ASSERT_TRUE(status.ok());
}

void waitRequestAndAssertError(std::string rid, std::string service, std::string region, grpc::StatusCode expectedStatus) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  grpc::ClientContext context;
  rendezvous::WaitRequestMessage request;
  rendezvous::Empty response;
  request.set_rid(rid);
  request.set_service(service);
  request.set_region(region);

  auto status = stub->wait(&context, request, &response);
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(expectedStatus, status.error_code());
}

/* --------

GTest Suit

----------- */

TEST(gRPCTest, RegisterRequest_WithRID_RegisterAndCloseBranches) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;
  std::string rid = RID;

  registerRequestAndAssert(&rid);

  registerBranchGTestAndAssert(rid, 0, "s", "r");
  registerBranchGTestesAndAssert(rid, 3, "", "");

  /* Close All Branches */
  for(int bid = 0; bid < 4; bid++) {
    closeBranchAndAssert(rid, bid);
  }
}

TEST(gRPCTest, registerBranchGTest_NoRID) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;

  /* Register Branch */
  grpc::ClientContext context;
  rendezvous::registerBranchGTestMessage request;
  rendezvous::registerBranchGTestResponse response;

  status = stub->registerBranchGTestRegion(&context, request, &response);

  ASSERT_TRUE(status.ok());
  ASSERT_EQ("rendezvous-0", response.rid());
}

TEST(gRPCTest, registerBranchGTestes_NoRID) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;

  /* Register Multiple Branches */
  grpc::ClientContext context;
  rendezvous::registerBranchGTestesMessage request;
  rendezvous::registerBranchGTestesResponse response;
  request.set_num(3);

  status = stub->registerBranchGTest(&context, request, &response);

  ASSERT_TRUE(status.ok());
  ASSERT_EQ("rendezvous-1", response.rid());
  ASSERT_TRUE(response.bid().size() == 3);
  ASSERT_EQ(0, response.bid()[0]);
  ASSERT_EQ(1, response.bid()[1]);
  ASSERT_EQ(2, response.bid()[2]);
}

TEST(gRPCTest, registerBranchGTest_InvalidRID) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;
  std::string rid = "";

  registerRequestAndAssert(&rid);

  /* Register Branch */
  grpc::ClientContext context;
  rendezvous::registerBranchGTestMessage request;
  rendezvous::registerBranchGTestResponse response;
  request.set_rid("invalid_rid");

  status = stub->registerBranchGTestRegion(&context, request, &response);

  ASSERT_FALSE(status.ok());
  ASSERT_EQ(grpc::INVALID_ARGUMENT, status.error_code());
}

TEST(gRPCTest, registerBranchGTestes_InvalidRid) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;

  /* Register Branch */
  grpc::ClientContext context;
  rendezvous::registerBranchGTestesMessage request;
  rendezvous::registerBranchGTestesResponse response;
  request.set_num(3);
  request.set_rid("invalid_rid");

  status = stub->registerBranchGTest(&context, request, &response);

  ASSERT_FALSE(status.ok());
  ASSERT_EQ(grpc::INVALID_ARGUMENT, status.error_code());
}

TEST(gRPCTest, CloseBranch_InvalidBID) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;
  std::string rid = "";

  registerRequestAndAssert(&rid);

  /* Close Branch */
  grpc::ClientContext context;
  rendezvous::CloseBranchMessage request;
  rendezvous::Empty response;
  request.set_rid(rid);
  request.set_bid(-1);

  status = stub->closeBranch(&context, request, &response);

  ASSERT_FALSE(status.ok());
  ASSERT_EQ(grpc::INVALID_ARGUMENT, status.error_code());
}


TEST(gRPCTest, CheckRequest) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;
  std::string rid = "";

  registerRequestAndAssert(&rid);

  checkRequestAndAssert(rid, "", "", CLOSED);

  registerBranchGTestAndAssert(rid, 0, "s", "r");
  registerBranchGTestAndAssert(rid, 1, "s", "");
  registerBranchGTestAndAssert(rid, 2, "s1", "r");

  checkRequestAndAssert(rid, "", "", OPENED);
  checkRequestAndAssert(rid, "s", "", OPENED);
  checkRequestAndAssert(rid, "", "r", OPENED);

  closeBranchAndAssert(rid, 0);

  checkRequestAndAssert(rid, "", "r", OPENED);

  closeBranchAndAssert(rid, 2);

  checkRequestAndAssert(rid, "", "r", CLOSED);
  checkRequestAndAssert(rid, "", "", OPENED);
  checkRequestAndAssert(rid, "s", "", OPENED);

  closeBranchAndAssert(rid, 1);

  checkRequestAndAssert(rid, "", "", CLOSED);
}

 TEST(gRPCTest, CheckRequest_InvalidRid) { 

  checkRequestAndAssertError("invalid_rid", "s", "r", grpc::INVALID_ARGUMENT);
}

TEST(gRPCTest, CheckRequest_ContextNotFound) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;
  std::string rid = "";

  registerRequestAndAssert(&rid);

  registerBranchGTestAndAssert(rid, 0, "s", "r");

  checkRequestAndAssertError(rid, "wrong_service", "", grpc::NOT_FOUND);
  checkRequestAndAssertError(rid, "", "wrong_region", grpc::NOT_FOUND);
  checkRequestAndAssertError(rid, "wrong_service", "wrong_region", grpc::NOT_FOUND);
}

TEST(gRPCTest, CheckRequestByRegions_InvalidRid) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;

  checkRequestByRegionsAndAssertError("invalid_rid", "s", grpc::INVALID_ARGUMENT);
}

TEST(gRPCTest, CheckRequestByRegions_ContextAndRegionNotFound) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;
  std::string rid = "";

  registerRequestAndAssert(&rid);

  registerBranchGTestAndAssert(rid, 0, "", "");

  /* Check Request By Regions But Region Not Found */
  checkRequestByRegionsAndAssertError(rid, "", grpc::NOT_FOUND);

  registerBranchGTestAndAssert(rid, 1, "s", "");

  checkRequestByRegionsAndAssertError(rid, "wrong_service", grpc::NOT_FOUND);
}

TEST(gRPCTest, CheckRequestByRegions) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  auto status = grpc::Status::OK;
  std::string rid = "";

  registerRequestAndAssert(&rid);

  registerBranchGTestAndAssert(rid, 0, "", "");
  registerBranchGTestAndAssert(rid, 1, "s", "");
  registerBranchGTestAndAssert(rid, 2, "", "r1");
  registerBranchGTestAndAssert(rid, 3, "s", "r2");

  /* Check Request By Regions */
  grpc::ClientContext context_chr2;
  rendezvous::CheckRequestByRegionsMessage request_chr2;
  rendezvous::CheckRequestByRegionsResponse response_chr2;
  request_chr2.set_rid(rid);

  status = stub->checkRequestByRegions(&context_chr2, request_chr2, &response_chr2);
  ASSERT_TRUE(status.ok());
  ASSERT_EQ(2, response_chr2.regionstatus().size());

  for(const auto& regionStatus : response_chr2.regionstatus()) {
    ASSERT_TRUE(regionStatus.region() == "r1" || regionStatus.region() == "r2");
    ASSERT_EQ(OPENED, regionStatus.status());
  }

  /* Close Branch in Service 's' */
  closeBranchAndAssert(rid, 3);

  /* Check Request By Regions With Service */
  grpc::ClientContext context_chr3;
  rendezvous::CheckRequestByRegionsMessage request_chr3;
  rendezvous::CheckRequestByRegionsResponse response_chr3;
  request_chr3.set_rid(rid);
  request_chr3.set_service("s");

  status = stub->checkRequestByRegions(&context_chr3, request_chr3, &response_chr3);
  ASSERT_TRUE(status.ok());
  ASSERT_EQ(1, response_chr3.regionstatus().size());

  ASSERT_EQ("r2", response_chr3.regionstatus()[0].region());
  ASSERT_EQ(CLOSED, response_chr3.regionstatus()[0].status());
}

TEST(gRPCTest, WaitRequest_InvalidRid) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);

  waitRequestAndAssertError("invalid_rid", "s", "r", grpc::INVALID_ARGUMENT);
}

TEST(gRPCTest, WaitRequest_ContextNotFound) {
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  std::string rid = "";

  registerRequestAndAssert(&rid);

  waitRequestAndAssertError(rid, "wrong_service", "", grpc::NOT_FOUND);
  waitRequestAndAssertError(rid, "", "wrong_region", grpc::NOT_FOUND);
  waitRequestAndAssertError(rid, "wrong_service", "wrong_region", grpc::NOT_FOUND);
}

TEST(gRPCTest, WaitRequest) { 
  auto channel = grpc::CreateChannel("localhost:8000", grpc::InsecureChannelCredentials());
  auto stub = rendezvous::RendezvousService::NewStub(channel);
  std::vector<std::thread> threads;
  auto status = grpc::Status::OK;
  std::string rid = "";

  registerRequestAndAssert(&rid);

  registerBranchGTestAndAssert(rid, 0, "", "");
  registerBranchGTestAndAssert(rid, 1, "service1", "");
  registerBranchGTestAndAssert(rid, 2, "", "region1");
  registerBranchGTestAndAssert(rid, 3, "service2", "region2");
  sleep(0.2);

  threads.emplace_back([rid] {
    waitRequest(rid, "", "");
  });
  threads.back().detach();

  threads.emplace_back([rid] {
    waitRequest(rid, "service1", "");
  });
  threads.back().detach();

  threads.emplace_back([rid] {
    waitRequest(rid, "", "region1");
  });
  threads.back().detach();

  threads.emplace_back([rid] {
    waitRequest(rid, "service2", "region2");
  });
  threads.back().detach();

  sleep(0.2);
  closeBranchAndAssert(rid, 0);
  closeBranchAndAssert(rid, 1);
  closeBranchAndAssert(rid, 2);

  /* Sanity Check - ensure that locks still work */
  registerBranchGTestAndAssert(rid, 4, "storage", "EU");
  registerBranchGTestAndAssert(rid, 5, "storage", "US");
  registerBranchGTestAndAssert(rid, 6, "notification", "GLOBAL");
  sleep(0.2);

  threads.emplace_back([rid] {
    waitRequest(rid, "", "");
  });
  threads.back().detach();

  threads.emplace_back([rid] {
    waitRequest(rid, "storage", "US");
  });
  threads.back().detach();

  threads.emplace_back([rid] {
    waitRequest(rid, "storage", "");
  });
  threads.back().detach();

  threads.emplace_back([rid] {
    waitRequest(rid, "", "GLOBAL");
  });
  threads.back().detach();

  sleep(0.2);
  closeBranchAndAssert(rid, 3);
  closeBranchAndAssert(rid, 4);
  closeBranchAndAssert(rid, 5);
  closeBranchAndAssert(rid, 6);
  

  /* wait for all threads */
  for(auto& thread : threads) {
    if (thread.joinable()) {
        thread.join();
    }
  }

  /* Validate number of prevented inconsistencies*/
  grpc::ClientContext context;
  rendezvous::Empty request;
  rendezvous::GetPreventedInconsistenciesResponse response;

  status = stub->getPreventedInconsistencies(&context, request, &response);
  ASSERT_TRUE(status.ok());
  long value = response.inconsistencies();
  ASSERT_EQ(8, value);
}