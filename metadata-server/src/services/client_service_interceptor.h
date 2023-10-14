#ifndef CLIENT_SERVICE_INTERCEPTOR_H
#define CLIENT_SERVICE_INTERCEPTOR_H

#include <memory>
#include <vector>

#include "server.grpc.pb.h"
#include "../metadata/request.h"
#include "../metadata/subscriber.h"
#include "../server.h"
#include "../replicas/replica_client.h"
#include "../utils/grpc_service.h"
#include "../utils/metadata.h"
#include "../utils/settings.h"
#include <atomic>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#include <unordered_map>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/generic/generic_stub.h>
#include <grpcpp/impl/codegen/proto_utils.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/client_interceptor.h>
#include <iostream>

class ClientServiceInterceptor : public grpc::experimental::Interceptor {
    private:
        grpc::experimental::ClientRpcInfo* _info;
        const std::string _client_method = "/rendezvous.ClientService/RegisterBranch";
        const std::string _replica_method = "/rendezvous_server.ServerService/RegisterBranch";
        

    public:
        explicit ClientServiceInterceptor(grpc::experimental::ClientRpcInfo* info) {
            _info = info;
        }

        void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
            // std::cout << "START" << _info->method() << methods->QueryInterceptionHookPoint() << std::endl;
            std::string method = _info->method();
            std::string hook_point = GetHookPoint(methods);
            std::cout << "[CLIENT] START " << method << ": " << hook_point << std::endl;
            std::cout << "[CLIENT] MESSAGE: " << methods->GetSerializedSendMessage() << std::endl;
            methods->Proceed();
            std::cout << "[CLIENT] END " << method << ": " << hook_point << std::endl;
        }

        std::string GetHookPoint(grpc::experimental::InterceptorBatchMethods* methods) {
            if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_SEND_INITIAL_METADATA)) {
                return "PRE_SEND_INITIAL_METADATA";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_SEND_MESSAGE)) {
                return "PRE_SEND_MESSAGE";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_SEND_MESSAGE)) {
                return "POST_SEND_MESSAGE";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_SEND_STATUS)) {
                return "PRE_SEND_STATUS";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_SEND_CLOSE)) {
                return "PRE_SEND_CLOSE";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_RECV_INITIAL_METADATA)) {
                return "PRE_RECV_INITIAL_METADATA";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_RECV_MESSAGE)) {
                return "PRE_RECV_MESSAGE";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_INITIAL_METADATA)) {
                return "POST_RECV_INITIAL_METADATA";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_MESSAGE)) {
                return "POST_RECV_MESSAGE";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_STATUS)) {
                return "POST_RECV_STATUS";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_CLOSE)) {
                return "POST_RECV_CLOSE";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_SEND_CANCEL)) {
                return "PRE_SEND_CANCEL";
            }
            else if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::NUM_INTERCEPTION_HOOKS)) {
                return "NUM_INTERCEPTION_HOOKS";
            }
            else {
                return "UNEXPECTED";
            }
        }
};

class ClientServiceInterceptorFactory : public grpc::experimental::ClientInterceptorFactoryInterface {
 public:
  grpc::experimental::Interceptor* CreateClientInterceptor(grpc::experimental::ClientRpcInfo* info) override {
    return new ClientServiceInterceptor(info);
  }
};

#endif
