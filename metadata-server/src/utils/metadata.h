#ifndef UTILS_METADATA_H
#define UTILS_METADATA_H

#include <string>
#include <map>
#include <set>

namespace utils {

    /* ------------------------------ */
    /* helper for CheckStatus request */
    /* ------------------------------ */
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
        std::set<std::string> deps;
        std::set<std::string> indirect_deps;
    } Dependencies;
    
    /* --------------*/
    /* status values */
    /* ------------- */
    const int CLOSED = 0;
    const int OPENED = 1;
    const int UNKNOWN = 2;

    // helper for errors
    const int OK = 0;
    const int INVALID_SERVICE = -2;
    const int INVALID_CONTEXT = -3;
}

#endif