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
    static std::string ROOT_ASYNC_ZONE_ID = "r";

    /* -------------------------------- */
    /* parsed values from settings.json */
    /* -------------------------------- */
    static bool ASYNC_REPLICATION = false;
    static bool CONTEXT_VERSIONING = false;
    
    /* --------------------------- */
    /* parsed values from env vars */
    /* --------------------------- */
    static bool CONSISTENCY_CHECKS = true;
}

#endif