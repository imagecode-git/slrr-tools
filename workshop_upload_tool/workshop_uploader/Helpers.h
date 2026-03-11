#pragma once

#define NOMINMAX
#include <windows.h>
#include <string>
#include <iostream> //cin, cout
#include <limits> //for UserPrompt()
#include <fstream>
#include <sstream>
#include <vector>
#include <thread> //PrintSpinner()
#include <chrono> //PrintSpinner()
#include <conio.h> //_getch()
#include <cctype> //isdigit()
#include <algorithm> //transform()
#include <regex> //ParseVideoUrl()
#include "Options.h"
#include "Locale.h"
#include "Environment.h"

#define DEBUG_LOG_FILE_MASK "debug.log"

extern CRITICAL_SECTION g_LogCriticalSection; //Helpers.cpp is the owner of this instance

enum class ConsoleTextColor
{
	Red,
	Yellow,
	Green,
	White,
	Cyan,
	Default
};

//array size helper
template <typename T, size_t N>
char(&_ArraySizeHelper(T(&array)[N]))[N];

#define ARRAY_SIZE(arr) sizeof(_ArraySizeHelper(arr))

template <typename Enum>
struct EnumStringPair
{
	Enum value;
	const char* name;
};

//full template function bodies must belong here, otherwise linker error
template <typename Enum>
bool ResolveEnumParam(const std::string& param, const EnumStringPair<Enum>* list, size_t size, Enum& outValue)
{
	for (size_t i = 0; i < size; ++i)
	{
		if (param == list[i].name)
		{
			outValue = list[i].value;
			return true;
		}
	}

	return false;
}

template <typename Enum>
const char* EnumParamToString(Enum value, const EnumStringPair<Enum>* list, size_t size, const char* fallback = "") //always have to pass size here due to ARRAY_SIZE macro restrictions
{
	for (size_t i = 0; i < size; ++i)
	{
		if (list[i].value == value)
			return list[i].name;
	}
	return fallback;
}

std::string ReadSteamAppId();

//path manipulation
std::string ResolveRelPath(const std::string fileName);
std::string ToShortPath(const std::string longPath);
std::string NormalizePath(const std::string& inputPath);

//file operations
bool FileExists(const std::string& filePath);
bool FindFiles(const std::string& fileDir, const std::string& fileMask, std::vector<std::string>& findResults);

//paths & directories
bool PathExists(const std::string& filePath); //something exists as a path, but it could be either file or directory
bool DirectoryExists(const std::string& directory);
bool DirectoryHasFiles(const std::string& directory);

//string tokenizer
void Tokenize(const std::string& input, std::vector<std::string>& output, const std::string& delim);

//parsers
std::string ParseVideoUrl(const std::string& videoUrl);
bool ParseBool(const std::string& strBool);
bool ParseUint64(const std::string& str, uint64& outValue);

//interaction with the user
bool UserPrompt(const std::string& question);
bool RequestString(const std::string& question, std::string& outValue); //string input
bool RequestInt(const std::string& question, int& outValue); //integer only

//progress indication
void PrintProgress(const uint64 processed, const uint64 total, const std::string& prefix = "");
void PrintSpinner(const std::string message);
void SetCursorVisible(bool bIsVisible); //hide or show caret
void PressAnyKey();

//messages
void PrintMessage(const std::string& message, ConsoleTextColor color = ConsoleTextColor::Default);
void WarningMessage(const std::string& message);
void ErrorMessage(const std::string& message);
void SuccessMessage(const std::string& message);
void ClearLine(); //removes leftovers from other messages
void SetTextColor(ConsoleTextColor color);

//logging
void DebugLog(const std::string& message);
void DebugLog(const char* message);
void DebugLogStrings(const std::vector<std::string>& vectorInput);