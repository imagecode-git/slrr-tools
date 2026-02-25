#include "stdafx.h"
#include "Helpers.h"

using namespace std;

CRITICAL_SECTION g_LogCriticalSection;

static size_t g_lastConsoleLineLength = 0;
static uint64 g_spinnerIndex = 0;
static chrono::steady_clock::time_point g_lastSpinnerUpdate;

//functions with static linkage must be always on top
static string JoinPath(const string& dir, const string& file)
{
	if (dir.empty())
		return file;

	if (dir.back() == '\\')
		return dir + file;

	return dir + "\\" + file;
}

static void TrimWhitespace(string& strInput)
{
	const char* whitespace = " \t\r\n";

	size_t start = strInput.find_first_not_of(whitespace);
	if (start == std::string::npos)
	{
		strInput.clear();
		return;
	}

	size_t end = strInput.find_last_not_of(whitespace);

	strInput.erase(end + 1);
	strInput.erase(0, start);
}

static void ToLower(string& strInput)
{
	transform(strInput.begin(), strInput.end(), strInput.begin(),
		[](unsigned char c) { return tolower(c); });
}

string ReadSteamAppId()
{
	string filePath = ResolveRelPath("steam_appid.txt");
	ifstream file(filePath);

	if (!file)
	{
		PrintMessage(LOC_READ_STEAM_APPID_FAIL);
		return {};
	}

	string appId;
	getline(file, appId); //reads first line only
	file.close();

	TrimWhitespace(appId);

	if (appId.empty())
	{
		PrintMessage(LOC_READ_STEAM_APPID_FAIL);
		return {};
	}

	if (appId != to_string(STEAM_APPID_SLRR) && appId != to_string(STEAM_APPID_SL1))
	{
		PrintMessage(LOC_INCORRECT_STEAM_APPID);
		return {};
	}

	PrintMessage(LOC_SIG_CONTROL_PASSED);
	return {};
}

string ResolveRelPath(const string fileName)
{
	if (fileName.empty())
	{
		DebugLog("ERROR: GetLocalPath() called with empty filename");
		return {};
	}

	char currentDir[MAX_PATH] = {};
	if (!GetCurrentDirectoryA(MAX_PATH, currentDir))
	{
		DebugLog("ERROR: GetCurrentDirectoryA failed");
		return {};
	}

	//build full path
	string fullPath = currentDir;

	if (fullPath.back() != '\\')
		fullPath += '\\';

	fullPath += fileName;

	DebugLog("GetLocalPath(): \n" + fullPath);

	return fullPath;
}

string ToShortPath(const string longPath)
{
	if (longPath.empty())
		return {};

	char shortPath[MAX_PATH] = {};
	DWORD result = GetShortPathNameA(
		longPath.c_str(),
		shortPath,
		MAX_PATH);

	if (result == 0 || result >= MAX_PATH)
	{
		DebugLog("ERROR: ToShortPath() failed");
		return {};
	}

	return string(shortPath);
}

string NormalizePath(const string& inputPath)
{
	if (inputPath.empty())
	{
		DebugLog("NormalizePath: empty inputPath!");
		return {};
	}

	char buffer[MAX_PATH] = {};
	DWORD len = GetFullPathNameA(
		inputPath.c_str(),
		MAX_PATH,
		buffer,
		nullptr);

	if (len == 0 || len >= MAX_PATH)
		return {};

	string result(buffer);

	//remove trailing slash (except root like C:\)
	if (result.length() > 3 && result.back() == '\\')
		result.pop_back();

	return result;
}

bool FileExists(const string& filePath)
{
	bool bDumpLogs = false;

	DWORD attr = GetFileAttributesA(filePath.c_str());

	if (bDumpLogs)
	{
		DebugLog("FileExists: " + filePath);

		if (attr == INVALID_FILE_ATTRIBUTES)
		{
			DebugLog("Directory does NOT exist");
		}
		else if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
		{
			DebugLog("Path exists but is NOT a directory");
		}
	}

	return (attr != INVALID_FILE_ATTRIBUTES) &&
		!(attr & FILE_ATTRIBUTE_DIRECTORY);
}

//scan target path and put filenames found into output char array 
bool FindFiles(const string& fileDir, const string& fileMask, vector<string>& findResults)
{
	bool bDumpLogs = false;
	if (bDumpLogs)
	{
		DebugLog("FindFiles: " + fileDir);
		DebugLog("fileMask: " + fileMask);
	}

	findResults.clear();

	WIN32_FIND_DATAA findData;

	//build search pattern
	string searchPath = JoinPath(fileDir, fileMask);

	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		if (bDumpLogs)
		{
			DebugLog("FindFiles: no files found");
			
			DWORD error = GetLastError();
			if(error)
				DebugLog("error code: " + to_string(error));
		}

		return false;
	}

	do
	{
		//skip directories
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			string fullPath = JoinPath(fileDir, findData.cFileName);
			findResults.emplace_back(fullPath);
		}

		if (bDumpLogs)
		{
			DebugLog("FindFiles: found file\n");
			DebugLog(string(findData.cFileName));
		}

	} while (FindNextFileA(hFind, &findData));

	FindClose(hFind);
	return !findResults.empty();
}

bool PathExists(const string& filePath)
{
	return GetFileAttributesA(filePath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool DirectoryExists(const string& path)
{
	DWORD attr = GetFileAttributesA(path.c_str());
	return (attr != INVALID_FILE_ATTRIBUTES) &&
		(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirectoryHasFiles(const string& directory)
{
	if (!DirectoryExists(directory))
		return false;

	vector<string> files;
	return FindFiles(directory, "*.*", files);
}


void Tokenize(const string& input, vector<string>& output, const string& delim)
{
	if (input.empty() || delim.empty())
		return;

	size_t start = 0;

	while (true)
	{
		size_t pos = input.find(delim, start);
		if (pos == string::npos)
		{
			output.emplace_back(input.substr(start));
			break;
		}

		output.emplace_back(input.substr(start, pos - start));
		start = pos + delim.size();
	}
}

string ParseVideoUrl(const string& videoUrl)
{
	/*
	* YouTube video ID:
	* 11 characters
	* allowed: A-Z a-z 0-9 - _
	*
	* supports:
	*  - youtube.com/watch?v=
	*  - youtu.be/
	*  - arbitrary subdomains (m., music., etc.)
	*  - embedded inside larger text
	*
	* ensures:
	*  - exactly 11 chars
	*  - not followed by another valid ID character
	* 
	* ^(?:https?://)?		->	optional http:// or https://
	* (?:[A-Za-z0-9-]+\.)? 	->	optional subdomain (e.g. www., m., music., etc.)
	* (?:youtube\.com/(?:watch\?.*?v=)|youtu\.be/) -> either full youtube watch URL or short youtu.be
	* ([A-Za-z0-9_-]{11})	-> capture exactly 11 valid ID characters
	* (?![A-Za-z0-9_-])		-> ensure the 11-character ID is not followed by another valid ID character
	*/

	static const regex ytRegex(
		R"((?:https?://)?(?:[A-Za-z0-9-]+\.)?(?:youtube\.com/(?:watch\?.*?v=)|youtu\.be/)([A-Za-z0-9_-]{11})(?![A-Za-z0-9_-]))",
		regex::icase
	);

	smatch match;

	if (regex_search(videoUrl, match, ytRegex))
		return match[1].str(); //captured video ID

	return {};
}

bool ParseBool(const string& strBool)
{
	string value = strBool; //need a copy because strBool comes as const ref
	
	TrimWhitespace(value); //accepts "  true" or "  false"
	ToLower(value); //accepts "True" or "FALSE"

	if (value == "true" || value == "1" ||
		value == "yes" || value == "on")
		return true;

	if (value == "false" || value == "0" ||
		value == "no" || value == "off")
		return false;

	DebugLog("ParseBool: invalid argument " + value);
	return false;
}

bool ParseUint64(const string& input, uint64& outValue)
{
	string str = input;
	
	TrimWhitespace(str); //accepts "  123" or "1 "

	if (str.empty())
		return false;

	uint64 value = 0;

	for (char singleChar : str)
	{
		if (!isdigit(static_cast<unsigned char>(singleChar)))
			return false;

		uint64 digit = static_cast<uint64>(singleChar - '0');

		//check overflow before multiplying
		if (value > (numeric_limits<uint64_t>::max() - digit) / 10)
			return false;

		value = value * 10 + digit;
	}

	outValue = value;
	return true;
}

bool UserPrompt(const string& question)
{
	while (!g_bUserInterrupted)
	{
		WarningMessage(question + " (Y/N)");

		char input = 0;
		cin >> input;

		//clear remaining input buffer (important if user typed "yes", etc.)
		cin.ignore(numeric_limits<streamsize>::max(), '\n');

		if (input == 'Y' || input == 'y')
			return true;

		if (input == 'N' || input == 'n')
			return false;

		ErrorMessage(LOC_PROMPT_INVALID_INPUT);
	}

	return false;
}

bool RequestString(const std::string& question, std::string& outValue)
{
	WarningMessage(question);

	if (!getline(cin, outValue))
		return false; //stream failure (EOF, redirected input ended)

	TrimWhitespace(outValue);

	return true;
}

bool RequestInt(const string& question, int& outValue)
{
	string userInput;

	if (!RequestString(question, userInput))
		return false;

	try
	{
		size_t pos = 0;
		int intValue = stoi(userInput, &pos);

		if (pos != userInput.length())
			return false; //trailing garbage

		outValue = intValue;
		return true;
	}
	catch (invalid_argument)
	{
		DebugLog("RequestInt: invalid_argument exception");
		DebugLog("   userInput: " + userInput);

		return false;
	}

	catch (out_of_range)
	{
		DebugLog("RequestInt: out_of_range exception");
		DebugLog("   userInput: " + userInput);

		return false;
	}
}

void PrintMessage(const string& message, ConsoleTextColor color)
{
	bool bChangeColor = false;
	if (color != ConsoleTextColor::Default)
		bChangeColor = true;

	if(bChangeColor)
		SetTextColor(color);

    printf("%s\n", message.c_str());

	if (bChangeColor)
		SetTextColor(ConsoleTextColor::Default);
}

void WarningMessage(const string& message)
{
	PrintMessage(message, ConsoleTextColor::Yellow);
}

void ErrorMessage(const string& message)
{
	PrintMessage(message, ConsoleTextColor::Red);
}

void SuccessMessage(const string& message)
{
	PrintMessage(message, ConsoleTextColor::Green);
}

void PrintProgress(const uint64 processed, const uint64 total, const string& prefix)
{
	int percent = total ? int((processed * 100ULL) / total) : 0; //100ULL to force 64-bit math

	if (percent > 100)
		percent = 100;

	string line;

	if (processed != 0 && total != 0)
		line = prefix + LOC_PROGRESS + to_string(percent) + "%";
	else
		line = prefix;

	cout << "\r" << line;

	//erase leftover characters
	if (line.length() < g_lastConsoleLineLength)
		cout << string(g_lastConsoleLineLength - line.length(), ' ');

	cout << flush;

	g_lastConsoleLineLength = line.length();
}

void PrintSpinner(const std::string message)
{
	static const char spinner[] = { '|', '/', '-', '\\' };
	
	auto now = chrono::steady_clock::now();
	auto elapsed = chrono::duration_cast<std::chrono::milliseconds>(now - g_lastSpinnerUpdate).count();
	
	const int freq = 90; //update frequency in milliseconds
	
	if (elapsed >= freq)
	{
		uint64 steps = elapsed / freq;

		g_spinnerIndex += steps;
		g_lastSpinnerUpdate += chrono::milliseconds(steps * freq);
	}

	string line = message + spinner[static_cast<size_t>(g_spinnerIndex % 4)];
	cout << "\r" << line;

	//erase leftover characters
	if (line.length() < g_lastConsoleLineLength)
		cout << string(g_lastConsoleLineLength - line.length(), ' ');

	cout << flush;
	g_lastConsoleLineLength = line.length();
}

void ClearLine()
{
	cout << "\r\033[2K" << flush;  // \033[2K = clear entire line
}

void SetTextColor(ConsoleTextColor color)
{
	WORD consoleColor;
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	switch (color)
	{
	case(ConsoleTextColor::Red):
		consoleColor = FOREGROUND_RED | FOREGROUND_INTENSITY;
		break;

	case(ConsoleTextColor::Green):
		consoleColor = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;

	case(ConsoleTextColor::Yellow):
		consoleColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;

	case(ConsoleTextColor::White):
		consoleColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		break;

	case(ConsoleTextColor::Cyan):
		consoleColor = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		break;

	default:
		consoleColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		break;
	}

	SetConsoleTextAttribute(hConsole, consoleColor);
}

void SetCursorVisible(bool bIsVisible)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	CONSOLE_CURSOR_INFO cursorInfo;
	GetConsoleCursorInfo(hConsole, &cursorInfo);

	cursorInfo.bVisible = bIsVisible;
	SetConsoleCursorInfo(hConsole, &cursorInfo);
}

void PressAnyKey()
{
	PrintMessage(LOC_PRESS_ANY_KEY, ConsoleTextColor::White);
	_getch();
}

void DebugLog(const string& message)
{
#if defined(_DEBUG) && defined(DUMP_LOGS)
    EnterCriticalSection(&g_LogCriticalSection);

    ofstream file(DEBUG_LOG_FILE_MASK, ios::out | ios::app);
    if (!file.is_open())
        return;

    file << message << endl;

    LeaveCriticalSection(&g_LogCriticalSection);
#endif
}

void DebugLog(const char* message)
{
    if (!message)
        return;

    DebugLog(string(message));
}

void DebugLogStrings(const vector<string>& vectorInput)
{
	if (vectorInput.empty())
		DebugLog("empty!");
	else
	{
		for (string vectorItem : vectorInput)
			DebugLog(vectorItem);
	}
}