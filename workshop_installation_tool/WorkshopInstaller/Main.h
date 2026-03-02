#pragma once

#define STEAM_APPID_SLRR 497180
#define STEAM_APPID_SL1 1571280

#define STEAM_APPID_SLRR_INSTALLER 990120
#define STEAM_APPID_SL1_INSTALLER 4363130

#define STEAM_APPID_FILE "steam_appid.txt"
#define UNINSTALL_MASK "workshop_uninstall"
#define SENTINEL_MASK ".sentinel"

#define DEBUG_LOG_FILENAME "debug.log"

#define MAX_WORKSHOP_ITEMS 1024
#define MAX_TITLE 256
#define MAX_DESC 256
#define ITEM_CACHE_LIMIT 10 //in seconds, tells us: "reuse cached workshop item data if it's X seconds old"
#define METADATA_TIMEOUT 15000 //in milliseconds

#define MODE_UNDEFINED 0x0
#define MODE_INSTALL 0x1
#define MODE_REMOVE 0x2

#define MAIN CMainModule::GetInstance()

#include <algorithm> //remove_if()
#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <memory> //unique_ptr
#include <utility> //move
#include <vector>
#include "Locale.h"
#include "InstallForm.h"
#include "ManagerForm.h"
#include "DialogForm.h"
#include "CWorkshopItem.h"
#include "stdlib.h"
#include "resource.h"
#include "steam/steam_api.h"
#include "Helpers.h"
#include "Options.h"
#include "UIState.h"

#pragma comment(lib, "steam/steam_api")
#pragma comment(lib, "Shell32.lib") //ShellExecuteW
#pragma comment(lib, "Advapi32.lib") //RegOpenKey

#using <System.Core.dll> //HashSet, Dictionary

using namespace WorkshopInstaller;
using namespace System::Windows::Forms;
using namespace System::Runtime::InteropServices;
using namespace System::Text;
using namespace System::Diagnostics; //Process
using namespace System::IO; //files
using namespace System::Collections::Generic; //List<T>, HashSet
using namespace System::Text::RegularExpressions;

#ifdef _DEBUG
//memleaks detection, call _CrtDumpMemoryLeaks() to dump all leaks to debug output
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#ifdef USE_MUTEX
extern HANDLE g_hInstanceMutex;
#endif

using namespace std;

class CMainModule
{
public:
	static CMainModule* GetInstance()
	{
		if (!m_ModuleInstance)
			m_ModuleInstance = new CMainModule(); //this should not be deleted at shutdown, because process exit is the destructor
		
		return m_ModuleInstance;
	}

	enum FileCollectMode
	{
		FC_ALL_FILES,
		FC_NO_JVM_FILES,
		FC_JVM_FILES_ONLY
	};

	enum WorkshopItemManageMode
	{
		MM_INSTALL,
		MM_REMOVE
	};

	bool	InitModule();
	void	Shutdown();

	void	GetConfigOptions();

	//direct workshop item operations
	bool	ManageWorkshopItem(uint64 itemId); //install or remove workshop item from the game
	bool	DetermineWorkshopItemAction(uint64 itemId, CWorkshopItem*& outItem, WorkshopItemManageMode& outMode, String^% outItemTitle);
	bool	InstallWorkshopItem(uint64 itemId, CWorkshopItem* item, String^ itemTitle);
	bool	RemoveWorkshopItem(uint64 itemId, CWorkshopItem* item, String^ itemTitle);
	
	//sentinel lifecycle
	void	CreateSentinelFile(uint64 itemId); //creates a sentinel marking "installation in progress" and if it remains after app restart or failure, installation must be verified
	void	DeleteSentinelFile(uint64 itemId); //deletes sentinel only after successful installation commit
	void	CleanupOrphanSentinels(); //removes orphan sentinel files left from previous installs
	bool	IsInstallInterrupted(uint64 itemId); //tells us that sentinel file exists and VerifyInstallation() must be called
	
	//integrity
	bool	VerifyInstallation(uint64 itemId); //compares contents of uninstall log against Steam cache
	
	//state handling
	void	RefreshInstallerState(); //refresh workshop item info, their states, etc.
	void	MarkInstallationBroken(uint64 itemId, bool bIsBroken); //flags item as broken/needs reinstall
	String^ FormatItemState(uint32 state);
	
	bool	AllWorkshopItemsResolved();

	List<String^>^	GetInstalledWorkshopItems();
	List<String^>^	GetSubscribedItems();
	List<String^>^	GetCachedItems(); //returns folders from steamapps\workshop\content\<SteamappId>
	List<UInt64>^	GetKnownItemIds(); //returns all item ID's from the m_vecWorkshopItems
	List<UInt64>^	GetInstalledItemIds(); //returns ID's of installed items only

	CWorkshopItem* FindWorkshopItem(uint64 itemId); //find item by itemId
	void RemoveWorkshopItemByPtr(CWorkshopItem* item); //safely remove item from m_vecWorkshopItems

	void	QueryWorkshopState(); //request what items have been downloaded by Steam and ready for processing, request previously installed items

	int		GetNumWorkshopItems();
	int		GetNumSubscribedItems();

	//path handling
	String^ GetWorkshopPath(); //workshop -> content path, only used by GetCachedItems()
	String^	GetItemCachePath(CWorkshopItem* item); //workshop cache path for a specific item
	String^ GetUninstallRoot();
	String^ GetUninstallFilePath(CWorkshopItem* item);
	String^ GetSentinelFilePath(uint64 itemId);
	String^ GetInstallerPath(); //path to the installer exe
	String^ GetGamePath(); //path to the game itself
	String^ GetSteamPath(); //returns SteamApps()->GetAppInstallDir() for m_SteamAppId
	bool	VerifyGamePath(String^ gamePath); //returns true if any signs of Street Legal executable are found in this folder
	
	//appId handling
	AppId_t	GetSteamAppId(); //returns Steam App ID that the installer already knows
	AppId_t ReadSteamAppId(); //reads Steam App ID from steam_appid.txt

	List<String^>^ GetFilesToInstall(uint64 itemId, FileCollectMode collectMode); //normalized relative paths
	List<String^>^ GetCachedFiles(uint64 itemId); //get files from Steam cache
	HashSet<String^>^ GetInstalledFiles(uint64 itemId); //normalized relative paths
	
	//file conflict handling
	Dictionary<String^, List<uint64>^>^ BuildFileOwnerMap(); //reads uninstall log of each workshop item and tells us which item currently owns which installed file
	bool IsItemMarkedOverwritten(uint64 itemId, bool bShowReport = false); //tells the UI that this item should be marked as overwritten by other items
	bool IsItemOverwrittenByOwnership(uint64 itemId, HashSet<String^>^ installedFiles, Dictionary<String^, UInt64>^ fileOwners); //determines whether any installed file of this item is currently owned by another item
	bool CheckFileConflicts(uint64 itemId, List<String^>^ candidateFiles); //replicates file conflict behavior of the g13ba's installer
	bool IsItemOverwrittenByAnotherItem(uint64 itemId); //to comply with the g13ba's installer

	//messages, prompts
	void	ErrorMessage(String^ message);
	void	WarningMessage(String^ message);
	void	InfoMessage(String^ message, String^ caption);
	bool	UserPrompt(String^ message);
	bool	UserPromptYesNo(String^ message, String^ caption);

	//debug tools
	void	DumpWorkshopItemsInfo();
	void	DownloadAllWorkshopItems();

	//misc
	bool	IsDemoMode();

private:
	CMainModule();
	~CMainModule();

	static CMainModule* m_ModuleInstance;

	AppId_t	m_SteamAppId;
	int		m_nWorkshopNumSubscribedItems;
	bool	m_bLoadingComplete;
	bool	m_bAskUserForGamePath;
	bool	m_bDemoMode; //action button is disabled in this mode
	
	PublishedFileId_t m_vecSubscribedItems[MAX_WORKSHOP_ITEMS];
	vector<unique_ptr<CWorkshopItem>> m_vecWorkshopItems; //handling the vector with unique_ptr instead of new/delete

	char* m_pchUserDefinedGamePath; //better to use std::string instead

#ifdef USE_MUTEX
	bool EnsureSingleInstance(); //returns false if another instance of the installer is trying to run
#endif

	STEAM_CALLBACK(CMainModule, OnWorkshopItemDownloaded, DownloadItemResult_t);
};