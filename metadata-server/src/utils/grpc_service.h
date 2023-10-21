#ifndef UTILS_GRPC_SERVICE_H
#define UTILS_GRPC_SERVICE_H

#include <string>

namespace utils {

    /* --------------------- */
    /* gRPC structure helper */
    /* --------------------- */
    typedef google::protobuf::RepeatedPtrField<std::string> ProtoVec;

    /* ------------------- */
    /* gRPC error messages */
    /* ------------------- */

    /* client gRPC custom error messages */
    const std::string ERR_MSG_SERVICE_NOT_FOUND = "Request status not found for the provided service";
    const std::string ERR_MSG_SERVICE_EMPTY = "Service cannot be empty";
    const std::string ERR_MSG_INVALID_REGION = "Branch does not exist in the given region";
    const std::string ERR_MSG_BRANCH_NOT_FOUND = "No branch was found with the provided bid";
    const std::string ERR_MSG_INVALID_BRANCH_SERVICE = "No branch was found for the provided service";
    const std::string ERR_MSG_INVALID_BRANCH_REGION = "No branch was found for the provided service";
    const std::string ERR_MSG_EMPTY_REGION = "Region cannot be empty";
    const std::string ERR_MSG_INVALID_TIMEOUT = "Invalid timeout. Value must to be greater than 0";
    const std::string ERR_MSG_REGISTER_BRANCHES_INVALID_DATASTORES = "Invalid datastores arguments";
    const std::string ERR_MSG_INVALID_TAG_USAGE = "Tag can only be specified when service is specified";
    const std::string ERR_MSG_FAILED_DETAILED_QUERY = "Cannot provide detailed information without specifying service";
    const std::string ERR_MSG_TAG_ALREADY_EXISTS = "Tag already exists and must to be unique";
    const std::string ERR_MSG_REGISTERING_BRANCH = "Error registering branch";
    const std::string ERR_MSG_NUM_SERVICES_TAGS_DOES_NOT_MATCH = "If tags are provided, the number is required to be equal to the number of services";
    const std::string ERR_MSG_NUM_SERVICES_CONTEXTS_DOES_NOT_MATCH = "Number of contexts does not match the number of services";
    const std::string ERR_MSG_INVALID_CONTEXT = "Invalid rendezvous context (prev_service or rid)";
    const std::string ERR_MSG_INVALID_SERVICE_REGION = "Invalid service or region provided";
    const std::string ERR_MSG_WAIT_CALL_NO_CURRENT_SERVICE = "Current service branch needs to be registered before any wait call";
    const std::string ERR_MSG_INVALID_TAG = "Invalid service tag";
    const std::string ERR_MSG_INVALID_SERVICES_EXCLUSIVE = "Cannot provide either 'service' or 'services' simultaneously";
    const std::string ERR_MSG_INVALID_SERVICE = "Invalid service";
    const std::string ERR_MSG_VISIBLE_BIDS_TIMEOUT = "Timedout while waiting for bids to be visible";
    
    /* common gRPC custom error messages */
    const std::string ERR_MSG_INVALID_REQUEST = "Invalid request identifier";
    const std::string ERR_MSG_INVALID_ASYNC_ZONE = "Invalid async zone identifier";
    const std::string ERR_MSG_INVALID_REQUEST_SUB_RID = "Invalid request identifier (subrid does not exist)";
    const std::string ERR_MSG_REQUEST_ALREADY_EXISTS = "A request was already registered with the provided identifier";
    const std::string ERR_MSG_BRANCH_ALREADY_EXISTS = "A branch was already registered with the provided identifier";
    const std::string ERR_PARSING_RID = "Unexpected error parsing rid";
    const std::string ERR_PARSING_BID = "Unexpected error parsing bid";
}

#endif