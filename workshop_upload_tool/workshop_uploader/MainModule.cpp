/*
 * =============================================================================
 *  Street Legal Workshop Uploader
 * =============================================================================
 *  Copyright ImageCode LLC.
 * 
 *  Purpose: the tool creates new Steam workshop items, modifies or deletes
 *  existing ones.
 *
 *  This project is not affiliated with, endorsed by, or sponsored by Valve
 *  Corporation. Steam and Steam Workshop are trademarks of Valve Corporation.
 * =============================================================================
 */

#pragma once

#include "stdafx.h"
#include "MainModule.h"

#ifdef USE_MUTEX
HANDLE g_hInstanceMutex = NULL;

atomic<bool> g_bUserInterrupted{ false };

bool MainModule::EnsureSingleInstance()
{
	g_hInstanceMutex = CreateMutexW(
		NULL,
		TRUE, //request initial ownership
		L"Global\\StreetLegalWorkshopUploader"
	);

	if (!g_hInstanceMutex)
		return true; //fail open, don't brick the app

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(g_hInstanceMutex);
		g_hInstanceMutex = NULL;

		return false; //another instance is running
	}

	return true;
}
#endif

BOOL WINAPI ConsoleHandler(DWORD signal)
{
	switch (signal)
	{
		case CTRL_C_EVENT:
			DebugLog("ConsoleHandler(): Ctrl+C event has been captured");
			DebugLog("");
			g_bUserInterrupted = true;

			return TRUE;
	}

	return FALSE;
}

int main(int argc, char* argv[])
{
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	InitializeCriticalSection(&g_LogCriticalSection);

#ifdef RUN_TESTS
	extern void RunUnitTests();
	RunUnitTests();
	return 0;
#endif

	MainModule mainModule;

	//stop execution if the uploader is not ready to continue
	if (!mainModule.IsAlive())
		return 0;

	DebugLog("trying to enter main loop...");

	if (mainModule.Initialize())
	{
		WorkshopItem item;
		WorkshopManageResult manageResult;
		//WorkshopManageAction manageAction = mainModule.ParseParam(argc, argv, item);

		vector<pair<string, string>> targetParams;
		vector<pair<string, string>> iniParams = mainModule.LoadIniConfig();
		vector<pair<string, string>> cliParams = mainModule.ParseCommandLine(argc, argv);

		if (!iniParams.empty())
		{
			DebugLog("using parameters from " + string(CONFIG_INI_FILENAME));
			targetParams = iniParams;
		}
		else
		{
			DebugLog("using CLI params");
			targetParams = cliParams;
		}

		WorkshopManageAction manageAction = mainModule.ApplyParams(targetParams, item);

		do
		{
			manageResult = mainModule.ManageWorkshopItem(item, manageAction);
			
			switch (manageResult)
			{
			case WorkshopManageResult::Continue:
				DebugLog("WorkshopManageResult::Continue");

				if(manageAction != WorkshopManageAction::Unknown) //no need to run async tasks in this mode
					mainModule.Run();
				break;

			case WorkshopManageResult::Abort:
				DebugLog("WorkshopManageResult::Abort");
				ErrorMessage(LOC_WMR_TERMINATED_BY_USER);
				PrintMessage("");
				break;

			case WorkshopManageResult::Error:
				DebugLog("WorkshopManageResult::Error");
				ErrorMessage(LOC_WMR_CRITICAL_ERROR);
				PrintMessage("");
				break;

			case WorkshopManageResult::Retry:
				DebugLog("WorkshopManageResult::Retry");

				if (UserPrompt(LOC_WMR_RETRY))
					continue;
				else
					manageResult = WorkshopManageResult::Abort;

				break;
			}
		}
		while (manageResult == WorkshopManageResult::Retry && mainModule.IsAlive());
	}
	else
	{
		DebugLog("mainModule.Initialize() failed");
	}

	DebugLog("main loop complete");

	mainModule.WaitForExit();
	DeleteCriticalSection(&g_LogCriticalSection);

#ifdef _DEBUG
	_CrtDumpMemoryLeaks(); //throws debug output on termination, requires "Attach to process"
#endif
	
	return 0;
}

MainModule::MainModule()
{
#ifdef USE_MUTEX
	if (!EnsureSingleInstance())
	{
		ErrorMessage(LOC_MUTEX_ALREADY_RUNNING);
		WaitForExit();
	}
#endif

	m_SteamAppId = 0;
	m_SteamAccountId = 0;
	m_PublishedItemsListPageNum = 1;

	m_hQueryUGCRequestDetails = NULL;
	m_hQueryUGCUpdate = NULL;
}

MainModule::~MainModule()
{
}

bool MainModule::Initialize()
{
	PrintMessage("");
	PrintMessage(LINE_SEPARATOR_BOLD);
	PrintMessage("");
	PrintMessage(LOC_STARTUP_MESSAGE);
	PrintMessage(LOC_COPYRIGHT);
	PrintMessage(string(LOC_VERSION) + string(UPLOADER_VERSION));
	PrintMessage("");
	PrintMessage(LINE_SEPARATOR_BOLD);
	PrintMessage("");

	if (!SteamAPI_Init())
	{
		ErrorMessage(LOC_STEAM_INIT_FAIL);
		PrintMessage("");
		return false;
	}
	else
	{
		m_SteamAppId = SteamUtils()->GetAppID();
		m_SteamAccountId = SteamUser()->GetSteamID().GetAccountID();

		if (!m_SteamAccountId)
			DebugLog("Failed to obtain Steam user account ID!");

		PrintMessage(LOC_STEAM_INIT_OK);

		char* loc = setlocale(LC_ALL, "");

		if (loc == NULL)
			PrintMessage(LOC_LOCALE_NOT_SET);
		else
			PrintMessage(LOC_LOCALE_SET_TO + string(loc));

		PrintMessage("");
		ReadSteamAppId();
		PrintMessage("");

		string gameName;
		string playerName;

		if (SteamFriends())
		{
			const char* steamPersonaName = SteamFriends()->GetPersonaName();
			playerName = steamPersonaName ? steamPersonaName : "";
		}

		if (!playerName.empty())
			PrintMessage(LOC_LOGGED_IN_AS + playerName);
		else
			DebugLog("WARNING: failed to get player name from Steam");

		if (m_SteamAppId == STEAM_APPID_SLRR)
			gameName = LOC_TITLE_SLRR;

		if (m_SteamAppId == STEAM_APPID_SL1)
			gameName = LOC_TITLE_SL1R;

		if (!gameName.empty())
			PrintMessage(LOC_GAME + gameName);

		DebugLog("game: " + gameName);
	}

	return true;
}

void MainModule::WaitForExit()
{
	m_bIsRunning = false;
	m_bReadyToTerminate = true;

	if (!UploaderConfig::Instance().bNoWait)
	{
		DebugLog("WaitForExit(): waiting for user");
		PressAnyKey();
	}
}

void MainModule::Run()
{
	m_bIsRunning = true;
	
	if(!g_bUserInterrupted)
		PrintMessage(LOC_WAITING_FOR_STEAM);

	while (m_bIsRunning && !g_bUserInterrupted)
	{
		SteamAPI_RunCallbacks();

		for (auto& task : m_activeManageTasks)
			task->Tick();

		if (m_activeManageTasks.empty() && m_pendingAsyncOps == 0)
			m_bIsRunning = false;

		Sleep(10); //cap CPU load
	}

	if (g_bUserInterrupted)
	{
		//clear any previous progress messages
		if (m_PendingManageAction == WorkshopManageAction::Create ||
			m_PendingManageAction == WorkshopManageAction::Modify)
			ClearLine();

		DebugLog("Run() is terminated by g_bUserInterrupted");
		ErrorMessage(LOC_WMR_TERMINATED_BY_USER);
	}	
}

bool MainModule::IsAlive()
{
	return !m_bReadyToTerminate;
}

vector<pair<string, string>> MainModule::ParseCommandLine(int argc, char** argv)
{
	vector<pair<string, string>> params;

	for (int i = 1; i < argc; i++)
	{
		string key = argv[i];

		if (i + 1 >= argc)
		{
			PrintMessage(LOC_PARAM_MISSING_VALUE);
			break;
		}

		string value = argv[++i];

		params.emplace_back(key, value);
	}

	return params;
}

vector<pair<string, string>> MainModule::LoadIniConfig()
{
	vector<pair<string, string>> params;

	INIReader reader(CONFIG_INI_FILENAME);

	//silently return if config file is empty or unused
	if (reader.ParseError() != 0)
		return params;

	PrintMessage(string(LOC_READING_CONFIG_FILE) + string(CONFIG_INI_FILENAME));
	PrintMessage("");

	auto push = [&](WorkshopUploaderParam param, const string& value)
	{
		const char* key = GetParamKey(param);

		if (!value.empty())
			params.emplace_back(key, value);
	};

	//workshop item
	string secItem = "item";
	push(WorkshopUploaderParam::Title,			reader.Get(secItem, GetParamKey(WorkshopUploaderParam::Title), ""));
	push(WorkshopUploaderParam::Description,	reader.Get(secItem, GetParamKey(WorkshopUploaderParam::Description), ""));
	push(WorkshopUploaderParam::Visibility,		reader.Get(secItem, GetParamKey(WorkshopUploaderParam::Visibility), ""));
	push(WorkshopUploaderParam::Category,		reader.Get(secItem, GetParamKey(WorkshopUploaderParam::Category), ""));
	push(WorkshopUploaderParam::PreviewImage,	reader.Get(secItem, GetParamKey(WorkshopUploaderParam::PreviewImage), ""));
	push(WorkshopUploaderParam::ScreenshotPath,	reader.Get(secItem, GetParamKey(WorkshopUploaderParam::ScreenshotPath), ""));
	push(WorkshopUploaderParam::VideoUrl,		reader.Get(secItem, GetParamKey(WorkshopUploaderParam::VideoUrl), ""));
	push(WorkshopUploaderParam::FilesPath,		reader.Get(secItem, GetParamKey(WorkshopUploaderParam::FilesPath), ""));
	push(WorkshopUploaderParam::UpdateComment,	reader.Get(secItem, GetParamKey(WorkshopUploaderParam::UpdateComment), ""));

	//uploader
	string secUploader = "uploader";
	push(WorkshopUploaderParam::Action, reader.Get(secUploader, GetParamKey(WorkshopUploaderParam::Action), ""));

	UploaderConfig::Instance().bNoConfirm =
		reader.GetBoolean(secUploader, GetParamKey(WorkshopUploaderParam::NoConfirm), false);

	UploaderConfig::Instance().bCreateDefaults =
		reader.GetBoolean(secUploader, GetParamKey(WorkshopUploaderParam::CreateDefaults), false);

	UploaderConfig::Instance().bNoWait =
		reader.GetBoolean(secUploader, GetParamKey(WorkshopUploaderParam::NoWait), false);

	bool bDebugPreviewParams = false;
	if (bDebugPreviewParams)
	{
		PrintMessage(LINE_SEPARATOR, ConsoleTextColor::Cyan);
		PrintMessage("LoadIniConfig()", ConsoleTextColor::Cyan);
		PrintMessage("");

		for (const auto& p : params)
			PrintMessage(p.first + " = " + p.second, ConsoleTextColor::Cyan);

		PrintMessage(LINE_SEPARATOR, ConsoleTextColor::Cyan);
	}

	return params;
}

WorkshopUploaderParam MainModule::ResolveParam(const string& arg)
{
	for (const auto& entry : kParamTable)
	{
		if (!entry.key)
			break;

		if (arg == entry.key)
			return entry.param;
	}
	
	return WorkshopUploaderParam::Empty;
}

const char* MainModule::GetParamKey(WorkshopUploaderParam param)
{
	for (const auto& entry : kParamTable)
	{
		if (entry.param == WorkshopUploaderParam::Empty)
			break;

		if (entry.param == param)
			return entry.key;
	}

	return nullptr;
}

WorkshopManageAction MainModule::ApplyParams(const vector<pair<string, string>>& params, WorkshopItem& item)
{
	WorkshopManageAction manageAction = WorkshopManageAction::Unknown;

	for (const auto& p : params)
	{
		const string& arg = p.first;
		const string& value = p.second;

		WorkshopUploaderParam param = ResolveParam(arg);

		if (param == WorkshopUploaderParam::Empty)
		{
			DebugLog("ApplyParams: unknown parameter " + arg);
			continue;
		}

		switch (param)
		{
		case WorkshopUploaderParam::Action:
		{
			if (!ResolveEnumParam(value, WorkshopUploaderActionList, ARRAY_SIZE(WorkshopUploaderActionList), manageAction))
				manageAction = WorkshopManageAction::Unknown;

			break;
		}

		case WorkshopUploaderParam::ItemId:
		{
			uint64 itemIdFromStr = 0;

			if (!ParseUint64(value, itemIdFromStr))
				WarningMessage(LOC_INCORRECT_ITEM_ID + value); //don't brick the uploader, just default to 0 as AutoCorrectValidationPolicy would do

			item.SetItemId(static_cast<PublishedFileId_t>(itemIdFromStr));
			break;
		}

		case WorkshopUploaderParam::Title:
			item.SetTitle(value);
			break;

		case WorkshopUploaderParam::Description:
			item.SetDescription(value);
			break;

		case WorkshopUploaderParam::Visibility:
		{
			ERemoteStoragePublishedFileVisibility itemVisibility;
			if (!ResolveEnumParam(value, WorkshopItemVisibilityModesList, ARRAY_SIZE(WorkshopItemVisibilityModesList), itemVisibility))
				itemVisibility = k_ERemoteStoragePublishedFileVisibilityPrivate;

			item.SetVisibility(itemVisibility);
			break;
		}

		case WorkshopUploaderParam::Category:
		{
			vector<string> categoriesList;
			Tokenize(value, categoriesList, PARAM_DELIM);
			item.SetCategories(categoriesList);

			break;
		}

		case WorkshopUploaderParam::PreviewImage:
		{
			string fullPath = ResolveRelPath(value);
			item.SetPreviewImagePath(fullPath);
			break;
		}

		case WorkshopUploaderParam::ScreenshotPath:
		{
			string fullPath = ResolveRelPath(value);
			item.LoadScreenshotsFromDirectory(fullPath);
			break;
		}

		case WorkshopUploaderParam::VideoUrl:
		{
			vector<string> videoUrls;
			Tokenize(value, videoUrls, PARAM_DELIM); //split URL's into tokens

			for (string& url : videoUrls)
				url = ParseVideoUrl(url); //Steam only accepts ID's of YouTube videos

			item.SetVideoUrls(move(videoUrls));
			break;
		}

		case WorkshopUploaderParam::FilesPath:
		{
			string fullPath = ResolveRelPath(value);
			item.SetContentDir(fullPath);
			break;
		}

		case WorkshopUploaderParam::UpdateComment:
			item.SetUpdateComment(value);
			break;

		case WorkshopUploaderParam::NoConfirm:
			UploaderConfig::Instance().bNoConfirm = ParseBool(value);
			break;

		case WorkshopUploaderParam::NoWait:
			UploaderConfig::Instance().bNoWait = ParseBool(value);
			break;

		case WorkshopUploaderParam::CreateDefaults:
			UploaderConfig::Instance().bCreateDefaults = ParseBool(value);
			break;
		}
	}

	if (manageAction != WorkshopManageAction::Unknown &&
		manageAction != WorkshopManageAction::Info &&
		manageAction != WorkshopManageAction::Delete)
	{
		bool bSuppressWarnings = (manageAction == WorkshopManageAction::Modify);

		PrintMessage("");
		PrintMessage(LOC_PARAMS_PARSED, ConsoleTextColor::White);
		PrintMessage(LINE_SEPARATOR);
		PrintWorkshopItemInfo(item, bSuppressWarnings);
		PrintMessage(LINE_SEPARATOR);
	}

	return manageAction;
}

void MainModule::PrintWorkshopItemInfo(WorkshopItem& item, bool bSuppressWarnings)
{
	auto JoinCategories = [](const vector<string> cats)
	{
		if (cats.empty())
			return string{};

		string result = cats[0];

		for (size_t i = 1; i < cats.size(); ++i)
		{
			result += ", ";
			result += cats[i];
		}

		return result;
	};

	//prints "<not set>" if no value
	auto PrintStr = [&](const string& str)
	{
		if (!str.empty())
			PrintMessage(str);
		else
			WarningMessage(LOC_NOT_SET);
	};

	//prints "<not set>" if vector is empty
	auto PrintStrVector = [&](const vector<string>& strVec)
	{
		if (strVec.empty())
		{
			WarningMessage(LOC_NOT_SET);
			return;
		}

		for (const string& str : strVec)
			PrintMessage(str);
	};

	//workshop item ID
	PrintMessage(LOC_WII_ITEM_ID, ConsoleTextColor::White);
	PrintMessage(to_string(item.GetItemId()));
	PrintMessage("");

	//item title
	PrintMessage(LOC_WII_TITLE, ConsoleTextColor::White);
	PrintStr(item.GetTitle());
	PrintMessage("");
	
	//item description
	PrintMessage(LOC_WII_DESCRIPTION, ConsoleTextColor::White);
	PrintStr(item.GetDescription());
	PrintMessage("");

	//item visibility
	PrintMessage(LOC_WII_VISIBILITY, ConsoleTextColor::White);
	PrintStr(item.GetVisibilityString());
	PrintMessage("");
	
	//item categories
	PrintMessage(LOC_WII_CATEGORIES, ConsoleTextColor::White);
	PrintStr(JoinCategories(item.GetCategories()));
	PrintMessage("");
	
	//content dir
	PrintMessage(LOC_WII_CONTENT_DIR, ConsoleTextColor::White);
	PrintStr(item.GetContentDir());
	PrintMessage("");
	
	//preview image path
	PrintMessage(LOC_WII_PREVIEW_IMG, ConsoleTextColor::White);
	PrintStr(item.GetPreviewImagePath());
	PrintMessage("");
	
	//item screenshots
	PrintMessage(LOC_WII_SCREENSHOTS, ConsoleTextColor::White);
	PrintStrVector(item.GetScreenshots());
	PrintMessage("");
	
	//video URL's
	PrintMessage(LOC_WII_VIDEO_URLS, ConsoleTextColor::White);
	PrintStrVector(item.GetVideoUrls());
	PrintMessage("");
	
	//update notes
	PrintMessage(LOC_WII_UPDATE_COMMENT, ConsoleTextColor::White);
	PrintStr(item.GetUpdateComment());
	PrintMessage("");
}

void MainModule::PrintInitialUserInfo()
{
	PrintMessage("");
	WarningMessage(LOC_IUI_ABOUT);
	PrintMessage("");

	WarningMessage(LOC_IUI_MODES_HEADING);
	PrintMessage(LOC_IUI_DESC_CREATE);
	PrintMessage(LOC_IUI_DESC_MODIFY);
	PrintMessage(LOC_IUI_DESC_DELETE);
	PrintMessage(LOC_IUI_DESC_INFO);
	PrintMessage("");

	WarningMessage(LOC_IUI_OPTIONS_HEADING);
	PrintMessage(LOC_IUI_OPTION_MODE);
	PrintMessage(LOC_IUI_OPTION_ID);
	PrintMessage(LOC_IUI_OPTION_TITLE);
	PrintMessage(LOC_IUI_OPTION_DESC);
	PrintMessage(LOC_IUI_OPTION_VISIBILITY);
	PrintMessage(LOC_IUI_OPTION_CATEGORY);
	PrintMessage(LOC_IUI_OPTION_PREVIEW_IMG);
	PrintMessage(LOC_IUI_OPTION_SCREENSHOTS);
	PrintMessage(LOC_IUI_OPTION_VIDEO_URLS);
	PrintMessage(LOC_IUI_OPTION_CONTENT);
	PrintMessage(LOC_IUI_OPTION_UPDATE_COMM);
	PrintMessage(LOC_IUI_OPTION_NO_CONFIRM);
	PrintMessage("");

	PrintMessage(LOC_IUI_NOTES_QUOTES);
	PrintMessage(LOC_IUI_NOTES_VIDEO_URLS);
	PrintMessage(LOC_IUI_NOTES_ENGLISH_CHARS);
	PrintMessage(LOC_IUI_NOTES_INTERRUPTION);
	PrintMessage(LOC_IUI_NOTES_STEAM);
	PrintMessage("");
}

void MainModule::PrintPaginationControls()
{
	WarningMessage(LOC_UPC_CONTROLS);
	PrintMessage("");

	PrintMessage(string(UGC_PAGINATION_NEXT_PAGE) + string(LOC_UPC_NEXT_PAGE));
	PrintMessage(string(UGC_PAGINATION_PREV_PAGE) + string(LOC_UPC_PREV_PAGE));

	if (m_CurrentUGCQueryPurpose == UGCQueryPurpose::Select)
	{
		PrintMessage(string(UGC_PAGINATION_SELECT) + string(LOC_UPC_SELECT));
		PrintMessage(string(UGC_PAGINATION_QUIT) + string(LOC_UPC_SKIP_SELECTION));
	}

	if (m_CurrentUGCQueryPurpose == UGCQueryPurpose::Info)
		PrintMessage(string(UGC_PAGINATION_QUIT) + string(LOC_UPC_QUIT));

	PrintMessage("");
}

WorkshopManageResult MainModule::ManageWorkshopItem(WorkshopItem& item, WorkshopManageAction action)
{
	string strManageAction = string(LOC_ACTION) + EnumParamToString(action, WorkshopUploaderActionList, ARRAY_SIZE(WorkshopUploaderActionList));

	DebugLog("ManageWorkshopItem():");
	DebugLog("   action: " + strManageAction);
	DebugLog("   itemId: " + to_string(item.GetItemId()));
	DebugLog("");

	m_PendingWorkshopItem = item;
	m_PendingManageAction = action;

	if (action == WorkshopManageAction::Unknown)
	{
		PrintInitialUserInfo();
		return WorkshopManageResult::Continue;
	}
	else //don't print the action if it's undefined or unknown
	{
		PrintMessage(strManageAction, ConsoleTextColor::White);
		PrintMessage("");
	}

	PublishedFileId_t itemId = item.GetItemId();

	if (action != WorkshopManageAction::Info)
	{
		string userMessage = LOC_OPERATION_BEGIN;

		if(itemId > 0)
			userMessage += to_string(itemId);

		PrintMessage(userMessage);
	}

	switch (action)
	{
	case WorkshopManageAction::Create:
		break;
	
	case WorkshopManageAction::Modify:
	case WorkshopManageAction::Delete:
		if (!itemId)
		{
			m_CurrentUGCQueryPurpose = UGCQueryPurpose::Select; //list published items + select prompt
			RequestPublishedWorkshopItems();
			return WorkshopManageResult::Continue;
		}

		break;

	case WorkshopManageAction::Info:
		m_CurrentUGCQueryPurpose = UGCQueryPurpose::Info; //only list published items
		RequestPublishedWorkshopItems();

		return WorkshopManageResult::Continue;
		break;
	}

	return ValidateAndSubmit(move(item), action);
}

WorkshopManageResult MainModule::ValidateAndSubmit(WorkshopItem&& item, WorkshopManageAction action)
{
	DebugLog("ValidateAndSubmit():");
	DebugLog("   itemId: " + to_string(item.GetItemId()));

	PrintMessage(LOC_VALIDATING_PARAMS);

	//validate input params for this item
	CreateValidationPolicy createPolicy;
	DeleteValidationPolicy deletePolicy;
	BaseValidationPolicy basePolicy;
	IWorkshopValidationPolicy* itemValidationPolicy = nullptr;

	if (action == WorkshopManageAction::Create)
	{
		itemValidationPolicy = &createPolicy;
	}
	else if (action == WorkshopManageAction::Delete)
	{
		itemValidationPolicy = &deletePolicy;
	}
	else
	{
		itemValidationPolicy = &basePolicy;
	}

	item.ValidateForSubmission(*itemValidationPolicy);

	//collect warning messages reported by the item's validation policy
	const auto& policyMessages = itemValidationPolicy->GetMessages();

	//critical error, can't continue
	if (itemValidationPolicy->HasErrors())
	{
		DebugLog("itemValidationPolicy has errors:");
		DebugLog("");

		PrintMessage("");
		ErrorMessage(LOC_ERRORS_DETECTED);
		PrintMessage("");

		for (const auto& errorMessage : policyMessages)
		{
			if (errorMessage.type == WorkshopItemValidationMessage::Type::Error)
			{
				DebugLog("   " + errorMessage.message);
				DebugLog("");

				ErrorMessage(errorMessage.message);
				PrintMessage("");
			}	
		}

		return WorkshopManageResult::Error;
	}

	if (itemValidationPolicy->HasWarnings())
	{
		DebugLog("itemValidationPolicy has warnings:");
		DebugLog("");

		PrintMessage("");
		WarningMessage(LOC_WARNINGS_DETECTED);
		PrintMessage("");

		//print all validation warnings for this item
		for (const auto& warningMessage : policyMessages)
		{
			if (warningMessage.type == WorkshopItemValidationMessage::Type::Warning)
			{
				DebugLog("   " + warningMessage.message);
				DebugLog("");

				WarningMessage(warningMessage.message);
				PrintMessage("");
			}
		}
	}
	else
	{
		DebugLog("   validation successful");
		DebugLog("");

		PrintMessage("");
		PrintMessage(LOC_VALIDATE_OK);
		PrintMessage("");
	}

	bool bNoConfirm = UploaderConfig::Instance().bNoConfirm;

	if (!bNoConfirm)
	{
		if (!UserPrompt(LOC_PROMPT_SUBMIT_UGC))
		{
			if (action == WorkshopManageAction::Delete || action == WorkshopManageAction::Modify) //avoid another error message coming from a sync task
				g_bUserInterrupted = true;

			return WorkshopManageResult::Abort;
		}
	}

	SubmitWorkshopManageTask(move(item), action);

	return WorkshopManageResult::Continue;
}

void MainModule::SubmitWorkshopManageTask(WorkshopItem&& item, WorkshopManageAction action)
{
	DebugLog("SubmitWorkshopManageTask():");
	DebugLog("   itemId: " + to_string(item.GetItemId()));

	auto task = make_unique<WorkshopManageTask>(
		m_SteamAppId,
		action,
		std::move(item),
		[this](WorkshopManageTask* task, bool success)
		{
			OnWorkshopManageTaskFinished(task, success);
		});

	task->Start();
	m_activeManageTasks.push_back(std::move(task));

	DebugLog("   task has been pushed to m_activeManageTasks");
	DebugLog("");
}

void MainModule::OnWorkshopManageTaskFinished(WorkshopManageTask* finishedTask, bool bSuccess)
{
	DebugLog("OnWorkshopManageTaskFinished():");
	DebugLog("   itemId: " + to_string(finishedTask->GetWorkshopItem().GetItemId()));

	auto taskPos = remove_if(
		m_activeManageTasks.begin(),
		m_activeManageTasks.end(),
		[finishedTask](const std::unique_ptr<WorkshopManageTask>& ptr)
		{
			return ptr.get() == finishedTask;
		});

	m_activeManageTasks.erase(taskPos, m_activeManageTasks.end());

	DebugLog("   task has been removed from m_activeManageTasks");
	DebugLog("");
}

void MainModule::RequestPublishedWorkshopItems()
{
	DebugLog("requesting published workshop items...");

	PrintMessage(LOC_REQ_PUBLISHED_ITEMS);

	m_pendingAsyncOps++;

	m_hQueryUGCRequestDetails = SteamUGC()->CreateQueryUserUGCRequest(m_SteamAccountId, k_EUserUGCList_Published, k_EUGCMatchingUGCType_Items, k_EUserUGCListSortOrder_TitleAsc, m_SteamAppId, m_SteamAppId, m_PublishedItemsListPageNum);
	m_UGCQueryCompleted.Set(SteamUGC()->SendQueryUGCRequest(m_hQueryUGCRequestDetails), this, &MainModule::OnPublishedItemsQueryCompleted);
}

void MainModule::PromptUserToSelectItem(const vector<SteamUGCDetails_t>& items)
{
	int itemIndex = -1;
	
	RequestInt(LOC_CHOOSE_ITEM, itemIndex);
	itemIndex -= 1;

	if (itemIndex < 0 || itemIndex >= static_cast<int>(items.size()))
	{
		if (UserPrompt(LOC_INVALID_SELECTION))
			PromptUserToSelectItem(items);

		return;
	}

	m_selectedPublishedItem = items[itemIndex];

	string strItemId = to_string(m_selectedPublishedItem.m_nPublishedFileId);
	string itemTitle = string(m_selectedPublishedItem.m_rgchTitle);

	PrintMessage("");
	PrintMessage(LOC_ITEM_SELECTED + itemTitle + " (" + strItemId + ")");

	m_PendingWorkshopItem.SetItemId(m_selectedPublishedItem.m_nPublishedFileId);
	ValidateAndSubmit(move(m_PendingWorkshopItem), m_PendingManageAction);
}

void MainModule::HandleUGCPagination(const vector<SteamUGCDetails_t>& items)
{
	if (items.empty())
		return;

	while (!g_bUserInterrupted)
	{
		string strInput;

		if (m_TotalUGCPages > 1)
		{
			PrintPaginationControls();
			RequestString(LOC_UPC_ENTER_KEY, strInput);
		}

		if (g_bUserInterrupted)
			return;

		if (strInput == UGC_PAGINATION_NEXT_PAGE)
		{
			if (m_PublishedItemsListPageNum < m_TotalUGCPages)
			{
				m_PublishedItemsListPageNum++;
				RequestPublishedWorkshopItems();
				return;
			}
			else
			{
				WarningMessage(LOC_UPC_ALREADY_LAST_PAGE);
				continue;
			}
		}
		else if (strInput == UGC_PAGINATION_PREV_PAGE)
		{
			if (m_PublishedItemsListPageNum > 1)
			{
				m_PublishedItemsListPageNum--;
				RequestPublishedWorkshopItems();
				return;
			}
			else
			{
				WarningMessage(LOC_UPC_ALREADY_FIRST_PAGE);
				continue;
			}
		}
		else if (strInput == UGC_PAGINATION_SELECT && m_CurrentUGCQueryPurpose == UGCQueryPurpose::Select)
		{
			PromptUserToSelectItem(items);
			return;
		}
		else if (strInput == UGC_PAGINATION_QUIT)
		{
			return;
		}
		else
		{
			WarningMessage(LOC_UPC_INCORRECT_INPUT);
			PrintMessage("");
			continue;
		}
	}
}

void MainModule::OnPublishedItemsQueryCompleted(SteamUGCQueryCompleted_t* pResult, bool bIOFailure)
{
	if (bIOFailure || pResult->m_eResult != k_EResultOK)
	{
		ErrorMessage(LOC_PUBLISHED_ITEMS_FAIL);
		
		SteamUGC()->ReleaseQueryUGCRequest(pResult->m_handle);
		m_pendingAsyncOps--;

		return;
	}

	uint32 numResults = pResult->m_unNumResultsReturned; //number of items on this page, default 50 items per page
	uint32 totalResults = pResult->m_unTotalMatchingResults; //total number of items

	DebugLog("user has " + to_string(totalResults) + " published workshop items");

	vector<SteamUGCDetails_t> publishedItems;
	publishedItems.reserve(numResults);

	const uint32 pageSize = UGC_QUERY_PAGE_SIZE;
	m_TotalUGCPages = (totalResults + pageSize - 1) / pageSize;

	if (numResults > 0)
	{
		PrintMessage("");
		PrintMessage(LOC_YOUR_PUBLISHED_ITEMS, ConsoleTextColor::White);
		PrintMessage("");
	}

	for (uint32 i = 0; i < numResults; ++i)
	{
		SteamUGCDetails_t itemDetails;

		if (!SteamUGC()->GetQueryUGCResult(pResult->m_handle, i, &itemDetails))
			continue;
		
		publishedItems.push_back(itemDetails);
		
		string userMessage = to_string(i + 1) + ". ";
		userMessage += itemDetails.m_rgchTitle;
		userMessage += " (" + to_string(itemDetails.m_nPublishedFileId) + ") ";
		
		string itemVisibility = EnumParamToString(itemDetails.m_eVisibility, WorkshopItemVisibilityModesList, ARRAY_SIZE(WorkshopItemVisibilityModesList));
		userMessage += "[" + itemVisibility + "]";

		PrintMessage(userMessage);
	}

	PrintMessage("");

	if (m_TotalUGCPages > 1)
	{
		PrintMessage(LOC_PAGE + to_string(m_PublishedItemsListPageNum) + "/" + to_string(m_TotalUGCPages));
		PrintMessage("");
	}

	SteamUGC()->ReleaseQueryUGCRequest(m_hQueryUGCRequestDetails);
	m_hQueryUGCRequestDetails = NULL;
	m_pendingAsyncOps--;

	if (numResults == 0)
	{
		WarningMessage(LOC_NO_PUBLISHED_ITEMS);
	}
	else if (m_TotalUGCPages > 1)
	{
		HandleUGCPagination(publishedItems);
	}
	else if (m_CurrentUGCQueryPurpose == UGCQueryPurpose::Select)
	{
		PromptUserToSelectItem(publishedItems);
	}
}