/*
 * =============================================================================
 *  Street Legal Workshop Installer
 * =============================================================================
 *  Copyright ImageCode LLC.
 * 
 *  This project is not affiliated with, endorsed by, or sponsored by Valve
 *  Corporation. Steam and Steam Workshop are trademarks of Valve Corporation.
 * =============================================================================
 */

//todo: detect exceptions BEFORE copying files
//todo: currently v140 runtime is required. how many users have it? any way to avoid v140 runtime error on launch?

//todo: "offline mode" that allows to cache/install items from the workshop cache folder

#include "Main.h"

bool g_EnableOverwrittenColumn;
bool g_PromptUserOnManageItem;
bool g_UseFictiveJavaFiles;
bool g_UseFictiveClassFiles;

CMainModule* CMainModule::m_ModuleInstance = 0;
[STAThreadAttribute]
int main()
{
	Application::EnableVisualStyles();
	Application::SetCompatibleTextRenderingDefault(false);

	if (MAIN->InitModule())
		Application::Run(ManagerForm::GetInstance());


#ifdef _DEBUG
	_CrtDumpMemoryLeaks(); //throws debug output on termination, requires "Attach to process" and only applies to C++ stuff
#endif

	return 0;
}

#ifdef USE_MUTEX
HANDLE g_hInstanceMutex = NULL;

bool CMainModule::EnsureSingleInstance()
{
	g_hInstanceMutex = CreateMutexW(
		NULL,
		TRUE, //request initial ownership
		L"Global\\StreetLegalWorkshopInstaller"
	);

	if (!g_hInstanceMutex)
		return true; //fail open, don't brick installer

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(g_hInstanceMutex);
		g_hInstanceMutex = NULL;

		return false; //another instance is running
	}

	return true;
}
#endif

CMainModule::CMainModule()
{
#ifdef USE_MUTEX
	if (!EnsureSingleInstance())
		ErrorMessage(LOC_MUTEX_ALREADY_RUNNING);
#endif

	m_pchUserDefinedGamePath = nullptr;
	m_bAskUserForGamePath = false;
	m_bDemoMode = false;

	bool bIsStreetLegal = false;
	AppId_t appIdFromFile = ReadSteamAppId();

	if (appIdFromFile == STEAM_APPID_SL1 || appIdFromFile == STEAM_APPID_SLRR)
	{
		bIsStreetLegal = true;
		m_SteamAppId = appIdFromFile;
	}
	else if (m_SteamAppId == 0) //ReadSteamAppId() may return 0 if there's no steam_appid.txt or there's garbage inside
	{
		m_SteamAppId = STEAM_APPID_SLRR;
	}
	else
	{
		ErrorMessage(String::Format(LOC_INCORRECT_APPID, STEAM_APPID_FILE)); //not Street Legal appId
	}

	m_ModuleInstance = this;
	m_bLoadingComplete = false;

	InitializeCriticalSection(&g_LogCriticalSection);
}

CMainModule::~CMainModule()
{
	Shutdown();
	DeleteCriticalSection(&g_LogCriticalSection);
}

bool CMainModule::InitModule()
{
	if (!SteamAPI_Init())
		ErrorMessage(LOC_API_FAIL);
	else
	{
		GetConfigOptions();

		String^ gamePath = GetGamePath();
		if (String::IsNullOrEmpty(gamePath))
			m_bDemoMode = true;

		QueryWorkshopState();

		if (!m_nWorkshopNumSubscribedItems)
		{
#ifdef _DEBUG
			WarningMessage(LOC_NO_SUBSCRIBED_ITEMS); //can continue in debug mode
#else
			ErrorMessage(LOC_NO_SUBSCRIBED_ITEMS);
#endif
		}
		else
		{
			if (m_nWorkshopNumSubscribedItems > MAX_WORKSHOP_ITEMS)
				m_nWorkshopNumSubscribedItems = MAX_WORKSHOP_ITEMS;
		}

#ifdef _DEBUG
		DebugLog("m_SteamAppId: " + m_SteamAppId);
		DebugLog("Game path: " + GetGamePath());
		DebugLog("Steam path: " + GetSteamPath());
		DebugLog("Workshop path: " + GetWorkshopPath());
		DebugLog("Uninstall root: " + GetUninstallRoot());
		DebugLog("Subscribed items: " + m_nWorkshopNumSubscribedItems); //SteamUGC may return fewer items than the user has actually subscribed
		DebugLog("");
#endif
	}

	return true;
}

void CMainModule::RefreshInstallerState()
{
	if (!m_bLoadingComplete)
	{
		for (auto& item : m_vecWorkshopItems)
		{
			if (item->m_LoadState == CWorkshopItem::WorkshopItemLoadState::Requesting &&
				GetTimeNow() - item->m_u64RequestTime > METADATA_TIMEOUT)
			{
				DebugLog("RefreshInstallerState()");
				DebugLog("   METADATA_TIMEOUT for itemId " + item->GetItemId());

				item->m_LoadState = CWorkshopItem::WorkshopItemLoadState::Failed;
			}
		}

		if (AllWorkshopItemsResolved())
		{
			CleanupOrphanSentinels();

			bool bFoundBrokenInstalls = false;

			//check if there are any broken items left after previous session
			for (auto& item : m_vecWorkshopItems)
			{
				uint64 itemId = item->GetItemId();
				if (IsInstallInterrupted(itemId))
				{
					bool bInstallVerified = VerifyInstallation(itemId);

					if (!bInstallVerified)
					{
						//only mark broken if there is evidence of a past install
						if (GetInstalledFiles(itemId)->Count > 0)
						{
							MarkInstallationBroken(itemId, true);
							bFoundBrokenInstalls = true;
						}
						else
						{
							DebugLog("deleting orphan/ambiguous sentinel for item " + itemId);
							DeleteSentinelFile(itemId);
						}
					}
				}

				DebugLog(String::Format("GetItemCachePath({0})", item->GetItemId()));
				DebugLog("   " + gcnew String(GetItemCachePath(item.get())));
				DebugLog("");
			}

			UIState::NeedsFullUpdate = true;
			m_bLoadingComplete = true;

			//if any broken installs have been found, show warning message to the user
			if (bFoundBrokenInstalls)
				WarningMessage(LOC_BROKEN_INSTALLS_FOUND);
		}
	}
}

void CMainModule::MarkInstallationBroken(uint64 itemId, bool bIsBroken)
{
	CWorkshopItem* item = FindWorkshopItem(itemId);
	bool bRequestUpdateUI = (item->m_bIsBroken != bIsBroken);

	if (!item)
	{
		DebugLog(String::Format("MarkInstallationBroken({0}): workshop item not found!", itemId));
		return;
	}

	if (bIsBroken)
	{
		item->m_bIsInstalled = false;
		DebugLog(String::Format("MarkInstallationBroken({0}): item marked as broken and needs reinstall!", itemId));
	}	

	item->m_bIsBroken = bIsBroken;

	//refresh UI only if necessary
	if (bRequestUpdateUI)
		UIState::NeedsFullUpdate = true;
}

String^ CMainModule::FormatItemState(uint32 state)
{
	if (state == k_EItemStateNone)
		return "k_EItemStateNone";

	List<String^>^ flags = gcnew List<String^>();

	if (state & k_EItemStateSubscribed)
		flags->Add("k_EItemStateSubscribed");

	if (state & k_EItemStateLegacyItem)
		flags->Add("k_EItemStateLegacyItem");

	if (state & k_EItemStateInstalled)
		flags->Add("k_EItemStateInstalled");

	if (state & k_EItemStateNeedsUpdate)
		flags->Add("k_EItemStateNeedsUpdate");

	if (state & k_EItemStateDownloading)
		flags->Add("k_EItemStateDownloading");

	if (state & k_EItemStateDownloadPending)
		flags->Add("k_EItemStateDownloadPending");

	if (state & k_EItemStateDisabledLocally)
		flags->Add("k_EItemStateDisabledLocally");

	return String::Join(" | ", flags);
}

String^ CMainModule::GetSteamPath()
{
	auto IsValidSteamRoot = [](String^ path) -> bool
	{
		if (String::IsNullOrEmpty(path))
			return false;

		return true;
	};

	//attempt to access registry first
	try
	{
		HKEY hKey = nullptr;
		const char* keys[] =
		{
			"SOFTWARE\\WOW6432Node\\Valve\\Steam",
			"SOFTWARE\\Valve\\Steam"
		};

		for (int i = 0; i < _countof(keys); ++i)
		{
			if (RegOpenKeyExA(
				HKEY_LOCAL_MACHINE,
				keys[i],
				0,
				KEY_READ,
				&hKey) == ERROR_SUCCESS)
			{
				char buf[MAX_PATH] = {};
				DWORD size = sizeof(buf);

				if (RegQueryValueExA(
					hKey,
					"InstallPath",
					nullptr,
					nullptr,
					(LPBYTE)buf,
					&size) == ERROR_SUCCESS)
				{
					String^ candidate = gcnew String(buf);
					if (IsValidSteamRoot(candidate))
					{
						RegCloseKey(hKey);
						return candidate;
					}
				}

				RegCloseKey(hKey);
			}
		}
	}
	catch (Exception^ e)
	{
		DebugLog("GetSteamPath() method #1 exception: " + e->Message);
	}

	//secondary approach, try to get path from SteamAPI
	try
	{
		const char* steamPath = SteamAPI_GetSteamInstallPath();
		if (steamPath && steamPath[0])
		{
			String^ candidate = gcnew String(steamPath);
			if (IsValidSteamRoot(candidate))
				return candidate;
		}
	}
	catch (Exception^ e)
	{
		DebugLog("GetSteamPath() method #2 exception: " + e->Message);
	}

	ErrorMessage(LOC_GAME_PATH_FAIL);
	return nullptr;
}

AppId_t CMainModule::GetSteamAppId()
{
	return m_SteamAppId;
}

AppId_t CMainModule::ReadSteamAppId()
{
	AppId_t appId = 0;
	String^ filePath = STEAM_APPID_FILE;

	if (!File::Exists(filePath))
		return appId;

	//reads entire file and trims whitespace/newlines
	String^ strFileText = File::ReadAllText(filePath)->Trim();

	if (strFileText)
		UInt32::TryParse(strFileText, appId);

	return appId;
}

String^ CMainModule::GetInstallerPath()
{
	String^ installerExe = System::Reflection::Assembly::GetExecutingAssembly()->Location;
	String^ installerExePath = Path::GetDirectoryName(installerExe);

	if (!String::IsNullOrEmpty(installerExePath))
		return installerExePath;

	return nullptr;
}

String^ CMainModule::GetGamePath()
{
	//if user has defined game path already or it's imported from config file
	if (m_pchUserDefinedGamePath)
	{
		DebugLog("GetGamePath(): m_pchUserDefinedGamePath is " + gcnew String(m_pchUserDefinedGamePath));
		return gcnew String(m_pchUserDefinedGamePath);
	}

	String^ gamePath = nullptr;

	//1. game dir could be the same as installer dir
	gamePath = GetInstallerPath();

	//filter misleading paths
	if (VerifyGamePath(gamePath))
		return gamePath;

	//2. if nothing next to the installer has been found, asking SteamAPI
	char buffer[MAX_PATH] = {};
	uint32 size = SteamApps()->GetAppInstallDir(GetSteamAppId(), buffer, sizeof(buffer));
	if (size != 0)
		gamePath = gcnew String(buffer);

	if (VerifyGamePath(gamePath))
		return gamePath;

	DebugLog("GetGamePath(): path not found! asking the user with UserPrompt()");

	if (!m_bAskUserForGamePath) //ask only once
	{
		if (UserPrompt(LOC_GAME_PATH_PROMPT))
		{
			m_pchUserDefinedGamePath = BrowseForDirectory();
			gamePath = gcnew String(m_pchUserDefinedGamePath);

			ManagerForm::GetInstance()->BringToFront();

			DebugLog("   user defined path: " + gamePath);
		}
		else
		{
			gamePath = nullptr;
		}

		m_bAskUserForGamePath = true;
		return gamePath;
	}

	return nullptr;
}

bool CMainModule::VerifyGamePath(String^ gamePath)
{
	if (String::IsNullOrEmpty(gamePath))
		return false;

	if (!Directory::Exists(gamePath))
		return false;

	//skip folders that don't have main rpk file
	String^ systemRpkPath = Path::Combine(gamePath, "system.rpk");
	if (!File::Exists(systemRpkPath))
		return false;

	int matchScore = 0;
	int scoreThreshold = 3; //accept any combined results above this threshold

	String^ exeMaskSL2 = "StreetLegal_Redline";
	String^ exeMaskSL1R = "SL_REVision";
	String^ exeMaskSL1R_launcher = "SL_Launcher";
	
	String^ folderMaskSL2 = "Street Legal Racing Redline";
	String^ folderMaskSL1R = "Street Legal 1 REVision";

	//1. try to find any exe that matches the mask
	cli::array<String^>^ exeFiles;
	try
	{
		exeFiles = Directory::GetFiles(gamePath, "*.exe", SearchOption::TopDirectoryOnly);
	}
	catch (Exception^ e)
	{
		DebugLog("VerifyGamePath() IO exception:");
		DebugLog("   " + e->Message);

		return false; //access denied or IO error
	}

	for each (String^ exePath in exeFiles)
	{
		String^ exeName = Path::GetFileNameWithoutExtension(exePath);

		//direct name match
		if (exeName->IndexOf(exeMaskSL2, StringComparison::OrdinalIgnoreCase) >= 0 ||
			exeName->IndexOf(exeMaskSL1R, StringComparison::OrdinalIgnoreCase) >= 0 ||
			exeName->IndexOf(exeMaskSL1R_launcher, StringComparison::OrdinalIgnoreCase) >= 0)
		{
			matchScore += 5;
			continue;
		}

		//if any other exe is found, look into original filename
		try
		{
			FileVersionInfo^ info = FileVersionInfo::GetVersionInfo(exePath);
			if (!String::IsNullOrEmpty(info->OriginalFilename))
			{
				if (info->OriginalFilename->IndexOf(exeMaskSL2, StringComparison::OrdinalIgnoreCase) >= 0 ||
					info->OriginalFilename->IndexOf(exeMaskSL1R, StringComparison::OrdinalIgnoreCase) >= 0)
				{
					matchScore += 3;
				}
			}
		}
		catch (Exception^ e)
		{
			DebugLog("VerifyGamePath() metadata exception:");
			DebugLog("   " + e->Message);
		}
	}

	//2. folder name heuristic (weak signal)
	String^ folderName = Path::GetFileName(gamePath->TrimEnd(Path::DirectorySeparatorChar));

	if (!String::IsNullOrEmpty(folderName))
	{
		if (folderName->IndexOf(folderMaskSL2, StringComparison::OrdinalIgnoreCase) >= 0 ||
			folderName->IndexOf(folderMaskSL1R, StringComparison::OrdinalIgnoreCase) >= 0)
		{
			matchScore += 2;
		}
	}

	return matchScore >= scoreThreshold;
}

String^ CMainModule::GetUninstallRoot()
{
	String^ gamePath = GetGamePath();
	if (String::IsNullOrEmpty(gamePath))
	{
		String^ installerPath = GetInstallerPath();
		return installerPath;
	}

	return Path::Combine(gamePath, UNINSTALL_MASK);
}


String^ CMainModule::GetUninstallFilePath(CWorkshopItem* item)
{
	if (!item)
		return nullptr;

	//uninstall log file is named exactly as itemId
	String^ uninstFilePath = Path::Combine(GetUninstallRoot(), item->GetItemId().ToString());

	//no uninstall log -> nothing installed
	if (!File::Exists(uninstFilePath))
		return nullptr;

	return uninstFilePath;
}

String^ CMainModule::GetSentinelFilePath(uint64 itemId)
{
	return Path::Combine(GetUninstallRoot(), itemId.ToString() + SENTINEL_MASK);
}

String^ CMainModule::GetWorkshopPath()
{
	String^ workshopPath = Path::Combine(
		GetSteamPath(), //Steam dir, NOT game dir!
		"steamapps",
		"workshop",
		"content",
		GetSteamAppId().ToString()
	);
	
	return workshopPath;
}

String^ CMainModule::GetItemCachePath(CWorkshopItem* item)
{
	if (!item)
		return nullptr;

	char installDir[MAX_PATH] = {};
	bool bInstallDirExists = item->TryGetInstallDir(installDir, sizeof(installDir));

	if (!bInstallDirExists)
		return nullptr;

	return gcnew String(installDir);
}

List<String^>^ CMainModule::GetFilesToInstall(uint64 itemId, FileCollectMode collectMode)
{
	DebugLog(String::Format("GetFilesToInstall({0})", itemId));
	
	if (collectMode == FC_JVM_FILES_ONLY)
		DebugLog("collectMode: FC_JVM_FILES_ONLY (only .java and .class files)");
	else if(collectMode == FC_NO_JVM_FILES)
		DebugLog("collectMode: FC_NO_JVM_FILES (excluding .java and .class files)");

	List<String^>^ result = gcnew List<String^>();

	CWorkshopItem* item = FindWorkshopItem(itemId);
	if (!item)
	{
		DebugLog("GetFilesToInstall: item is NULL!");
		return result;
	}

	String^ itemPath = GetItemCachePath(item);
	DebugLog("srcPath: " + itemPath);

	DirectoryInfo^ dirInfo = gcnew DirectoryInfo(itemPath);

	//exclude files with certain names
	List<String^>^ fileNamesToFilter = gcnew List<String^>();
	fileNamesToFilter->Add("imagecode.png");

	for each (FileInfo ^ file in dirInfo->GetFiles("*", SearchOption::AllDirectories))
	{
		String^ fileName = file->FullName;

		DebugLog("iterate file: " + fileName);

		if (IsBakFile(fileName)) //skip files ending with .bak<number> (file.bak, file.bak12, file.bak999)
		{
			DebugLog(fileName + " is a .bak file, continue");
			continue;
		}
		else if (IsInsideBakDir(fileName))
		{
			DebugLog(fileName + " is inside bak dir, continue");
			continue;
		}
		else if (fileNamesToFilter->Contains(file->Name)) //untested!
		{
			DebugLog(fileName + " is inside a fileNamesToFilter");
			continue;
		}
		else
		{
			String^ relPath = GetNormalizedRelativePath(itemPath, fileName); //relative to itemPath, not to the GetWorkshopPath()
			
			if (relPath)
			{
				String^ fileExt = Path::GetExtension(relPath);
				bool bIsJvmFile = false;

				if (fileExt->Equals(".java", StringComparison::OrdinalIgnoreCase) ||
					fileExt->Equals(".class", StringComparison::OrdinalIgnoreCase))
				{
					bIsJvmFile = true;
				}

				if (collectMode == FC_NO_JVM_FILES) //skip .java/.class files
				{
					if (!bIsJvmFile)
						result->Add(relPath);
				}
				else if (collectMode == FC_JVM_FILES_ONLY) //include .java/.class files, skip everything else
				{
					if (bIsJvmFile)
						result->Add(relPath);
				}
				else
				{
					result->Add(relPath); //no file extension filter
				}
			}
				
		}
	}

	DebugLog("result:");
	for each (String ^ fileName in result)
	{
		DebugLog(fileName);
	}
	DebugLog("");

	return result;
}

List<String^>^ CMainModule::GetCachedFiles(uint64 itemId)
{
	DebugLog("GetCachedFiles(" + itemId + ")");

	auto result = gcnew List<String^>();

	if (itemId == 0)
		return result;

	CWorkshopItem* item = FindWorkshopItem(itemId);
	if (!item)
		return result;

	String^ itemCachePath = GetItemCachePath(item);

	//cache dir not found for this item
	DirectoryInfo^ rootDir = gcnew DirectoryInfo(itemCachePath);
	if (!rootDir->Exists)
		return result;

	try
	{
		//enumerate directories first
		for each (DirectoryInfo ^ dir in rootDir->EnumerateDirectories("*", SearchOption::AllDirectories))
		{
			String^ relPath = dir->FullName->Substring(rootDir->FullName->Length + 1);
			result->Add(relPath);

			DebugLog("   " + relPath);
		}

		//enumerate files
		for each (FileInfo ^ file in rootDir->EnumerateFiles("*", SearchOption::AllDirectories))
		{
			String^ relPath = file->FullName->Substring(rootDir->FullName->Length + 1);
			result->Add(relPath);

			DebugLog("   " + relPath);
		}
	}
	catch (Exception^ ex)
	{
		DebugLog("WARNING: failed to enumerate cache files for item " + itemId);
		DebugLog(ex->ToString());
	}

	return result;
}

HashSet<String^>^ CMainModule::GetInstalledFiles(uint64 itemId)
{
	DebugLog(String::Format("GetInstalledFiles({0})", itemId));

	auto result = gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

	CWorkshopItem* item = FindWorkshopItem(itemId);
	if (!item)
		return result;

	String^ uninstFilePath = GetUninstallFilePath(item);
	if (uninstFilePath)
	{
		DebugLog("reading uninstall file:");
		for each (String ^ line in File::ReadLines(uninstFilePath))
		{
			if (String::IsNullOrWhiteSpace(line))
				continue;

			DebugLog("  " + line);
			result->Add(line);
		}
	}
	else
	{
		if(item->m_bIsInstalled)
			DebugLog("ERROR: uninstFilePath is nullptr!");
	}

	DebugLog("");

	return result;
}

Dictionary<String^, List<uint64>^>^ CMainModule::BuildFileOwnerMap()
{
	auto ownersDict =
		gcnew Dictionary<String^, List<uint64>^>(StringComparer::OrdinalIgnoreCase);

	String^ gamePath = GetGamePath();

	for each (String^ strItemId in GetInstalledWorkshopItems())
	{
		uint64 itemId = ItemIdFromString(strItemId);
		if (!itemId)
			continue;

		CWorkshopItem* item = FindWorkshopItem(itemId);
		if (!item || item->m_bIsBroken || !item->m_bIsInstalled)
			continue;

		auto filesSet = GetInstalledFiles(itemId);

		for each (String^ relPath in filesSet)
		{
			String^ normPath = relPath->Trim()->ToLowerInvariant();
			normPath = normPath->Replace("/", "\\");

			if (String::IsNullOrWhiteSpace(normPath))
				continue;

			if (IsBakFile(normPath) || IsInsideBakDir(normPath))
				continue;

			if (!ownersDict->ContainsKey(normPath))
				ownersDict[normPath] = gcnew List<uint64>();

			if (!ownersDict[normPath]->Contains(itemId))
				ownersDict[normPath]->Add(itemId);
		}
	}

	bool bDumpDict = false;

	if (bDumpDict)
	{
		DebugLog("---- fileOwnerMulti dump BEGIN ----");

		for each (auto kv in ownersDict)
		{
			String^ filePath = kv.Key;
			List<uint64>^ owners = kv.Value;

			StringBuilder^ line = gcnew StringBuilder();
			line->Append(filePath);
			line->Append(" -> ");

			for (int i = 0; i < owners->Count; ++i)
			{
				line->Append(owners[i]);

				if (i < owners->Count - 1)
					line->Append(", ");
			}

			DebugLog(line->ToString());
		}

		DebugLog("---- fileOwnerMulti dump END ----");
	}

	return ownersDict;
}

bool CMainModule::CheckFileConflicts(uint64 itemId, List<String^>^ candidateFiles)
{
	if (!candidateFiles || candidateFiles->Count == 0)
		return true;

	//build authoritative ownership map once
	Dictionary<String^, List<UInt64>^>^ fileOwners = BuildFileOwnerMap();

	//ownerId -> list of conflicting files
	Dictionary<String^, HashSet<UInt64>^>^ fileConflicts =
		gcnew Dictionary<String^, HashSet<UInt64>^>(StringComparer::OrdinalIgnoreCase);

	for each (String^ relPath in candidateFiles)
	{
		if (String::IsNullOrWhiteSpace(relPath))
			continue;

		String^ displayPath = relPath; //path displayed in the UI
		String^ normalizedPath = relPath->Trim()->ToLowerInvariant();
		normalizedPath = normalizedPath->Replace("/", "\\");

		// -----------------------------
		// 1. Exact file conflict
		// -----------------------------
		List<UInt64>^ owners;
		if (fileOwners->TryGetValue(normalizedPath, owners))
		{
			for each (UInt64 ownerId in owners)
			{
				if (ownerId == itemId)
					continue;

				if (!fileConflicts->ContainsKey(displayPath))
					fileConflicts[displayPath] = gcnew HashSet<UInt64>();

				fileConflicts[displayPath]->Add(ownerId);
			}
		}

		// -----------------------------
		// 2. JVM counterpart conflict
		// -----------------------------
		String^ counterpartLookup = GetJvmCounterpart(normalizedPath);

		if (counterpartLookup != nullptr)
		{
			String^ counterpartNormalized = counterpartLookup->Trim()->ToLowerInvariant();
			counterpartNormalized = counterpartNormalized->Replace("/", "\\");

			if (fileOwners->TryGetValue(counterpartNormalized, owners))
			{
				String^ counterpartDisplay = GetJvmCounterpart(displayPath);
				if (String::IsNullOrWhiteSpace(counterpartDisplay))
					counterpartDisplay = counterpartLookup; //prevents a null dictionary key crash

				for each (UInt64 ownerId in owners)
				{
					if (ownerId == itemId)
						continue;

					if (!fileConflicts->ContainsKey(counterpartDisplay))
						fileConflicts[counterpartDisplay] = gcnew HashSet<UInt64>();

					fileConflicts[counterpartDisplay]->Add(ownerId);
				}
			}
		}
	}

	if (fileConflicts->Count == 0)
		return true;

	//build message
	StringBuilder^ msg = gcnew StringBuilder();
	msg->AppendLine(LOC_INSTALL_CONFLICT);
	msg->AppendLine();

	for each (auto kv in fileConflicts)
	{
		String^ filePath = kv.Key;
		HashSet<UInt64>^ owners = kv.Value;

		msg->AppendLine(filePath);

		for each (UInt64 otherId in owners)
		{
			CWorkshopItem* otherItem = FindWorkshopItem(otherId);
			String^ title = otherItem ? gcnew String(otherItem->GetTitle()) : "";

			msg->AppendLine("  " + otherId + " " + title);
		}

		msg->AppendLine();
	}

	DialogForm^ dialog = gcnew DialogForm();
	try
	{
		dialog->SetButtonLayout(DialogButtons::OKCancel);
		dialog->SetHeading(LOC_CONFLICT_FILES_INSTALL);
		dialog->SetDescription(msg->ToString());
		dialog->SetFooter(LOC_CONFLICT_CONTINUE_RW);

		return dialog->ShowDialog() == DialogResult::OK;
	}
	finally
	{
		delete dialog;
	}
}

bool CMainModule::IsItemOverwrittenByAnotherItem(uint64 itemId)
{
	String^ strItemId = Convert::ToString((long long)itemId, 10);
	String^ strFileC = Path::Combine(UNINSTALL_MASK, "c", strItemId);

	return File::Exists(strFileC) && (gcnew FileInfo(strFileC))->Length > 0;
}

bool CMainModule::IsItemMarkedOverwritten(uint64 itemId, bool bShowReport)
{
	if (!g_EnableOverwrittenColumn)
		return false;

	CWorkshopItem* item = FindWorkshopItem(itemId);
	if (!item || !item->m_bIsInstalled)
		return false;

	String^ depFile = Path::Combine(GetUninstallRoot(), "c\\" + itemId.ToString());
	if (!File::Exists(depFile))
		return false;

	FileInfo^ file = gcnew FileInfo(depFile);
	if (file->Length == 0)
		return false;

	if (!bShowReport)
		return true;

	//build report if requested
	List<String^>^ blockers = gcnew List<String^>();

	for each (String^ line in File::ReadLines(depFile))
	{
		if (!String::IsNullOrWhiteSpace(line))
			blockers->Add(line);
	}

	if (blockers->Count == 0)
	{
		InfoMessage(LOC_OVERWRITE_NO_RESULTS, LOC_INFO_MSG_CAPTION);
		return false;
	}

	StringBuilder^ reportMsg = gcnew StringBuilder();
	reportMsg->AppendLine(LOC_OVERWRITE_MSG_BODY);
	reportMsg->AppendLine();

	for each (String ^ blockerIdStr in blockers)
	{
		uint64 blockerId = ItemIdFromString(blockerIdStr);
		CWorkshopItem* blockerItem = FindWorkshopItem(blockerId);

		String^ title =	blockerItem ? gcnew String(blockerItem->GetTitle()) : "";

		reportMsg->AppendLine(blockerIdStr + " " + title);
	}    DialogForm^ dialog = gcnew DialogForm();
	try
	{
		dialog->SetButtonLayout(DialogButtons::OK);
		dialog->SetHeading(LOC_OVERWRITE_MSG_CAPTION);
		dialog->SetDescription(reportMsg->ToString());
		dialog->SetFooter(LOC_CONFLICT_PRESS_OK);

		dialog->ShowDialog();
	}
	finally
	{
		delete dialog;
	}

	return true;
}

bool CMainModule::IsItemOverwrittenByOwnership(uint64 itemId, HashSet<String^>^ installedFiles, Dictionary<String^, UInt64>^ fileOwners)
{
	for each (String^ relPath in installedFiles)
	{
		if (String::IsNullOrWhiteSpace(relPath))
			continue;

		if (IsBakFile(relPath) || IsInsideBakDir(relPath))
			continue;

		UInt64 ownerId = 0;
		if (!fileOwners->TryGetValue(relPath, ownerId))
			continue; //vanilla

		if (ownerId != itemId)
			return true;
	}

	return false;
}

CWorkshopItem* CMainModule::FindWorkshopItem(uint64 itemId)
{
	for (auto& item : m_vecWorkshopItems)
		if (item->GetItemId() == itemId)
			return item.get();
	
	return NULL;
}

void CMainModule::RemoveWorkshopItemByPtr(CWorkshopItem* item)
{
	DebugLog("RemoveWorkshopItemByPtr()");

	if (!item)
	{
		DebugLog("   item is NULL! nothing to do");
		return;
	}

	for (auto it = m_vecWorkshopItems.begin(); it != m_vecWorkshopItems.end(); ++it)
	{
		if (it->get() != item)
			continue;

		uint64 itemId = item->GetItemId();

		if (item->GetLoadState() == CWorkshopItem::WorkshopItemLoadState::Requesting)
		{
			DebugLog(String::Format("   failed to delete item {0}! this item is in use", item->GetItemId()));
			return;
		}

		DebugLog(String::Format("   deleting item {0} from m_vecWorkshopItems...", item->GetItemId()));
		m_vecWorkshopItems.erase(it);

		return;
	}
}

void CMainModule::QueryWorkshopState()
{
	DebugLog("QueryWorkshopState() BEGIN");

	m_vecWorkshopItems.clear();
	m_nWorkshopNumSubscribedItems = SteamUGC()->GetSubscribedItems(m_vecSubscribedItems, MAX_WORKSHOP_ITEMS);

	//add subscribed items and fill with placeholder metadata
	for (int i = 0; i < m_nWorkshopNumSubscribedItems; i++)
	{
		auto item = make_unique<CWorkshopItem>();
		item->SetItemId(m_vecSubscribedItems[i]);

		String^ uninstFilePath = GetUninstallFilePath(item.get());
		auto installedFiles = GetInstalledFiles(item->GetItemId());
		if (uninstFilePath || installedFiles->Count) //uninstall file may be empty for some items, currently we still consider them installed
			item->m_bIsInstalled = true;
		else
			item->m_bIsInstalled = false;

		uint64 itemId = item->GetItemId();
		uint32 state = SteamUGC()->GetItemState(itemId);

		//subscription
		item->m_bIsSubscribed = (state & k_EItemStateSubscribed) != 0;

		//download readiness
		item->m_bIsDownloaded = (state & k_EItemStateInstalled) != 0;

		//if downloading or needs update, it is NOT downloaded
		if (state &
			(k_EItemStateNeedsUpdate | k_EItemStateDownloading | k_EItemStateDownloadPending))
		{
			item->m_bIsDownloaded = false;
		}

		//metadata
		if (item->m_bIsSubscribed || item->m_bIsInstalled)
		{
			item->m_LoadState = CWorkshopItem::WorkshopItemLoadState::Requesting;

			SteamAPICall_t call = SteamUGC()->RequestUGCDetails(itemId, ITEM_CACHE_LIMIT);

			item->m_SteamCallResult.Set(call, item.get(), &CWorkshopItem::OnUGCDetails);
		}
		else
		{
			item->m_LoadState = CWorkshopItem::WorkshopItemLoadState::Completed;
		}

		DebugLog("   itemId: " + itemId);
		DebugLog("   state:" + FormatItemState(state));
		DebugLog("");

		m_vecWorkshopItems.push_back(move(item));
	}

	//find installed and unsubscribed items, add them to the vector
	for each (String^ strItemId in GetInstalledWorkshopItems())
	{
		uint64 itemId = ItemIdFromString(strItemId);

		if (itemId)
		{
			if (!FindWorkshopItem(itemId)) //user is NOT subscribed to this item
			{
				auto item = make_unique<CWorkshopItem>();

				item->m_LoadState = CWorkshopItem::WorkshopItemLoadState::Completed;
				item->SetItemId(itemId);
				item->SetTitle(LOC_INSTALLED_ITEM);
				item->SetDescription(LOC_DESC_UNSUBSCRIBED_ITEM);
				item->m_bIsInstalled = true;
				item->m_bIsSubscribed = false;

				item->m_LoadState = CWorkshopItem::WorkshopItemLoadState::Requesting;

				SteamAPICall_t steamApiCall = SteamUGC()->RequestUGCDetails(itemId, ITEM_CACHE_LIMIT);
				item->m_SteamCallResult.Set(steamApiCall, item.get(), &CWorkshopItem::OnUGCDetails);

				m_vecWorkshopItems.push_back(move(item));
			}
		}
	}

	DebugLog("QueryWorkshopState() END");
}

bool CMainModule::AllWorkshopItemsResolved()
{
	for (auto& item : m_vecWorkshopItems)
	{
		if (item->m_LoadState == CWorkshopItem::WorkshopItemLoadState::Undefined
			|| item->m_LoadState == CWorkshopItem::WorkshopItemLoadState::Requesting)
		{
			return false;
		}
	}

	return true;
}

List<String^>^ CMainModule::GetInstalledWorkshopItems()
{
	DebugLog("GetInstalledWorkshopItems() BEGIN");

	List<String^>^ result = gcnew List<String^>();

	String^ uninstallPath = GetUninstallRoot();
	DirectoryInfo^ dirInfo = gcnew DirectoryInfo(uninstallPath);

	if (dirInfo->Exists)
	{
		Regex^ numericOnly = gcnew Regex("^[0-9]+$", RegexOptions::Compiled);
		for each (FileInfo^ file in dirInfo->GetFiles())
		{
			String^ fileName = file->Name;
			if (!numericOnly->IsMatch(fileName)) //not including names that are not itemId numbers
				continue;

			result->Add(fileName);
			DebugLog(fileName);
		}
	}

	DebugLog("GetInstalledWorkshopItems() END");
	DebugLog("");

	return result;
}

List<String^>^ CMainModule::GetSubscribedItems()
{
	DebugLog("GetSubscribedItems() BEGIN");

	List<String^>^ result = gcnew List<String^>();

	vector<PublishedFileId_t> subscribedItems;
	uint32 itemsCount = SteamUGC()->GetNumSubscribedItems();

	if (itemsCount == 0)
	{
		DebugLog("SteamUGC()->GetNumSubscribedItems() returned 0!");
		return result;
	}

	subscribedItems.resize(itemsCount);
	SteamUGC()->GetSubscribedItems(subscribedItems.data(), itemsCount);

	for (auto itemId : subscribedItems)
	{
		result->Add(gcnew String(itemId.ToString()));
	}

	for each (String ^ subscribedItem in result)
	{
		DebugLog(subscribedItem);
	}

	DebugLog("GetSubscribedItems() END");

	return result;
}

List<String^>^ CMainModule::GetCachedItems()
{
	DebugLog("GetCachedItems() BEGIN");

	List<String^>^ result = gcnew List<String^>();

	DirectoryInfo^ dirInfo = gcnew DirectoryInfo(GetWorkshopPath());
	if (!dirInfo->Exists)
		return result;

	//only numeric folder names(Steam Workshop IDs)
	Regex^ numericRegex = gcnew Regex("^\\d+$", RegexOptions::Compiled);

	for each (DirectoryInfo ^ itemDir in dirInfo->GetDirectories())
	{
		try
		{
			String^ folderName = itemDir->Name;

			//skip if name is not a pure number
			if (!numericRegex->IsMatch(folderName))
				continue;

			//check if the directory (including subdirectories) contains any files
			auto files = itemDir->EnumerateFiles("*", SearchOption::AllDirectories);
			bool hasFiles = false;
			
			for each (FileInfo^ f in files)
			{
				hasFiles = true;
				break;
			}

			if (hasFiles)
			{
				result->Add(folderName);
				DebugLog(folderName);
			}
		}
		catch (UnauthorizedAccessException^ ex)
		{
#ifdef _DEBUG
			WarningMessage(ex->Message);
#endif
		}
		catch (IOException^ ex)
		{
#ifdef _DEBUG
			WarningMessage(ex->Message);
#endif
		}
	}

	DebugLog("GetCachedItems() END");

	return result;
}

List<UInt64>^ CMainModule::GetKnownItemIds()
{
	List<UInt64>^ result = gcnew List<UInt64>();

	for (auto& item : m_vecWorkshopItems)
	{
		result->Add(item->GetItemId());
	}

	return result;
}

List<UInt64>^ CMainModule::GetInstalledItemIds()
{
	List<UInt64>^ result = gcnew List<UInt64>();

	for each (String^ strItemId in GetInstalledWorkshopItems())
	{
		uint64 itemId = ItemIdFromString(strItemId);
		
		if(itemId)
			result->Add(itemId);
	}

	return result;
}

int CMainModule::GetNumSubscribedItems()
{
	return m_nWorkshopNumSubscribedItems;
}

int CMainModule::GetNumWorkshopItems()
{
	return m_vecWorkshopItems.size();
}

bool CMainModule::ManageWorkshopItem(uint64 itemId)
{
	DebugLog(String::Format("ManageWorkshopItem({0}) BEGIN", itemId));

	bool bSuccess = false;

	CWorkshopItem* item = nullptr;
	WorkshopItemManageMode manageMode;
	String^ itemTitle;

	if (!DetermineWorkshopItemAction(itemId, item, manageMode, itemTitle))
		return false;

	if (manageMode == MM_INSTALL)
		bSuccess = InstallWorkshopItem(itemId, item, itemTitle);
	else if (manageMode == MM_REMOVE)
		bSuccess = RemoveWorkshopItem(itemId, item, itemTitle);

	DebugLog(String::Format("ManageWorkshopItem({0}) END", itemId));

	if(bSuccess)
		UIState::NeedsFullUpdate = true;

	return bSuccess;
}

bool CMainModule::DetermineWorkshopItemAction(uint64 itemId, CWorkshopItem*& outItem, WorkshopItemManageMode& outMode, String^% outItemTitle)
{
	DebugLog(String::Format("DetermineWorkshopItemAction({0}) BEGIN", itemId));

	outItem = nullptr;
	outItemTitle = nullptr;
	outMode = MM_INSTALL;

	//workshop list must exist
	if (!GetNumWorkshopItems())
	{
		DebugLog("   no workshop items available, doing nothing");
		return false;
	}

	//lookup item
	CWorkshopItem* item = FindWorkshopItem(itemId);
	if (!item)
	{
		DebugLog("   workshop item not found!");
		return false;
	}

	outItem = item;

	//item title may become stale later
	outItemTitle = gcnew String(item->GetTitle());

	//decide action
	if (!item->m_bIsInstalled && !item->m_bIsBroken)
	{
		outMode = MM_INSTALL;
		DebugLog("Action determined: INSTALL");
	}
	else
	{
		outMode = MM_REMOVE;
		DebugLog("Action determined: REMOVE");
	}

	DebugLog(String::Format("DetermineWorkshopItemAction({0}) END", itemId));

	return true;
}

//todo: atomic replace instead of File::Delete() + File::Copy()
bool CMainModule::InstallWorkshopItem(uint64 itemId, CWorkshopItem* item, String^ itemTitle)
{
	DebugLog(String::Format("InstallWorkshopItem({0}) BEGIN", itemId));

	String^ srcPath = GetItemCachePath(item);
	String^ destPath = GetGamePath();

	String^ uninstPath = GetUninstallRoot();
	String^ uninstPathC = Path::Combine(uninstPath, "c");
	String^ strItemId = itemId.ToString();
	String^ uninstFile = Path::Combine(uninstPath, strItemId);

	InstallForm^ installForm = InstallForm::GetInstance();

	// ------------------------------------------------------------
	// Step 1: validate source path
	// ------------------------------------------------------------

	if (!Directory::Exists(srcPath))
	{
		WarningMessage(String::Format(LOC_INSTALL_ERR_SRC_PATH, srcPath));
		return false;
	}

	bool bIsEmpty = !Directory::EnumerateFileSystemEntries(srcPath)->GetEnumerator()->MoveNext();

	if (bIsEmpty)
	{
		WarningMessage(String::Format(gcnew String(LOC_INSTALL_SRC_PATH_EMPTY), srcPath));
		return false;
	}

	// ------------------------------------------------------------
	// Step 2: collect files
	// ------------------------------------------------------------

	List<String^>^ filesToInstallList	= GetFilesToInstall(itemId, FC_NO_JVM_FILES);
	List<String^>^ jvmFilesList			= GetFilesToInstall(itemId, FC_JVM_FILES_ONLY);
	List<String^>^ allFilesList			= GetFilesToInstall(itemId, FC_ALL_FILES);

	//conflict-relevant subset
	List<String^>^ conflictRelevantFiles = gcnew List<String^>();
	for each (String ^ relPath in allFilesList) //checking against all files, including .java/.class
	{
		if (IsInstallPayloadFile(relPath))
			conflictRelevantFiles->Add(relPath);
	}

	// ------------------------------------------------------------
	// Step 3: conflict detection
	// ------------------------------------------------------------

	HashSet<String^>^ conflictedItemIds =
		gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

	//this makes uninstall log idempotent, prevents duplicate entries
	HashSet<String^>^ writtenFiles =
		gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

	if (!CheckFileConflicts(itemId, conflictRelevantFiles))
		return false;

	auto fileOwners = BuildFileOwnerMap();

	for each (String^ relPath in conflictRelevantFiles)
	{
		List<UInt64>^ owners;

		if (fileOwners->TryGetValue(relPath, owners))
		{
			for each (UInt64 ownerId in owners)
			{
				if (ownerId != itemId)
					conflictedItemIds->Add(ownerId.ToString());
			}
		}
	}

	// ------------------------------------------------------------
	// Step 4: execute install transaction
	// ------------------------------------------------------------

	if (installForm)
		installForm->LoadForm();
	else
		DebugLog("ERROR: failed to load InstallForm!");

	FileStream^ fStream = nullptr;
	StreamWriter^ uInfo = nullptr;

	CreateSentinelFile(itemId);

	/* test scenarios for installing JVM files:
	* 
	* I. INSTALL JAVA FILE
	* 
	* 1. java[+] & class[+] -> backup old java, backup old class, install new java, fictive 0KB class [OK]
	* 2. java[+] & class[-] -> backup old java, install new java, fictive 0KB class [OK]
	* 3. java[-] & class[+] -> install new java, backup old class, fictive 0KB class [OK]
	* 4. java[-] & class[-] -> install new java, fictive 0KB class [OK]
	* 
	* II. INSTALL CLASS FILE
	* 
	* 1. java[+] & class[+] -> backup old class, backup old java, install new class [OK]
	* 2. java[+] & class[-] -> backup old java, install new class [OK]
	* 3. java[-] & class[+] -> backup old class, install new class [OK]
	* 4. java[-] & class[-] -> install new class [OK]
	*/

	try
	{
		DebugLog("-------- INSTALLING FILES --------");

		//create directory uninstall logs, if it doesn't exist
		if (!Directory::Exists(uninstPath))
		{
			DebugLog("CreateDirectory: " + uninstPath);
			Directory::CreateDirectory(uninstPath);
		}

		//making sure uninstall log file is not read only, if exists
		if (File::Exists(uninstFile))
			File::SetAttributes(uninstFile, FileAttributes::Normal);

		fStream = File::Create(uninstFile);
		uInfo = gcnew StreamWriter(fStream);

		if (!uInfo)
			throw gcnew InvalidOperationException(LOC_INSTALL_LOG_INIT_EX);

		UninstallLogWriter^ logWriter = gcnew UninstallLogWriter(uInfo);

		//build sets of JVM files
		HashSet<String^>^ installedClassFiles = gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);
		HashSet<String^>^ installedJavaFiles = gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

		//mark each JVM file that we're going to install
		for each (String^ relPath in jvmFilesList)
		{
			if (relPath->EndsWith(".java", StringComparison::OrdinalIgnoreCase))
				installedJavaFiles->Add(relPath);

			if (relPath->EndsWith(".class", StringComparison::OrdinalIgnoreCase))
				installedClassFiles->Add(relPath);
		}

		//install .class files
		for each (String^ relPath in jvmFilesList)
		{
			if (!relPath->EndsWith(".class", StringComparison::OrdinalIgnoreCase))
				continue;

			String^ srcJvm = Path::Combine(srcPath, relPath);
			String^ dstJvm = Path::Combine(destPath, relPath);

			String^ dstDir = Path::GetDirectoryName(dstJvm);
			if (!Directory::Exists(dstDir))
			{
				DebugLog("CreateDirectory: " + dstDir);
				Directory::CreateDirectory(dstDir);
			}

			//neutralize java source if it exists
			String^ classDir = Path::GetDirectoryName(relPath); //example: "sl\Scripts\game"
			String^ className = Path::GetFileNameWithoutExtension(relPath); //example: "Bot"

			String^ javaRelPath = Path::Combine(classDir, "src", className + ".java");
			String^ javaFullPath = Path::Combine(destPath, javaRelPath);

			DebugLog("trying to find the sourcefile: " + javaFullPath);

			if (File::Exists(javaFullPath))
			{
				DebugLog("neutralizing java source:");
				DebugLog("   " + javaRelPath);

				String^ javaBackupRelPath = javaRelPath + ".bak" + strItemId;

				BackupFile(destPath, javaRelPath, javaBackupRelPath, logWriter);

				File::SetAttributes(javaFullPath, FileAttributes::Normal);
				File::Delete(javaFullPath);

				//log the operation so uninstall restores it later
				logWriter->Write(javaRelPath);
			}

			//backup original .class file
			String^ relPathBackup = relPath + ".bak" + strItemId;
			BackupFile(destPath, relPath, relPathBackup, logWriter);

			DebugLog("File::Copy");
			DebugLog("   " + srcJvm);
			DebugLog("   " + dstJvm);

			File::Copy(srcJvm, dstJvm, true);
			
			if (writtenFiles->Add(relPath))
				logWriter->Write(relPath);

			if (g_UseFictiveJavaFiles)
			{
				//locate sibling .java file
				String^ classDirRel = Path::GetDirectoryName(relPath); //example: "sl\Scripts\game"
				String^ className = Path::GetFileNameWithoutExtension(relPath); //example: "Bot"

				String^ siblingJavaRelPath = Path::Combine(classDirRel, "src", className + ".java");
				String^ siblingJavaFullPath = Path::Combine(destPath, siblingJavaRelPath);

				bool bJavaInstalledByItem = installedJavaFiles->Contains(siblingJavaRelPath);

				//log a fictive .java if none exists
				if (!bJavaInstalledByItem)
				{
					DebugLog("logging fictive .java file:");
					DebugLog("   " + siblingJavaRelPath);

					//log so uninstall removes it
					if (writtenFiles->Add(siblingJavaRelPath))
						logWriter->Write(siblingJavaRelPath);
				}
			}
		}

		//install .java files
		for each (String ^ relPath in jvmFilesList)
		{
			if (!relPath->EndsWith(".java", StringComparison::OrdinalIgnoreCase))
				continue;

			String^ srcJvm = Path::Combine(srcPath, relPath);
			String^ dstJvm = Path::Combine(destPath, relPath);

			DebugLog("CreateDirectory: " + Path::GetDirectoryName(dstJvm));
			Directory::CreateDirectory(Path::GetDirectoryName(dstJvm));

			//backup existing .java file
			String^ relPathBackup = relPath + ".bak" + strItemId;
			BackupFile(destPath, relPath, relPathBackup, logWriter);

			String^ dstDir = Path::GetDirectoryName(dstJvm);
			if (!Directory::Exists(dstDir))
			{
				DebugLog("CreateDirectory: " + dstDir);
				Directory::CreateDirectory(dstDir);
			}

			DebugLog("File::Copy");
			DebugLog("   " + srcJvm);
			DebugLog("   " + dstJvm);

			File::Copy(srcJvm, dstJvm, true);

			if (writtenFiles->Add(relPath))
				logWriter->Write(relPath);

			//locate sibling .class file
			String^ javaDirRel = Path::GetDirectoryName(relPath); //example: "sl\Scripts\game\src"
			String^ parentDirRel = Path::GetDirectoryName(javaDirRel); //example: "sl\Scripts\game"

			String^ siblingClassName = Path::GetFileNameWithoutExtension(relPath) + ".class";
			String^ siblingClassRelPath = Path::Combine(parentDirRel, siblingClassName);
			String^ siblingClassFullPath = Path::Combine(destPath, siblingClassRelPath);

			if (g_UseFictiveClassFiles)
			{
				bool bDumpSiblingPaths = false;
				if (bDumpSiblingPaths)
				{
					DebugLog("g_UseFictiveClassFiles");

					DebugLog("javaDirRel:");
					DebugLog("   " + javaDirRel);

					DebugLog("parentDirRel:");
					DebugLog("   " + parentDirRel);

					DebugLog("siblingClassName:");
					DebugLog("   " + siblingClassName);

					DebugLog("siblingClassRelPath:");
					DebugLog("   " + siblingClassRelPath);

					DebugLog("siblingClassFullPath:");
					DebugLog("   " + siblingClassFullPath);
				}
			}

			//delete stale .class file
			int srcIdx = dstJvm->LastIndexOf("\\src\\", StringComparison::OrdinalIgnoreCase);
			if (srcIdx >= 0)
			{
				String^ baseDir = dstJvm->Substring(0, srcIdx);
				String^ className = Path::GetFileNameWithoutExtension(dstJvm) + ".class";
				String^ classPath = Path::Combine(baseDir, className);

				if (File::Exists(classPath))
				{
					String^ classRelPath = GetNormalizedRelativePath(destPath, classPath);
					
					//only back up if this class was NOT already handled in the install .class phase
					if (!installedClassFiles->Contains(classRelPath))
					{
						String^ classBackupRelPath = classRelPath + ".bak" + strItemId;
						BackupFile(destPath, classRelPath, classBackupRelPath, logWriter);

						//only delete if this item did NOT install that class
						DebugLog("DELETE STALE CLASS FILE:");
						DebugLog("   " + classPath);

						File::Delete(classPath);
					}
				}
				else
				{
					DebugLog("WARNING: trying to delete stale class file twice!");
					DebugLog("   " + classPath);
				}
			}

			if (g_UseFictiveClassFiles)
			{
				bool bClassInstalledByItem = installedClassFiles->Contains(siblingClassRelPath);

				//create fictive .class if none exists
				if (!bClassInstalledByItem && !File::Exists(siblingClassFullPath))
				{
					DebugLog("creating fictive .class file:");
					DebugLog("   " + siblingClassFullPath);

					String^ siblingClassDirFull = Path::GetDirectoryName(siblingClassFullPath);
					if (!Directory::Exists(siblingClassDirFull))
					{
						DebugLog("CreateDirectory: " + siblingClassDirFull);
						Directory::CreateDirectory(siblingClassDirFull);
					}

					FileStream^ fs = File::Create(siblingClassFullPath);
					fs->Close();

					//log so uninstall removes it
					if (writtenFiles->Add(siblingClassRelPath))
						logWriter->Write(siblingClassRelPath);
				}
			}
		}

		//installing normal files
		for each (String ^ relPath in filesToInstallList)
		{
			//already installed these
			if (IsJvmFile(relPath))
				continue;

			String^ sourcePath = Path::Combine(srcPath, relPath);
			String^ targetPath = Path::Combine(destPath, relPath);

			DebugLog("relPath: " + relPath);
			DebugLog("sourcePath: " + sourcePath);
			DebugLog("destPath: " + destPath);
			DebugLog("targetPath " + targetPath);

			String^ targetDir = Path::GetDirectoryName(targetPath);
			if (!Directory::Exists(targetDir))
			{
				DebugLog("CreateDirectory: " + Path::GetDirectoryName(targetPath));
				Directory::CreateDirectory(targetDir);
			}

			//backup existing file
			if (File::Exists(targetPath))
			{
				DebugLog("making backup of " + targetPath);

				String^ relPathBackup = relPath;

				if (IsUnderDir(relPath, "music"))
				{
					relPathBackup = Path::Combine("music\\_music.bak" + strItemId, relPath->Substring(6));
				}
				else if (IsUnderDir(relPath, "save"))
				{
					relPathBackup = Path::Combine("save\\_save.bak" + strItemId, relPath->Substring(5));
				}
				else
				{
					relPathBackup = relPath + ".bak" + strItemId;
				}

				BackupFile(destPath, relPath, relPathBackup, logWriter);

				DebugLog("File::Delete");
				DebugLog("   " + targetPath);
				File::Delete(targetPath);
			}

			//write install log
			DebugLog("uInfo->WriteLine: " + relPath);
			if (writtenFiles->Add(relPath))
				logWriter->Write(relPath);

			File::Copy(sourcePath, targetPath, true);
			DebugLog("File::Copy");
			DebugLog("   " + sourcePath);
			DebugLog("   " + targetPath);
		}

		uInfo->Flush();
		uInfo->Close();
		fStream->Close();
	}
	catch (Exception^ ex)
	{
		DebugLog("INSTALLATION FAILED:");
		DebugLog(ex->ToString());

		if (uInfo)
			uInfo->Close();

		if (fStream)
			fStream->Close();

		MarkInstallationBroken(itemId, true);

		if (installForm)
			installForm->UnloadForm();

		WarningMessage(ex->Message);

		return false;
	}

	FileInfo^ file = gcnew FileInfo(uninstFile);
	if (file->Exists)
		File::SetAttributes(file->FullName, FileAttributes::ReadOnly);

	// ------------------------------------------------------------
	// Step 5: dependency graph (required by the old installer)
	// ------------------------------------------------------------

	if (conflictedItemIds->Count > 0)
	{
		DebugLog("building transitive dependency graph");

		Directory::CreateDirectory(uninstPathC);

		//breadth-first propagation
		Generic::Queue<String^>^ toProcess =
			gcnew Generic::Queue<String^>();

		HashSet<String^>^ visited =
			gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

		//seed queue with directly overwritten items
		for each (String^ strId in conflictedItemIds)
		{
			if (!String::IsNullOrWhiteSpace(strId) &&
				!String::Equals(strId, strItemId, StringComparison::OrdinalIgnoreCase))
			{
				toProcess->Enqueue(strId);
			}
		}

		while (toProcess->Count > 0)
		{
			String^ targetId = toProcess->Dequeue();

			if (visited->Contains(targetId))
				continue;

			visited->Add(targetId);

			if (String::Equals(targetId, strItemId, StringComparison::OrdinalIgnoreCase))
				continue;

			String^ depFilePath = Path::Combine(uninstPathC, targetId);

			HashSet<String^>^ overwrittenBy =
				gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

			//load existing dependency list
			if (File::Exists(depFilePath))
			{
				for each (String^ line in File::ReadLines(depFilePath))
				{
					if (String::IsNullOrWhiteSpace(line))
						continue;

					overwrittenBy->Add(line);

					//propagate upward (transitive)
					if (!visited->Contains(line) &&
						!String::Equals(line, strItemId, StringComparison::OrdinalIgnoreCase))
					{
						toProcess->Enqueue(line);
					}
				}
			}

			//record that current item overwrites this target
			overwrittenBy->Add(strItemId);

			DebugLog("updating dependency file: " + depFilePath);

			File::WriteAllLines(depFilePath, overwrittenBy);
		}
	}

	DeleteSentinelFile(itemId);
	item->m_bIsInstalled = true;

	DebugLog("-------- INSTALLING COMPLETE --------");

	if (installForm)
		installForm->UnloadForm();

	InfoMessage(String::Format(LOC_OPERATION_INSTALL, itemTitle), LOC_INFO_MSG_CAPTION);

	DebugLog(String::Format("InstallWorkshopItem({0}) END", itemId));

	return true;
}

//todo: add SteamUserID to uninstall logs as meta information, currently the logs are shared across all Steam users
bool CMainModule::RemoveWorkshopItem(uint64 itemId, CWorkshopItem* item, String^ itemTitle)
{
	DebugLog(String::Format("RemoveWorkshopItem({0}) BEGIN", itemId));

	if (item->m_bIsBroken)
		DebugLog("   this item is broken! performing best-effort cleanup");

	String^ strItemId = itemId.ToString();

	String^ uninstPath = GetUninstallRoot();
	String^ uninstPathC = Path::Combine(uninstPath, "c");
	String^ uninstFile = Path::Combine(uninstPath, strItemId);
	String^ uninstFileC = Path::Combine(uninstPathC, strItemId);

	InstallForm^ installForm = InstallForm::GetInstance();

	// ------------------------------------------------------------
	// Step 1: block uninstall if other items depend on this one
	// ------------------------------------------------------------

	if (File::Exists(uninstFileC))
	{
		List<String^>^ blockers = gcnew List<String^>();

		for each (String ^ line in File::ReadLines(uninstFileC)) //read all blockers from the uninstall file
		{
			if (!String::IsNullOrWhiteSpace(line))
				blockers->Add(line);
		}

		//if any blockers found, build a message for user
		if (blockers->Count > 0)
		{
			DebugLog("   uninstall blocked by dependencies, blockers found: " + blockers->Count);

			StringBuilder^ msg = gcnew StringBuilder();
			msg->AppendLine(LOC_REMOVE_OVERWRITTEN_BY);
			msg->AppendLine();

			for each (String ^ blockerIdStr in blockers)
			{
				uint64 blockerId = ItemIdFromString(blockerIdStr);
				CWorkshopItem* blockerItem = FindWorkshopItem(blockerId);
				String^ strBlockerItemTitle = blockerItem ? gcnew String(blockerItem->GetTitle()) : "";

				msg->AppendLine(blockerIdStr + " " + strBlockerItemTitle);

				DebugLog(String::Format("   blockerId: {0} ({1})", blockerId, strBlockerItemTitle));
			}

			msg->AppendLine();
			msg->AppendLine(LOC_REMOVE_USER_HINT);

			//show message to user
			DialogForm^ dialogForm = gcnew DialogForm();
			try
			{
				dialogForm->SetButtonLayout(DialogButtons::OK); //installation order is linear and currently we can't allow user to proceed here
				dialogForm->SetHeading(LOC_CONFLICT_FILES_REMOVE);
				dialogForm->SetDescription(msg->ToString());
				dialogForm->SetFooter(LOC_CONFLICT_PRESS_OK);

				dialogForm->ShowDialog();
			}
			finally
			{
				delete dialogForm;
			}

			return false;
		}
	}

	// ------------------------------------------------------------
	// Step 2: ensure uninstall log exists
	// ------------------------------------------------------------

	if (!File::Exists(uninstFile))
	{
		WarningMessage(String::Format(LOC_REMOVE_LOG_NOT_FOUND, uninstFile));
		return false;
	}

	if (installForm)
		installForm->LoadForm();

	DebugLog("-------- UNINSTALLING FILES --------");

	// ------------------------------------------------------------
	// Phase 3: scan uninstall log file
	// ------------------------------------------------------------

	List<FileInfo^>^ simpleBackups = gcnew List<FileInfo^>();
	List<FileInfo^>^ centralBackups = gcnew List<FileInfo^>();
	HashSet<String^>^ touchedDirs = gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

	DebugLog("   scanning uninstall file " + uninstFile);
	for each (String^ relPath in File::ReadLines(uninstFile))
	{
		DebugLog("   " + relPath);

		if (String::IsNullOrWhiteSpace(relPath))
			continue;

		String^ fullPath = Path::Combine(GetGamePath(), relPath);
		
		FileInfo^ file = gcnew FileInfo(fullPath);
		if (!file->Exists)
			continue;

		//track directories for cleanup
		String^ relDir = file->Directory->FullName->Replace(GetGamePath() + "\\", "");

		if (!String::IsNullOrEmpty(relDir))
			touchedDirs->Add(relDir);

		//classify file
		if (file->Extension->Equals(".bak" + strItemId, StringComparison::OrdinalIgnoreCase))
		{
			DebugLog("   " + file);
			simpleBackups->Add(file);
		}
		else if (relPath->IndexOf("\\_music.bak" + strItemId + "\\", StringComparison::OrdinalIgnoreCase) >= 0 ||
			relPath->IndexOf("\\_save.bak" + strItemId + "\\", StringComparison::OrdinalIgnoreCase) >= 0)
		{
			DebugLog("   " + file);
			centralBackups->Add(file);
		}
		else
		{
			File::SetAttributes(file->FullName, FileAttributes::Normal);
			file->Delete();
		}
	}

	// ------------------------------------------------------------
	// Step 4: restore simple backups
	// ------------------------------------------------------------

	for each (FileInfo^ file in simpleBackups)
	{
		if (!file)
			continue;

		String^ backupPath = file->FullName;

		//backup may have been deleted or never created
		if (!File::Exists(backupPath))
		{
			DebugLog("WARNING: backup file missing, skipping restore");
			DebugLog("   " + backupPath);

			continue;
		}

		String^ restored = backupPath->Replace(".bak" + strItemId, "");

		try
		{
			//destination object might already exist
			if (File::Exists(restored))
			{
				File::SetAttributes(restored, FileAttributes::Normal);
				File::Delete(restored);
			}

			DebugLog("File::Move");
			DebugLog("   " + backupPath);
			DebugLog("   " + restored);

			File::Move(backupPath, restored);
		}
		catch (Exception^ ex)
		{
			DebugLog("WARNING: failed to restore backup");
			DebugLog("   from: " + backupPath);
			DebugLog("   to  : " + restored);
			DebugLog("   error: " + ex->Message);

			//best-effort uninstall: continue with remaining files
			continue;
		}
	}

	// ------------------------------------------------------------
	// Step 5: restore central backups (music/save)
	// ------------------------------------------------------------

	for each (FileInfo ^ file in centralBackups)
	{
		String^ path = file->FullName;
		DebugLog(path);

		String^ bakMusic = "\\_music.bak" + strItemId + "\\";
		String^ bakSave = "\\_save.bak" + strItemId + "\\";

		int idx = path->IndexOf(bakMusic, StringComparison::OrdinalIgnoreCase);
		if (idx >= 0)
		{
			String^ restored = path->Remove(idx, bakMusic->Length - 1);

			//destination object might be busy!
			if (File::Exists(restored))
			{
				File::SetAttributes(restored, FileAttributes::Normal);
				File::Delete(restored);
			}

			File::Move(path, restored);

			DebugLog("File::Move");
			DebugLog("   " + path);
			DebugLog("   " + restored);

			continue;
		}

		idx = path->IndexOf(bakSave, StringComparison::OrdinalIgnoreCase);
		if (idx >= 0)
		{
			String^ restored = path->Remove(idx, bakSave->Length - 1);

			//destination object might be busy!
			if (File::Exists(restored))
			{
				File::SetAttributes(restored, FileAttributes::Normal);
				File::Delete(restored);
			}

			File::Move(path, restored);

			DebugLog("File::Move");
			DebugLog("   " + path);
			DebugLog("   " + restored);
		}
	}

	// ------------------------------------------------------------
	// Step 6: remove uninstall log and sentinel file
	// ------------------------------------------------------------

	FileInfo^ file = gcnew FileInfo(uninstFile);
	file->Attributes = FileAttributes::Normal;
	file->Delete();

	DebugLog("deleted uninstall log:");
	DebugLog("   " + file->FullName);

	DeleteSentinelFile(itemId);

	// ------------------------------------------------------------
	// Step 7: clean dependency graph
	// ------------------------------------------------------------

	if (Directory::Exists(uninstPathC))
	{
		DebugLog("cleaning dependency graph");

		for each (String ^ depFile in Directory::GetFiles(uninstPathC))
		{
			List<String^>^ remainingList = gcnew List<String^>();
			bool bFound = false;

			for each (String ^ line in File::ReadLines(depFile))
			{
				if (String::IsNullOrWhiteSpace(line))
					continue;

				if (line == strItemId)
					bFound = true;
				else
					remainingList->Add(line);
			}

			if (!bFound)
				continue;

			if (remainingList->Count > 0)
				File::WriteAllLines(depFile, remainingList);
			else
			{
				DebugLog("File::Delete");
				DebugLog("   " + depFile);

				File::Delete(depFile);
			}
		}
	}

	// ------------------------------------------------------------
	// Step 8: remove empty directories
	// ------------------------------------------------------------

	DebugLog("removing empty directories");
	for each (String ^ relDir in touchedDirs)
	{
		DirectoryInfo^ dir =
			gcnew DirectoryInfo(Path::Combine(GetGamePath(), relDir));

		//BUG:
		//System.UnauthorizedAccessException: 'Access to the path 'D:\SteamLibrary\steamapps\common\Street Legal Racing Redline\ug)\additional_source\workshop_installation_tool\Debug\sl\Scripts\game' is denied.'
		//how to reproduce:
		//try to enter any folder that the installer will try to delete, then click "REMOVE" on the item and it will fail
		//windows explorer keeps this folder busy

		while (dir->Exists &&
			!dir->FullName->Equals(GetGamePath(), StringComparison::OrdinalIgnoreCase) &&
			Directory::GetFiles(dir->FullName, "*", SearchOption::AllDirectories)->Length == 0)
		{
			DirectoryInfo^ parentDir = dir->Parent;
			dir->Delete(true);
			dir = parentDir;

			DebugLog("deleted dir: ");
			DebugLog("   " + parentDir);
		}
	}

	// ------------------------------------------------------------
	// Step 9: finalize uninstall
	// ------------------------------------------------------------

	item->m_bIsInstalled = false;
	MarkInstallationBroken(itemId, false);

	//title may have changed
	itemTitle = gcnew String(item->GetTitle());

	//this item can't be installed again, we're removing it
	if (!item->m_bIsSubscribed)
		RemoveWorkshopItemByPtr(item);

	DebugLog("-------- UNINSTALL COMPLETE --------");

	if (installForm)
		installForm->UnloadForm();

	InfoMessage(String::Format(LOC_OPERATION_REMOVE, itemTitle), LOC_INFO_MSG_CAPTION); //not using strItemTitle because it might be stale
	
	DebugLog(String::Format("RemoveWorkshopItem({0}) END", itemId));
	
	return true;
}

void CMainModule::CreateSentinelFile(uint64 itemId)
{
	try
	{
		//sentinel lives next to uninstall logs
		String^ sentinelPath = GetSentinelFilePath(itemId);

		//ensure directory exists
		String^ dir = Path::GetDirectoryName(sentinelPath);
		if (!Directory::Exists(dir))
			Directory::CreateDirectory(dir);

		//overwrite if exists
		StringBuilder^ content = gcnew StringBuilder();
		content->AppendLine(SentinelFormat::itemKey + SentinelFormat::separator + itemId);
		content->AppendLine(SentinelFormat::timeKey + SentinelFormat::separator + DateTime::UtcNow.ToString("o"));

		File::WriteAllText(sentinelPath, content->ToString());

		DebugLog("created sentinel file for item " + itemId + ":");
		DebugLog("   " + sentinelPath);
	}
	catch (Exception^ ex)
	{
		//sentinel failure must NOT abort installation
		DebugLog("WARNING: failed to create sentinel file for item " + itemId);
		DebugLog("   " + ex->ToString());
	}
}

void CMainModule::DeleteSentinelFile(uint64 itemId)
{
	try
	{
		String^ sentinelPath = GetSentinelFilePath(itemId);

		DebugLog("deleting sentinel file for item " + itemId + ":");
		DebugLog("   " + sentinelPath);

		//edge case: sentinel file could be read-only
		if (File::Exists(sentinelPath))
		{
			File::SetAttributes(sentinelPath, FileAttributes::Normal);
			File::Delete(sentinelPath);
		}
	}
	catch (Exception^ ex)
	{
		DebugLog("WARNING: failed to delete sentinel file for item " + itemId);
		DebugLog("   " + ex->ToString());
	}
}

void CMainModule::CleanupOrphanSentinels()
{
	DebugLog("CleanupOrphanSentinels() BEGIN");

	String^ uninstallRoot = GetUninstallRoot();
	if (String::IsNullOrEmpty(uninstallRoot))
		return;

	DirectoryInfo^ dir = gcnew DirectoryInfo(uninstallRoot);
	if (!dir->Exists)
		return;

	for each (FileInfo^ file in dir->GetFiles("*" + SENTINEL_MASK))
	{
		bool bShouldDelete = false;

		uint64 fileItemId = ItemIdFromSentinelFileName(file->Name);
		uint64 contentItemId = 0;
		bool bHasContentItemId = false;

		//1. parse itemId from filename
		if (!fileItemId)
		{
			DebugLog("incorrect sentinel filename, deleting:");
			DebugLog("   " + file->FullName);
			
			bShouldDelete = true;
		}

		//2. parse itemId from content
		if (!bShouldDelete)
		{
			try
			{
				String^ sentinelPath = file->FullName;
				for each (String^ line in File::ReadLines(sentinelPath))
				{
					String^ key;
					String^ value;

					if (!SentinelFormat::TryParseLine(line, key, value))
						continue;

					if (key->Equals(SentinelFormat::itemKey, StringComparison::OrdinalIgnoreCase))
					{
						if (UInt64::TryParse(value, contentItemId))
							bHasContentItemId = true;

						break;
					}
				}
			}
			catch (Exception^ ex)
			{
				DebugLog("CleanupOrphanSentinels(): failed to read sentinel:");
				DebugLog("   " + file->FullName);
				DebugLog(ex->ToString());
				continue; //unreadable sentinel, leave it as is
			}
		}

		//missing ITEM field
		if (!bShouldDelete && !bHasContentItemId)
		{
			DebugLog("Sentinel without ITEM field, deleting:");
			DebugLog("   " + file->FullName);

			bShouldDelete = true;
		}

		//mismatch between filename and content
		if (!bShouldDelete && fileItemId != contentItemId)
		{
			DebugLog("Sentinel itemId mismatch, deleting:");
			DebugLog("   filename: " + fileItemId);
			DebugLog("   content : " + contentItemId);

			bShouldDelete = true;
		}

		//orphan item
		if (!bShouldDelete && !FindWorkshopItem(contentItemId))
		{
			DebugLog("Orphan sentinel (item no longer known), deleting:");
			DebugLog("   " + contentItemId);

			bShouldDelete = true;
		}

		//apply decision
		if (bShouldDelete)
		{
			try
			{
				System::IO::File::SetAttributes(file->FullName,	System::IO::FileAttributes::Normal); //remove read-only flags if any
				file->Delete();
			}
			catch (System::Exception^ ex)
			{
				DebugLog("WARNING: failed to delete sentinel:");
				DebugLog(ex->ToString());
			}
		}
	}

	DebugLog("CleanupOrphanSentinels() END");
}

bool CMainModule::IsInstallInterrupted(uint64 itemId)
{
	try
	{
		String^ sentinelPath = GetSentinelFilePath(itemId);

		//no sentinel -> no interrupted install
		if (!File::Exists(sentinelPath))
			return false;

		DebugLog("IsInstallInterrupted(" + itemId + ")");

		//sentinel exists -> make sure it belongs to this item
		for each (String^ line in File::ReadLines(sentinelPath))
		{
			String^ key;
			String^ value;

			if (!SentinelFormat::TryParseLine(line, key, value))
				continue;

			DebugLog("TryParseLine:");
			DebugLog("   " + line);
			DebugLog("   key: " + key);
			DebugLog("   value: " + value);
			DebugLog("");

			if (key->Equals(SentinelFormat::itemKey, StringComparison::OrdinalIgnoreCase))
			{
				UInt64 parsedItemId = 0;

				if (UInt64::TryParse(value, parsedItemId))
					return parsedItemId == itemId;

				break; //malformed ITEM line, treat this install as interrupted
			}
		}

		//sentinel exists but content is malformed, still treat this install as interrupted
		return true;
	}
	catch (Exception^ ex)
	{
		DebugLog("WARNING: Failed to read sentinel file");
		DebugLog(ex->ToString());

		//fail-safe: sentinel exists but unreadable, assume this install as interrupted
		return true;
	}
}

//if there a stale .class has been previously deleted, then this function will return false
bool CMainModule::VerifyInstallation(uint64 itemId)
{
	DebugLog(String::Format("VerifyInstallation({0}) BEGIN", itemId));

	CWorkshopItem* item = FindWorkshopItem(itemId);
	if (!item)
	{
		DebugLog("   ERROR: can't find item " + itemId);
		return false;
	}

	//reading uninstall log
	auto installedFilesList = GetInstalledFiles(itemId);
	if (installedFilesList->Count == 0)
	{
		DebugLog("  ERROR: uninstall log missing or empty");
		return false;
	}

	auto cachedFilesList = GetCachedFiles(itemId);
	if (cachedFilesList->Count == 0)
	{
		DebugLog("   workshop cache is empty, nothing to do");
		return true;
	}

	//build a set of cached files
	auto cacheFileSet = gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);
	for	each (String ^ relPath in cachedFilesList)
	{
		if (String::IsNullOrWhiteSpace(relPath))
			continue;

		if (IsBakFile(relPath) || IsInsideBakDir(relPath) || !IsInstallPayloadFile(relPath))
			continue;

		String^ cacheFullPath = Path::Combine(GetItemCachePath(item), relPath);
		if (Directory::Exists(cacheFullPath))
			continue;

		cacheFileSet->Add(relPath);
	}

	//build uninstall payload set
	auto uninstallFileSet = gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

	for each (String ^ relPath in installedFilesList)
	{
		if (String::IsNullOrWhiteSpace(relPath))
		{
			DebugLog("   skip empty file " + relPath);
			continue;
		}

		if (IsBakFile(relPath) || IsInsideBakDir(relPath))
		{
			DebugLog("   skip bak file " + relPath);
			continue;
		}

		uninstallFileSet->Add(relPath);

		String^ fullPath = Path::Combine(GetGamePath(), relPath);
		DebugLog("   " + fullPath);

		//file must exist
		if (!File::Exists(fullPath))
		{
			if (g_UseFictiveJavaFiles)
			{
				String^ counterpartRel = GetJvmCounterpart(relPath);
				if (counterpartRel != nullptr)
				{
					String^ counterpartFull = Path::Combine(GetGamePath(), counterpartRel);
					if (File::Exists(counterpartFull))
					{
						DebugLog("  satisfied fictive JVM counterpart: " + relPath);
						continue;
					}
				}
			}

			DebugLog("  missing installed file: " + relPath);
			return false;
		}

		//file must be readable
		try
		{
			auto fs = File::Open(fullPath, FileMode::Open, FileAccess::Read, FileShare::Read);
			fs->Close();
		}
		catch (Exception^)
		{
			DebugLog("  installed file not accessible: " + relPath);
			return false;
		}
	}

	//enforce equality between uninstall log and cache payload
	for each (String ^ relPath in uninstallFileSet)
	{
		if (!cacheFileSet->Contains(relPath))
		{
			//skip fictive java mismatch
			if (g_UseFictiveJavaFiles && relPath->EndsWith(".java", StringComparison::OrdinalIgnoreCase))
			{
				DebugLog("  skipping fictive .java during verification: " + relPath);
				continue;
			}

			DebugLog("  uninstall log references file missing from cache: " + relPath);
			return false;
		}
	}

	for each (String^ relPath in cacheFileSet)
	{
		if (!uninstallFileSet->Contains(relPath))
		{
			DebugLog("  cache file missing from uninstall log: " + relPath);
			return false;
		}
	}

	DebugLog(String::Format("VerifyInstallation({0}) OK", itemId));
	return true;
}

void CMainModule::OnWorkshopItemDownloaded(DownloadItemResult_t* pResult)
{
	if (!pResult)
		return;

	uint64 itemId = pResult->m_nPublishedFileId;
	CWorkshopItem* item = FindWorkshopItem(itemId);

	if (!item)
		return; //unknown or stale

	DebugLog("OnWorkshopItemDownloaded() BEGIN");
	DebugLog("   DownloadItemResult: " + itemId);

	//Steam-level failure
	if (pResult->m_eResult != k_EResultOK)
	{
		DebugLog("   download failed, result = " + Convert::ToString(pResult->m_eResult));

		item->m_bIsDownloaded = false;
		item->m_bDownloadInProgress = false;
		
		UIState::NeedsActionButtonUpdate = true;
		return;
	}

	uint32 state = SteamUGC()->GetItemState(itemId);
	DebugLog("   post-download state: " + FormatItemState(state));

	//download progress flags
	item->m_bDownloadInProgress = (state & k_EItemStateDownloading) || (state & k_EItemStateDownloadPending);

	//download readiness
	item->m_bIsDownloaded = (state & k_EItemStateInstalled) != 0;

	//if Steam still reports NeedsUpdate, treat as not ready
	if (state & k_EItemStateNeedsUpdate)
		item->m_bIsDownloaded = false;

	//only update metadata load state if it was waiting
	if (item->m_LoadState == CWorkshopItem::WorkshopItemLoadState::Requesting)
		item->m_LoadState = CWorkshopItem::WorkshopItemLoadState::Completed;

	UIState::NeedsActionButtonUpdate = true;
	DebugLog("OnWorkshopItemDownloaded() END");
}

void CMainModule::Shutdown()
{
	if (m_pchUserDefinedGamePath)
		delete[] m_pchUserDefinedGamePath; //created by StringToUtf8() when user defined game path himself in GetGamePath()

	m_vecWorkshopItems.clear();

#ifdef USE_MUTEX
	if (g_hInstanceMutex)
	{
		ReleaseMutex(g_hInstanceMutex);
		CloseHandle(g_hInstanceMutex);
	}
#endif

	SteamAPI_Shutdown();
}

void CMainModule::GetConfigOptions()
{
	g_EnableOverwrittenColumn	= ReadConfigBool("enable_overwritten_column", false);
	g_PromptUserOnManageItem	= ReadConfigBool("user_prompt_on_manage_item", false);
	g_UseFictiveJavaFiles		= ReadConfigBool("enable_fictive_java_files", false);
	g_UseFictiveClassFiles		= ReadConfigBool("enable_fictive_class_files", false);

	String^ gamePathFromConfig = ReadConfigString("game_path", nullptr);
	if (gamePathFromConfig)
		m_pchUserDefinedGamePath = StringToUtf8(gamePathFromConfig);

	DebugLog("GetConfigOptions()");
	DebugLog("g_EnableOverwrittenColumn: " + g_EnableOverwrittenColumn);
	DebugLog("g_PromptUserOnManageItem: " + g_PromptUserOnManageItem);
	DebugLog("gamePathFromConfig: " + gamePathFromConfig);
	DebugLog("m_pchUserDefinedGamePath: " + gcnew String(m_pchUserDefinedGamePath));
}

void CMainModule::DumpWorkshopItemsInfo()
{
	DebugLog("-------- DumpWorkshopItemsInfo()");
	DebugLog(String::Format("Total items: {0}", GetNumWorkshopItems()));
	DebugLog("");

	for (auto& item : m_vecWorkshopItems)
	{
		String^ itemCachePath = GetItemCachePath(item.get());

		if (String::IsNullOrEmpty(itemCachePath))
			itemCachePath = "<not installed>";

		DebugLog(String::Format(
				"ItemId: {0}\n"
				"Title: {1}\n"
				"Description: {2}\n"
				"AuthorID: {3}\n"
				"Size: ({4:F3} MB)\n"
				"GetItemCachePath(): {5}\n",
				UInt64(item->GetItemId()),
				gcnew String(item->GetTitle()),
				gcnew String(item->GetDescription()),
				item->GetAuthorID(),
				item->GetSizeInMegabytes(),
				itemCachePath
			));

		DebugLog(String::Format("Installed: {0}", item->m_bIsInstalled ? "true" : "false"));
		DebugLog("");
	}

	DebugLog("--------");
}

void CMainModule::DownloadAllWorkshopItems()
{
	for (auto& item : m_vecWorkshopItems)
	{
		if (item)
			item->ForceDownload();
	}
}

bool CMainModule::IsDemoMode()
{
	return m_bDemoMode;
}

void CMainModule::ErrorMessage(String^ message)
{
	MessageBox::Show(
		message,
		LOC_ERROR_MSG_CAPTION,
		MessageBoxButtons::OK,
		MessageBoxIcon::Error
	);
	Shutdown();
	exit(EXIT_FAILURE);
}

void CMainModule::WarningMessage(String^ message)
{
	MessageBox::Show(
		message,
		LOC_WARNING_MSG_CAPTION,
		MessageBoxButtons::OK,
		MessageBoxIcon::Warning
	);
}

void CMainModule::InfoMessage(String^ message, String^ caption)
{
	MessageBox::Show(
		message,
		caption,
		MessageBoxButtons::OK,
		MessageBoxIcon::Information
	);
}

bool CMainModule::UserPrompt(String^ message)
{
	DialogResult result = MessageBox::Show(
			message,
			LOC_USER_PROMPT_CAPTION,
			MessageBoxButtons::OKCancel,
			MessageBoxIcon::Question,
			MessageBoxDefaultButton::Button1
		);

	return result == DialogResult::OK;
}

bool CMainModule::UserPromptYesNo(String^ message, String^ caption)
{
	DialogResult result = MessageBox::Show(
		message,
		caption,
		MessageBoxButtons::YesNo,
		MessageBoxIcon::Question
	);

	return result == DialogResult::Yes;
}