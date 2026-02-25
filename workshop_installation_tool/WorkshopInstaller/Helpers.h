#pragma once
#include "Main.h"
#include "Locale.h"

extern CRITICAL_SECTION g_LogCriticalSection; //Helpers.cpp is the owner of this instance

bool ReadSteamAppID(const char* filename, std::string& outAppID);
bool BackupFile(System::String^ destRoot, System::String^ relTargetPath, System::String^ relBackupPath, StreamWriter^ uninstallLog);
void DebugLog(System::String^ text);

bool IsJvmFile(System::String^ fileName);
bool IsBakFile(System::String^ fileName);
bool IsInsideBakDir(System::String^ fileName);
bool IsUnderDir(System::String^ relPath, System::String^ dir);
bool IsConflictRelevantFile(System::String^ relPath);

uint64 ItemIdFromString(System::String^ strItemId);
uint64 ItemIdFromSentinelFileName(System::String^ fileName);
char* StringToUtf8(System::String^ str);
uint64 GetTimeNow(); //in ms
System::String^ TimeToString(int32 time); //returns UTC time
System::String^ GetNormalizedRelativePath(System::String^ rootPath, System::String^ fullPath);
char* BrowseForDirectory();

//SentinelFormat helps to read sentinel files in CMainModule::IsInstallInterrupted()
//abstract: prevents instantiation (gcnew SentinelFormat() is illegal)
//sealed: guarantees no one can extend or override the format and freezes the contract
ref class SentinelFormat abstract sealed
{
public:
    //using _literal_ here because we're defining the on-disk protocol that guards crash recovery logic
    //literal can't reassigned, modified accidentially and it can't depend on runtime initialization
    literal System::String^ itemKey = "ITEM";
    literal System::String^ timeKey = "START_TIME";
    literal wchar_t separator = L'=';

    //static, do NOT move implementation to the .cpp
    static bool TryParseLine(System::String^ line, System::String^% key, System::String^% value)
    {
        if (System::String::IsNullOrWhiteSpace(line))
            return false;

        int pos = line->IndexOf(separator);
        if (pos <= 0)
            return false;

        key = line->Substring(0, pos)->Trim();
        value = line->Substring(pos + 1)->Trim();
        
        return true;
    }
};