#ifndef UTILS_H
#define UTILS_H

#include <string>

namespace utils {

    typedef google::protobuf::RepeatedPtrField<std::string> ProtoVec;

    // debug mode
    #ifndef DEBUG 
    #define DEBUG 1
    #endif

    #if DEBUG
    #define log(...) {\
        char str[255];\
        sprintf(str, __VA_ARGS__);\
        std::cout << "[" << __FUNCTION__ << "] " << str << std::endl;\
        }
    #else
    #define log(...)
    #endif

    // measure overhead of API without any consistency checks
    #ifndef NO_CONSISTENCY_CHECKS 
    #define NO_CONSISTENCY_CHECKS 0
    #endif 

    /* client gRPC custom error messages */
    const std::string ERROR_MESSAGE_SERVICE_NOT_FOUND = "Request status not found for the provided service";
    const std::string ERROR_MESSAGE_REGION_NOT_FOUND = "Request status not found for the provided region";
    const std::string ERROR_MESSAGE_INVALID_BRANCH = "No branch was found with the provided bid";
    const std::string ERROR_MESSAGE_INVALID_BRANCH_SERVICE = "No branch was found for the provided service";
    const std::string ERROR_MESSAGE_INVALID_BRANCH_REGION = "No branch was found for the provided service";
    const std::string ERROR_MESSAGE_EMPTY_REGION = "Region cannot be empty";

    /* server gRPC custom error m essages */
    const std::string ERROR_MESSAGE_INVALID_CONTEXT = "Invalid context provided";
    
    /* common gRPC custom error messages */
    const std::string ERROR_MESSAGE_INVALID_REQUEST = "Invalid request identifier";
    const std::string ERROR_MESSAGE_REQUEST_ALREADY_EXISTS = "A request was already registered with the provided identifier";
    const std::string ERROR_MESSAGE_BRANCH_ALREADY_EXISTS = "A branch was already registered with the provided identifier";
}

#endif