#ifndef UTILS_METADATA_H
#define UTILS_METADATA_H

#include <string>
#include <map>
#include <vector>

namespace utils {

    static const char FULL_ID_DELIMITER = ':';

    /* ------------------------------ */
    /* helper for CheckStatus request */
    /* ------------------------------- */
    typedef struct StatusStruct {
        int status;
        std::map<std::string, int> tagged;
        std::map<std::string, int> regions;
    } Status;

    /* ------------------------------------ */
    /* helper for FetchDependencies request */
    /* ------------------------------------ */
    typedef struct DependenciesStruct {
        int res;
        std::vector<std::string> deps;
    } Dependencies;
    
    /* --------------*/
    /* status values */
    /* ------------- */
    const int CLOSED = 0;
    const int OPENED = 1;
    const int UNKNOWN = 2;

    // helper for errors
    const int INVALID_SERVICE = -2;
    const int INVALID_CONTEXT = -3;
}

#endif