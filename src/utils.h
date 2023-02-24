#ifndef UTILS_H
#define UTILS_H

namespace utils {

    #ifndef DEBUG 
    #define DEBUG 1 // set debug mode
    #endif

    #if DEBUG
    #define log(...) {\
        char str[100];\
        sprintf(str, __VA_ARGS__);\
        std::cout << "[" << __FUNCTION__ << "] " << str << std::endl;\
        }
    #else
    #define log(...)
    #endif

    /* request status */
    const int OPENED = 0;
    const int CLOSED = 1;

    /* gRPC error messages */
    const std::string ERROR_MSG_REQUEST_DOES_NOT_EXIST = "Request does not exist";
    const std::string ERROR_MSG_BRANCH_DOES_NOT_EXIST = "Branch does not exist";

    /* Client */
    const std::string REGISTER_REQUEST = "rr";
    const std::string REGISTER_BRANCH = "rb";
    const std::string REGISTER_BRANCHES = "rbs";
    const std::string CLOSE_BRANCH = "cb";
    const std::string WAIT_REQUEST = "wr";
    const std::string CHECK_REQUEST = "cr";
    const std::string CHECK_REQUEST_BY_REGIONS = "crr";
    const std::string SLEEP = "sleep";
    const std::string EXIT = "exit";

}

#endif