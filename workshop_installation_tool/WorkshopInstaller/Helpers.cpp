#include "Helpers.h"

CRITICAL_SECTION g_LogCriticalSection; //allows multiple threads to access DebugLog() or other important functions

bool ReadSteamAppID(const char* filename, std::string& outAppID)
{
	std::ifstream file(filename, std::ios::in | std::ios::binary);
	if (!file.is_open())
		return false;

	std::string contents;
	std::getline(file, contents);

	//remove UTF-8 BOM if present
	if (contents.size() >= 3 &&
		(unsigned char)contents[0] == 0xEF &&
		(unsigned char)contents[1] == 0xBB &&
		(unsigned char)contents[2] == 0xBF)
	{
		contents.erase(0, 3);
	}

	//trim whitespace (both ends)
	auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };

	while (!contents.empty() && isSpace(contents.front()))
		contents.erase(contents.begin());
	while (!contents.empty() && isSpace(contents.back()))
		contents.pop_back();

	if (contents.empty())
		return false;

	//validate numeric
	for (char c : contents)
	{
		if (c < '0' || c > '9')
			return false;
	}

	outAppID = contents;
	return true;
}

//this function must _not_ generate .bak filenames due to policy-rich backup rules of the installer
bool BackupFile(String^ destRoot, String^ relTargetPath, String^ relBackupPath, StreamWriter^ uninstallLog)
{
	String^ absTarget = Path::Combine(destRoot, relTargetPath);
	if (!File::Exists(absTarget))
	{
		DebugLog("BackupFile: skipping backup, source is missing!");
		DebugLog("   " + absTarget);
		DebugLog("");

		return false;
	}

	String^ absBackup = Path::Combine(destRoot, relBackupPath);

	String^ backupDir = Path::GetDirectoryName(absBackup);
	if (!Directory::Exists(backupDir))
		Directory::CreateDirectory(backupDir);

	DebugLog("Backup:");
	DebugLog("   " + absTarget);
	DebugLog("   " + absBackup);

	File::Copy(absTarget, absBackup, true);

	if (uninstallLog)
		uninstallLog->WriteLine(relBackupPath);

	return true;
}

void DebugLog(String^ text)
{
#ifdef _DEBUG
#ifdef DUMP_LOGS
	String^ errorMessage = nullptr;
	EnterCriticalSection(&g_LogCriticalSection);

	String^ exeDir = AppDomain::CurrentDomain->BaseDirectory;
	String^ logPath = Path::Combine(exeDir, DEBUG_LOG_FILENAME);

	try
	{
		File::AppendAllText(
			logPath,
			text + Environment::NewLine,
			Encoding::UTF8
		);
	}
	catch (UnauthorizedAccessException^)
	{
		errorMessage = String::Format(LOC_DBG_ACCESS_DENIED, logPath);
	}
	catch (Exception^ ex)
	{
		errorMessage = ex->Message;
	}

	LeaveCriticalSection(&g_LogCriticalSection);

	if(errorMessage != nullptr)
		MAIN->ErrorMessage("DebugLog: " + errorMessage);
#endif
#endif
}

//we're looking for files ending with .bak or .bak<0000000> (file.bak, file.bak12, file.bak999)
bool IsBakFile(String^ fileName)
{
	if (String::IsNullOrEmpty(fileName))
		return false;

	FileInfo^ file = gcnew FileInfo(fileName);
	String^ fileNameWithSuffix = file->Name;

	//files like ".bak<0000000>" with suffix
	int idx = fileNameWithSuffix->LastIndexOf(".bak", StringComparison::OrdinalIgnoreCase);
	if (idx < 0)
		return false;

	String^ suffix = fileNameWithSuffix->Substring(idx + 4);
	if (suffix->Length == 0)
		return true; //plain ".bak"

	UInt64 dummy = 0;
	return UInt64::TryParse(suffix, dummy);
}

bool IsJvmFile(String^ relPath)
{
	return relPath->EndsWith(".java", StringComparison::OrdinalIgnoreCase) ||
		relPath->EndsWith(".class", StringComparison::OrdinalIgnoreCase);
}

bool IsInsideBakDir(String^ fileName)
{
	FileInfo^ file = gcnew FileInfo(fileName);

	if (!file)
		return false;

	String^ dir = file->DirectoryName; //format is "C:\MyStuff"
	if (String::IsNullOrEmpty(dir))
		return false;

	cli::array<String^>^ pathParts = dir->Split(Path::DirectorySeparatorChar); //cli::array since we're using namespace std and Intellisence thinks it's std::array here
	for each (String^ partStr in pathParts)
	{
		if (partStr->StartsWith(".bak", StringComparison::OrdinalIgnoreCase) && partStr->Length > 4)
		{
			String^ suffix = partStr->Substring(4);
			int dummy;
			if (Int32::TryParse(suffix, dummy))
				return true;
		}
	}

	return false;
}

//tell us whether relative path (relPath) belongs to a given top-level directory (dir)
bool IsUnderDir(String^ relPath, String^ dir)
{
	return relPath->StartsWith(dir + "\\", StringComparison::OrdinalIgnoreCase);
}

//these files will be skipped during conflict checks
bool IsConflictRelevantFile(String^ relPath)
{
	if (relPath->Equals("imagecode.png"))
		return false;

	return true;
}

//retuns a path like "cars\racers\supra\scripts\chassis.cfg", i.e. with normalized slashes and relative to the rootPath (GetGamePath() or any other root path)
String^ GetNormalizedRelativePath(String^ rootPath, String^ fullPath)
{
	if (String::IsNullOrWhiteSpace(rootPath) || String::IsNullOrWhiteSpace(fullPath))
		return nullptr;

	fullPath = fullPath->Replace('/', '\\')->Trim();

	String^ gameRoot = rootPath->TrimEnd('\\') + "\\";

	//must be under the root path
	if (!fullPath->StartsWith(rootPath, StringComparison::OrdinalIgnoreCase))
		return nullptr;

	String^ relPath = fullPath->Substring(rootPath->Length);

	//safety checks
	if (String::IsNullOrWhiteSpace(relPath))
		return nullptr;

	if (relPath->Contains(".."))
		return nullptr;

	while (relPath->StartsWith("\\"))
		relPath = relPath->Substring(1);

	return relPath;
}

char* BrowseForDirectory()
{
	FolderBrowserDialog^ dialog = gcnew FolderBrowserDialog();

	dialog->Description = LOC_BROWSE_FOR_DIR;
	dialog->ShowNewFolderButton = false;

	DialogResult result = dialog->ShowDialog();

	if (result != DialogResult::OK)
		return nullptr;

	if (String::IsNullOrEmpty(dialog->SelectedPath))
		return nullptr;

	return StringToUtf8(dialog->SelectedPath);
}

uint64 ItemIdFromString(String^ strItemId)
{
	uint64 itemId = 0;
	UInt64::TryParse(strItemId, itemId);

	return itemId;
}

uint64 ItemIdFromSentinelFileName(String^ fileName)
{
	uint64 result = 0;
	String^ fileNameMask = gcnew String(SENTINEL_MASK);

	if (!fileName->EndsWith(fileNameMask, StringComparison::OrdinalIgnoreCase))
	{
		result = 0;
	}
	else
	{
		String^ strItemId = fileName->Substring(0, fileName->Length - fileNameMask->Length);
		UInt64::TryParse(strItemId, result);
	}

	return result;
}

char* StringToUtf8(String^ str)
{
	if (!str)
		return nullptr;

	cli::array<Byte>^ bytes = Encoding::UTF8->GetBytes(str);

	char* buf = new char[bytes->Length + 1];

	pin_ptr<Byte> pinned = &bytes[0];
	memcpy(buf, pinned, bytes->Length);

	buf[bytes->Length] = '\0';
	return buf;
}

uint64 GetTimeNow()
{
	//trying to access Steam server time first
	uint64 steamTime = SteamUtils()->GetServerRealTime();
	if (steamTime)
		return steamTime * 1000ULL; //seconds to ms

	return GetTickCount64(); //already ms
}

String^ TimeToString(int32 time)
{
	if (time <= 0)
		return gcnew String("N/A");

	// time is seconds since Unix epoch (UTC)
	DateTime epoch(1970, 1, 1, 0, 0, 0, DateTimeKind::Utc);
	DateTime dt = epoch.AddSeconds(time);

	//explicit invariant culture for stable formatting
	return dt.ToString("yyyy-MM-dd HH:mm:ss 'UTC'", Globalization::CultureInfo::InvariantCulture);
}
