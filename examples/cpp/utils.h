#ifndef UTILS_H
#define UTILS_H

#include "monitor.grpc.pb.h"

/* Input commands */
const std::string REGISTER_REQUEST = "rr";
const std::string REGISTER_BRANCH = "rb";
const std::string REGISTER_BRANCHES = "rbs";
const std::string CLOSE_BRANCH = "cb";
const std::string WAIT_REQUEST = "wr";
const std::string CHECK_REQUEST = "cr";
const std::string CHECK_REQUEST_BY_REGIONS = "crr";
const std::string CHECK_PREVENTED_INCONSISTENCIES = "gi";
const std::string SLEEP = "sleep";
const std::string EXIT = "exit";

// explicit workaround since grpc does not provide conversion in cpp
std::string StatusCodeToString(int code) {
    switch(code) {
        case 1: return "OK";
        case 2: return "UNKNOWN";
        case 3: return "INVALID_ARGUMENT";
        case 4: return "DEADLINE_EXCEEDED";
        case 5: return "NOT_FOUND";
        case 6: return "ALREADY_EXISTS";
        case 7: return "PERMISSION_DENIED";
        case 8: return "RESOURCE_EXHAUSTED";
        case 9: return "FAILED_PRECONDITION";
        case 10: return "ABORTED";
        case 11: return "OUT_OF_RANGE";
        case 12: return "UNIMPLEMENTED";
        case 13: return "INTERNAL";
        case 14: return "UNAVAILABLE";
        case 15: return "DATA_LOSS";
        case 16: return "UNAUTHENTICATED";
        default: return "UNKNOWN ERROR!";
    }
}

// explicit workaround once again
std::string RequestStatusToString(monitor::RequestStatus status) {
    switch (status) {
        case monitor::RequestStatus::OPENED: return "OPENED";
        case monitor::RequestStatus::CLOSED: return "CLOSED";
        default: return "UNKNOWN STATUS!";
    }
}

#endif