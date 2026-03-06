#pragma once

#define DUMP_LOGS
#define USE_MUTEX
//#define RUN_TESTS //run unit tests instead of normal execution pipeline

//UGC pagination control keys
#define UGC_PAGINATION_NEXT_PAGE    "n"
#define UGC_PAGINATION_PREV_PAGE    "p"
#define UGC_PAGINATION_QUIT         "q"
#define UGC_PAGINATION_SELECT       "s"

class UploaderConfig
{
public:
    static UploaderConfig& Instance()
    {
        static UploaderConfig instance;
        return instance;
    }

    bool bNoConfirm = false;
    bool bDumpDebugLogs = false;
    bool bAutoDefaults = false;
    bool bUseMutex = false;
    bool bNoWait = false;

private:
    UploaderConfig() = default;
};