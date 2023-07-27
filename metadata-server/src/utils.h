#ifndef UTILS_H
#define UTILS_H

#include <string>

namespace utils {

    typedef google::protobuf::RepeatedPtrField<std::string> ProtoVec;

    #ifndef CONTEXT_PROPAGATION
    #define CONTEXT_PROPAGATION 0
    #endif

    // subscribed requests
    #ifndef TRACK_SUBSCRIBED_BRANCHES
    #define TRACK_SUBSCRIBED_BRANCHES 1
    #endif

    // measure overhead of API without any consistency checks
    #ifndef SKIP_CONSISTENCY_CHECKS 
    #define SKIP_CONSISTENCY_CHECKS 0
    #endif 

    const std::string TIME_FORMAT = "%Y-%m-%d %H:%M:%S";

    const std::string ERR_PARSING_BID = "Unexpected error parsing full bid";
    
    /* client gRPC custom error messages */
    const std::string ERR_MSG_SERVICE_NOT_FOUND = "Request status not found for the provided service";
    const std::string ERR_MSG_INVALID_REGION = "Branch does not exist in the given region";
    const std::string ERR_MSG_BRANCH_NOT_FOUND = "No branch was found with the provided bid";
    const std::string ERR_MSG_INVALID_BRANCH_SERVICE = "No branch was found for the provided service";
    const std::string ERR_MSG_INVALID_BRANCH_REGION = "No branch was found for the provided service";
    const std::string ERR_MSG_EMPTY_REGION = "Region cannot be empty";
    const std::string ERR_MSG_INVALID_TIMEOUT = "Invalid timeout. Value needs to be greater than 0";
    const std::string ERR_MSG_REGISTER_BRANCHES_INVALID_DATASTORES = "Invalid datastores arguments";

    /* server gRPC custom error m essages */
    const std::string ERR_MSG_INVALID_CONTEXT = "Invalid context provided";
    
    /* common gRPC custom error messages */
    const std::string ERR_MSG_INVALID_REQUEST = "Invalid request identifier";
    const std::string ERR_MSG_REQUEST_ALREADY_EXISTS = "A request was already registered with the provided identifier";
    const std::string ERR_MSG_BRANCH_ALREADY_EXISTS = "A branch was already registered with the provided identifier";
}

#endif