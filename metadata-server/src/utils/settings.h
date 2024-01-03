#ifndef UTILS_SETTINGS_H
#define UTILS_SETTINGS_H

#include <string>
#include <map>
#include <set>

namespace utils {

    /* ------------------------ */
    /* parsing/formating of IDs */
    /*     (subrids, bids)      */
    /* ------------------------ */
    static int SIZE_SIDS = 1;
    static const char FULL_ID_DELIMITER = ':';
    static std::string ROOT_ACSL_ID = "";
    static std::string ROOT_SERVICE_NODE_ID = "";

    /* -------------------------------- */
    /* parsed values from settings.json */
    /* -------------------------------- */
    static bool ASYNC_REPLICATION;
    static bool CONTEXT_VERSIONING;
    static int WAIT_REPLICA_TIMEOUT_S;
    
    /* --------------------------- */
    /* parsed values from env vars */
    /* --------------------------- */
    static bool CONSISTENCY_CHECKS;
}

#endif
