#pragma once

#define MAX_WORKSHOP_ITEMS 1024
#define MAX_TITLE 256
#define MAX_DESC 256
#define MAX_SCREENSHOTS 64
#define UGC_QUERY_PAGE_SIZE 50

#define DEBUG_LOG_FILENAME "debug.log"

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
	AutoDefaults, //UploaderConfig.bAutoDefaults - auto-correct policy that fills default values if they're invalid or not set
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
	{ "-mode",			WorkshopUploaderParam::Action },
	{ "-item-id",		WorkshopUploaderParam::ItemId },
	{ "-title",			WorkshopUploaderParam::Title },
	{ "-description",	WorkshopUploaderParam::Description },
	{ "-visibility",	WorkshopUploaderParam::Visibility },
	{ "-category",		WorkshopUploaderParam::Category },
	{ "-preview",		WorkshopUploaderParam::PreviewImage },
	{ "-screenshots",	WorkshopUploaderParam::ScreenshotPath },
	{ "-video-urls",	WorkshopUploaderParam::VideoUrl },
	{ "-content",		WorkshopUploaderParam::FilesPath },
	{ "-comment",		WorkshopUploaderParam::UpdateComment },
	
	{ "-no-confirm",	WorkshopUploaderParam::NoConfirm },
	{ "-no-wait",		WorkshopUploaderParam::NoWait },
	{ "-auto-defaults",	WorkshopUploaderParam::AutoDefaults },
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

	//parameter handling
	WorkshopManageAction	ParseParam(int argc, char** argv, WorkshopItem& item); //parse parameter from .bat file
	WorkshopUploaderParam	ResolveParam(const string& arg); //resolve parameter with a mapping table to find a corresponding WorkshopUploaderParam
	
	//information requests
	void	PrintWorkshopItemInfo(WorkshopItem& item);
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