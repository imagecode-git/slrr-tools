#pragma once

#define MAX_WORKSHOP_ITEMS 1024
#define MAX_TITLE 256
#define MAX_DESC 256
#define MAX_SCREENSHOTS 64
#define UGC_QUERY_PAGE_SIZE 50

#define DEBUG_LOG_FILENAME "debug.log"
#define CONFIG_INI_FILENAME "workshop_uploader_config.ini"

#define PARAM_DELIM ", " //delimiters of cmdline params

#include "stdafx.h"
#include "stdlib.h"
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <memory> //unique_ptr
#include <utility> //move
#include <locale.h>

#include "WorkshopItem.h"
#include "WorkshopManageTask.h"
#include "Locale.h"
#include "Helpers.h"
#include "Environment.h"

#include "steam/steam_api.h"
#pragma comment(lib, "steam/steam_api")

#include "parser/cpp/INIReader.h"

#ifdef _DEBUG
//memleaks detection, call _CrtDumpMemoryLeaks() to dump all leaks to debug output
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#define UPLOADER_VERSION "1.2.0"

using namespace std;

enum class WorkshopUploaderParam
{
	Action,
	ItemId,
	Title,
	Description,
	Visibility,
	Category,
	PreviewImage,
	ScreenshotPath,
	VideoUrl,
	FilesPath,
	UpdateComment,
	NoConfirm, //UploaderConfig.bNoConfirm - no UserPrompt() to confirm input params
	NoWait, //UploaderConfig.bNoWait - no "Press any key" to allow batch executions
	CreateDefaults, //UploaderConfig.bCreateDefaults - auto-correct policy for create mode, it fills default values if they're invalid or not set
	Empty
};

struct ParamEntry
{
	const char* key;
	WorkshopUploaderParam param;
};

//mapping table for ResolveParam()
static const ParamEntry kParamTable[] =
{
	//"mode":	ini config alias
	//"-mode"	long verbose CLI alias
	//"-m":		short CLI alias

	{ "mode",			WorkshopUploaderParam::Action },
	{ "-mode",			WorkshopUploaderParam::Action },
	{ "-m",				WorkshopUploaderParam::Action },

	{ "item-id",		WorkshopUploaderParam::ItemId },
	{ "-item-id",		WorkshopUploaderParam::ItemId },
	{ "-id",			WorkshopUploaderParam::ItemId },

	{ "title",			WorkshopUploaderParam::Title },
	{ "-title",			WorkshopUploaderParam::Title },
	{ "-t",				WorkshopUploaderParam::Title },

	{ "description",	WorkshopUploaderParam::Description },
	{ "-description",	WorkshopUploaderParam::Description },
	{ "-d",				WorkshopUploaderParam::Description },

	{ "visibility",		WorkshopUploaderParam::Visibility },
	{ "-visibility",	WorkshopUploaderParam::Visibility },
	{ "-v",				WorkshopUploaderParam::Visibility },

	{ "category",		WorkshopUploaderParam::Category },
	{ "-category",		WorkshopUploaderParam::Category },
	{ "-c",				WorkshopUploaderParam::Category },

	{ "preview",		WorkshopUploaderParam::PreviewImage },
	{ "-preview",		WorkshopUploaderParam::PreviewImage },
	{ "-p",				WorkshopUploaderParam::PreviewImage },

	{ "screenshots",	WorkshopUploaderParam::ScreenshotPath },
	{ "-screenshots",	WorkshopUploaderParam::ScreenshotPath },
	{ "-sc",			WorkshopUploaderParam::ScreenshotPath },

	{ "video-urls",		WorkshopUploaderParam::VideoUrl },
	{ "-video-urls",	WorkshopUploaderParam::VideoUrl },
	{ "-yt",			WorkshopUploaderParam::VideoUrl },

	{ "content",		WorkshopUploaderParam::FilesPath },
	{ "-content",		WorkshopUploaderParam::FilesPath },
	{ "-f",				WorkshopUploaderParam::FilesPath },

	{ "comment",		WorkshopUploaderParam::UpdateComment },
	{ "-comment",		WorkshopUploaderParam::UpdateComment },
	{ "-uc",			WorkshopUploaderParam::UpdateComment },
	
	{ "no-confirm",		WorkshopUploaderParam::NoConfirm },
	{ "-no-confirm",	WorkshopUploaderParam::NoConfirm },
	{ "-nc",			WorkshopUploaderParam::NoConfirm },

	{ "no-wait",		WorkshopUploaderParam::NoWait },
	{ "-no-wait",		WorkshopUploaderParam::NoWait },
	{ "-nw",			WorkshopUploaderParam::NoWait },

	{ "create-defaults",	WorkshopUploaderParam::CreateDefaults },
	{ "-create-defaults",	WorkshopUploaderParam::CreateDefaults },
	{ "-cdf",				WorkshopUploaderParam::CreateDefaults },

	{ "", WorkshopUploaderParam::Empty },
};

enum class UGCQueryPurpose
{
	Info,
	Select //for edit/delete
};

class MainModule
{
public:
	MainModule();
	~MainModule();

	//app lifecycle
	bool	Initialize();
	void	WaitForExit();
	void	Run();
	bool	IsAlive();
	
	//input data
	vector<pair<string, string>> ParseCommandLine(int argc, char** argv);
	vector<pair<string, string>> LoadIniConfig();

	//parameter handling
	WorkshopManageAction	ApplyParams(const vector<pair<string, string>>& params, WorkshopItem& item);
	WorkshopUploaderParam	ResolveParam(const string& arg); //resolve parameter with a mapping table to find a corresponding WorkshopUploaderParam
	const char*				GetParamKey(WorkshopUploaderParam param); //get a matching WorkshopUploaderParam as const char*
	
	//information requests
	void	PrintWorkshopItemInfo(WorkshopItem& item, bool bSuppressWarnings = false);
	void	PrintInitialUserInfo();
	void	PrintPaginationControls();
	void	RequestPublishedWorkshopItems();

	//item selection
	void PromptUserToSelectItem(const vector<SteamUGCDetails_t>& items);
	void HandleUGCPagination(const vector<SteamUGCDetails_t>& items);
	
	//pending jobs
	WorkshopManageAction m_PendingManageAction;
	WorkshopItem m_PendingWorkshopItem;

	//workshop item operations
	WorkshopManageResult ManageWorkshopItem(WorkshopItem& item, WorkshopManageAction action); //user makes decision here
	WorkshopManageResult ValidateAndSubmit(WorkshopItem&& item, WorkshopManageAction action);
	void SubmitWorkshopManageTask(WorkshopItem&& item, WorkshopManageAction action);

private:
	//callbacks
	void OnWorkshopManageTaskFinished(WorkshopManageTask* finishedTask, bool bSuccess);
	void OnPublishedItemsQueryCompleted(SteamUGCQueryCompleted_t* pWorkshopItemGetInfo, bool bIOFailure);

	//callback results
	CCallResult<MainModule, SteamUGCQueryCompleted_t> m_UGCQueryCompleted;

	//UGC requests
	UGCQueryPurpose m_CurrentUGCQueryPurpose;
	uint32 m_PublishedItemsListPageNum;
	uint32 m_TotalUGCPages;

	//Steam stuff
	UGCQueryHandle_t		m_hQueryUGCRequestDetails;
	UGCUpdateHandle_t		m_hQueryUGCUpdate;
	AppId_t					m_SteamAppId;
	AccountID_t				m_SteamAccountId;

	SteamUGCDetails_t		m_selectedPublishedItem;

	//task management
	vector<unique_ptr<WorkshopManageTask>> m_activeManageTasks;
	int m_pendingAsyncOps = 0; //operations for the async flow
	bool m_bIsRunning = false; //controls main loop lifetime
	bool m_bReadyToTerminate = false; //signals shutdown of the uploader

#ifdef USE_MUTEX
	bool EnsureSingleInstance(); //returns false if another instance of the app is trying to run
#endif
};