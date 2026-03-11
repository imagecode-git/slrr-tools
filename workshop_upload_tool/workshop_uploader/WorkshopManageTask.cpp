#include "stdafx.h"
#include "WorkshopManageTask.h"

using namespace std;

void WorkshopManageTask::Start()
{
	auto ugc = SteamUGC();
	PublishedFileId_t itemId = m_workshopItem.GetItemId();

	if (!itemId && m_workshopAction != WorkshopManageAction::Create)
	{
		DebugLog("WorkshopManageTask::Start()");
		DebugLog("   ERROR: itemId is not defined!");
	}
	
	switch (m_workshopAction)
	{
		case WorkshopManageAction::Create:
		{
			SetCursorVisible(false);

			SteamAPICall_t call = ugc->CreateItem(m_steamAppId, k_EWorkshopFileTypeCommunity);
			m_SteamCreateCall.Set(call, this, &WorkshopManageTask::OnWorkshopItemCreated);
			m_bIsUpdateInProgress = false;

			break;
		}

		case WorkshopManageAction::Modify:
		{
			SetCursorVisible(false);
			m_bIsUpdateInProgress = true;
			QueryWorkshopItemDetails(itemId);

			break;
		}

		case WorkshopManageAction::Delete:
		{
			SetCursorVisible(false);
			m_bIsUpdateInProgress = true;

			if (!m_IsRollbackTask)
				QueryWorkshopItemDetails(itemId);
			else
				ContinueAfterValidation(); //rollback: skip validation but still proceed

			break;
		}
	}
}

void WorkshopManageTask::ContinueAfterValidation()
{
	ClearLine();

	auto ugc = SteamUGC();
	PublishedFileId_t itemId = m_workshopItem.GetItemId();

	switch (m_workshopAction)
	{
		case WorkshopManageAction::Modify:
		{
			if (!m_workshopItem.HasUpdateFields())
			{
				PrintMessage("");
				ErrorMessage(LOC_NOTHING_TO_UPDATE);

				NotifyFinished(false);
				return;
			}

			SetCursorVisible(false);

			BuildUGCUpdateRequest();
			PrintMessage(LOC_TRY_EDIT_ITEM + to_string(itemId));

			SteamAPICall_t call = ugc->SubmitItemUpdate(m_SteamUGCUpdateHandle, m_workshopItem.GetUpdateComment().c_str());
			m_SteamSubmitCall.Set(call, this, &WorkshopManageTask::OnWorkshopItemSubmitted);
			m_bIsUpdateInProgress = true;

			break;
		}

		case WorkshopManageAction::Delete:
		{
			if (!m_IsRollbackTask)
				PrintMessage(LOC_TRY_DELETE_ITEM + to_string(itemId));

			SteamAPICall_t call = ugc->DeleteItem(itemId);
			m_SteamDeleteCall.Set(call, this, &WorkshopManageTask::OnWorkshopItemDeleted);
			m_bIsUpdateInProgress = true;

			break;
		}
	}
}

void WorkshopManageTask::NotifyFinished(bool bSuccess)
{
	if (m_onFinished)
		m_onFinished(this, bSuccess);
}

void WorkshopManageTask::BuildUGCUpdateRequest()
{
	ISteamUGC* ugc = SteamUGC();
	bool bUseContentDir = false;
	bool bUsePreviewImage = false;

	PrintMessage(LOC_LOADING_DATA);

	m_SteamUGCUpdateHandle = ugc->StartItemUpdate(m_steamAppId, m_workshopItem.GetItemId());

	const string& itemTitle = m_workshopItem.GetTitle();
	const string& itemDescription = m_workshopItem.GetDescription();
	const string& itemContentDir = m_workshopItem.GetContentDir();
	const string& itemPreviewImage = m_workshopItem.GetPreviewImagePath();
	
	ERemoteStoragePublishedFileVisibility itemVisibility = m_workshopItem.GetVisibility();

	//getters return const vector<string>&, no copying is necessary here
	const auto& itemScreenshots = m_workshopItem.GetScreenshots();
	const auto& itemVideoUrls = m_workshopItem.GetVideoUrls();
	const auto& itemCategories = m_workshopItem.GetCategories();

	//cannot create items with no content or preview image
	if (m_workshopAction == WorkshopManageAction::Create)
	{
		bUseContentDir = true;
		bUsePreviewImage = true;

		ugc->SetItemUpdateLanguage(m_SteamUGCUpdateHandle, DEF_ITEM_LANGUAGE);
		ugc->SetItemMetadata(m_SteamUGCUpdateHandle, DEF_ITEM_METADATA);
	}
	else if (m_workshopAction == WorkshopManageAction::Modify)
	{
		//don't brick the uploader if content or preview image are not set in modify mode
		bUseContentDir = m_workshopItem.HasValidContentDir();
		bUsePreviewImage = m_workshopItem.HasValidPreviewImage();
	}

	if(m_workshopItem.HasTitle())
		ugc->SetItemTitle(m_SteamUGCUpdateHandle, itemTitle.c_str());

	if(m_workshopItem.HasDescription())
		ugc->SetItemDescription(m_SteamUGCUpdateHandle, itemDescription.c_str());

	if(m_workshopItem.HasVisibility())
		ugc->SetItemVisibility(m_SteamUGCUpdateHandle, itemVisibility);

	if(bUseContentDir)
		ugc->SetItemContent(m_SteamUGCUpdateHandle, itemContentDir.c_str());

	if (bUsePreviewImage)
		ugc->SetItemPreview(m_SteamUGCUpdateHandle, itemPreviewImage.c_str());

	//categories and tags
	if (m_workshopItem.HasCategories())
	{
		vector<const char*> tagPtrs;
		tagPtrs.reserve(itemCategories.size());

		for (const string& category : itemCategories)
			tagPtrs.push_back(category.c_str());

		SteamParamStringArray_t tags;
		tags.m_nNumStrings = static_cast<int32>(tagPtrs.size());
		tags.m_ppStrings = tagPtrs.data();

		ugc->SetItemTags(m_SteamUGCUpdateHandle, &tags);
	}

	//screenshots and videos
	bool bReplaceScreenshots = m_workshopItem.HasScreenshots();
	bool bReplaceVideos = m_workshopItem.HasVideoUrls();

	vector<size_t> indicesToRemove;

	if (bReplaceScreenshots)
		indicesToRemove.insert(indicesToRemove.end(),
			m_itemImagePreviewIndices.begin(),
			m_itemImagePreviewIndices.end());

	if (bReplaceVideos)
		indicesToRemove.insert(indicesToRemove.end(),
			m_itemVideoPreviewIndices.begin(),
			m_itemVideoPreviewIndices.end());

	//sort, remove in reverse
	sort(indicesToRemove.begin(), indicesToRemove.end());

	for (auto it = indicesToRemove.rbegin(); it != indicesToRemove.rend(); ++it)
		ugc->RemoveItemPreview(m_SteamUGCUpdateHandle, *it);

	//re-add media
	if (bReplaceScreenshots)
	{
		for (const string& screenshotPath : itemScreenshots)
			ugc->AddItemPreviewFile(m_SteamUGCUpdateHandle, screenshotPath.c_str(), k_EItemPreviewType_Image);
	}

	if (bReplaceVideos)
	{
		for (const string& videoUrl : itemVideoUrls)
			ugc->AddItemPreviewVideo(m_SteamUGCUpdateHandle, videoUrl.c_str());
	}
}

void WorkshopManageTask::Tick()
{
	if (!m_bIsUpdateInProgress)
		return;

	uint64 uploadProcessed = 0;
	uint64 uploadTotal = 0;

	EItemUpdateStatus statusCode = SteamUGC()->GetItemUpdateProgress(
		m_SteamUGCUpdateHandle,
		&uploadProcessed,
		&uploadTotal
	);

	string statusPrefix = "";
	bool bShowSpinner = false;

	switch (statusCode)
	{
		case k_EItemUpdateStatusPreparingConfig:
		{
			statusPrefix = LOC_US_PREPARING_CONFIG;
			bShowSpinner = true;
			break;
		}

		case k_EItemUpdateStatusPreparingContent:
		{
			statusPrefix = LOC_US_READING_CONTENT;
			bShowSpinner = false;
			break;
		}

		case k_EItemUpdateStatusUploadingContent:
		{
			statusPrefix = LOC_US_UPLOADING_CHANGES;
			bShowSpinner = true;
			break;
		}

		case k_EItemUpdateStatusUploadingPreviewFile:
		{
			statusPrefix = LOC_US_UPLOADING_PREVIEW;
			bShowSpinner = true;
			break;
		}

		case k_EItemUpdateStatusCommittingChanges:
		{
			statusPrefix = LOC_US_COMMITTING_CHANGES;
			bShowSpinner = true;
			break;
		}
	}

	bool bShowProgress = (statusCode == k_EItemUpdateStatusPreparingContent && uploadTotal > 0);

	if (bShowProgress)
		PrintProgress(uploadProcessed, uploadTotal, statusPrefix);
	else if(bShowSpinner)
		PrintSpinner(statusPrefix);

	m_lastUploadProcessed = uploadProcessed;
	m_lastUploadTotal = uploadTotal;
}

WorkshopItem& WorkshopManageTask::GetWorkshopItem()
{
	return m_workshopItem;
}

WorkshopManageAction WorkshopManageTask::GetManageAction() const
{
	return m_workshopAction;
}

void WorkshopManageTask::OnWorkshopItemCreated(CreateItemResult_t* result, bool bIOFailure)
{
	if (bIOFailure)
	{
		ErrorMessage(LOC_FAILED_TO_CREATE_ITEM);
		return;
	}

	PublishedFileId_t receivedItemId = result->m_nPublishedFileId;
	m_workshopItem.SetItemId(receivedItemId);

	m_IsNewlyCreatedItem = true;

	auto policy = InterpretSteamResult(result->m_eResult);

	if (!policy.success)
	{
		if (policy.fatal)
			ErrorMessage(policy.message);
		else
			WarningMessage(policy.message);

		m_IsRollbackTask = true;
		m_workshopAction = WorkshopManageAction::Delete;
		
		ResetUpdateProgress();
		Start();

		return;
	}
	else
	{
		PrintMessage(LOC_RECEIVED_ITEM_ID + to_string(receivedItemId));
	}

	BuildUGCUpdateRequest();

	SteamAPICall_t submitCall = SteamUGC()->SubmitItemUpdate(m_SteamUGCUpdateHandle, m_workshopItem.GetUpdateComment().c_str());
	m_SteamSubmitCall.Set(submitCall, this, &WorkshopManageTask::OnWorkshopItemSubmitted);

	m_bIsUpdateInProgress = true;
}

void WorkshopManageTask::OnWorkshopItemSubmitted(SubmitItemUpdateResult_t* result, bool bIOFailure)
{
	if (bIOFailure)
	{
		ErrorMessage(LOC_FAILED_TO_UPDATE_ITEM);
		return;
	}

	SetCursorVisible(true);
	ClearLine(); //clear progress message

	auto policy = InterpretSteamResult(result->m_eResult);
	
	PrintMessage("");

	if (policy.success)
		SuccessMessage(policy.message);
	else
	{
		ErrorMessage(policy.message);

		if (m_IsNewlyCreatedItem)
		{
			m_IsRollbackTask = true;
			m_workshopAction = WorkshopManageAction::Delete;

			ResetUpdateProgress();
			Start();

			return;
		}
	}	

	PrintMessage("");

	ResetUpdateProgress();
	NotifyFinished(policy.success);
}

void WorkshopManageTask::OnWorkshopItemDeleted(DeleteItemResult_t* result, bool bIOFailure)
{
	PrintMessage("");

	if (bIOFailure)
	{
		if(!m_IsRollbackTask)
			PrintMessage(LOC_STEAM_TEMPORARY_FAILURE);

		NotifyFinished(false);
		return;
	}

	//reset state so this task instance can be reused later
	ResetUpdateProgress();

	auto policy = InterpretSteamResult(result->m_eResult);

	if (!m_IsRollbackTask)
	{
		if (policy.success)
		{
			SuccessMessage(LOC_DELETE_SUCCESS + to_string(m_workshopItem.GetItemId()));
			PrintMessage("");
		}
		else
		{
			ErrorMessage(policy.message);
			PrintMessage("");
		}

		NotifyFinished(policy.success);
	}
	else
	{
		NotifyFinished(false); //reverse delete indicates that the preceding create operation is unsuccessful
	}
}

SteamResultPolicy WorkshopManageTask::InterpretSteamResult(EResult steamResult)
{
	switch (steamResult)
	{
	case k_EResultOK:
		return { true, false, LOC_OPERATION_SUCCESS };

	case k_EResultFail:
		return { false, true, LOC_OPERATION_FAIL };

	case k_EResultTimeout:
		return { false, false, LOC_FAIL_TIMEOUT };

	case k_EResultServiceUnavailable:
	case k_EResultBusy:
	case k_EResultTryAnotherCM:
		return { false, false, LOC_STEAM_TEMPORARY_FAILURE };

	case k_EResultNotLoggedOn:
	case k_EResultAccessDenied:
	case k_EResultInsufficientPrivilege:
		return { false, true, LOC_STEAM_AUTH_FAILURE };

	case k_EResultRateLimitExceeded:
		return { false, true, LOC_TOO_MANY_REQUESTS };

	case k_EResultLimitExceeded:
		return { false, true, LOC_FAIL_SIZE_LIMIT };

	case k_EResultInvalidParam:
	case k_EResultFileNotFound:
	case k_EResultInvalidName:
		return { false, true, LOC_INVALID_CONTENT };

	default:
		return { false, true, LOC_STEAM_UNKNOWN_ERROR };
	}
}

void WorkshopManageTask::ResetUpdateProgress()
{
	m_bIsUpdateInProgress = false;
	m_lastUploadProcessed = 0;
	m_lastUploadTotal = 0;

	m_itemImagePreviewIndices.clear();
	m_itemVideoPreviewIndices.clear();
}

void WorkshopManageTask::QueryWorkshopItemDetails(PublishedFileId_t itemId)
{
	PrintMessage(LOC_VALIDATING_ITEM_ID + to_string(itemId));

	UGCQueryHandle_t handle = SteamUGC()->CreateQueryUGCDetailsRequest(&itemId, 1);
	SteamUGC()->SetReturnAdditionalPreviews(handle, true); //item previews info won't be loaded without this
	SteamAPICall_t queryCall = SteamUGC()->SendQueryUGCRequest(handle);

	m_SteamItemIdQueryCall.Set(queryCall, this, &WorkshopManageTask::OnWorkshopItemDetailsQueryCompleted);
}

void WorkshopManageTask::OnWorkshopItemDetailsQueryCompleted(SteamUGCQueryCompleted_t* result,	bool bIOFailure)
{
	bool bSuccess = true;
	string userMessage;

	ISteamUGC* ugc = SteamUGC();
	SteamUGCDetails_t ugcDetails;
	PublishedFileId_t itemId = m_workshopItem.GetItemId();

	ClearLine();

	auto policy = InterpretSteamResult(result->m_eResult);

	if (!policy.success)
		userMessage = policy.message;

	if (bIOFailure)
	{
		bSuccess = false;
		userMessage = LOC_FAILED_TO_VALIDATE_ITEM;
	}
	else if (result->m_unNumResultsReturned != 1) //only one item must be receieved
	{
		bSuccess = false;
		userMessage = LOC_IIV_ITEM_DOES_NOT_EXIST;
	} else if (!ugc->GetQueryUGCResult(result->m_handle, 0, &ugcDetails))
	{
		bSuccess = false;
		userMessage = LOC_IIV_ITEM_DETAILS_FAIL;
	}
	else if (ugcDetails.m_nPublishedFileId != itemId)
	{
		bSuccess = false;
		userMessage = LOC_FAILED_TO_VALIDATE_ITEM;
	}
	else
	{
		CSteamID owner(ugcDetails.m_ulSteamIDOwner);
		AccountID_t ownerAccountId = owner.GetAccountID();
		AccountID_t currentAccountId = SteamUser()->GetSteamID().GetAccountID();

		if (ownerAccountId == 0)
		{
			bSuccess = false;
			userMessage = LOC_FAILED_TO_VALIDATE_ITEM;
		} else if (ownerAccountId != currentAccountId) //item owners must match
		{
			bSuccess = false;
			userMessage = LOC_IIV_WRONG_USER;
		}
	}

	if (bSuccess)
	{
		uint32 itemPreviewCount = ugc->GetQueryUGCNumAdditionalPreviews(result->m_handle, 0);

		//we need to memorize which index is a screenshot and which is a video because they're all "previews" in Steam terms
		for (uint32 i = 0; i < itemPreviewCount; ++i)
		{
			char previewUrl[1024];
			char originalFileName[1024];
			EItemPreviewType type;

			if (ugc->GetQueryUGCAdditionalPreview(
				result->m_handle,
				0,
				i,
				previewUrl,
				sizeof(previewUrl),
				originalFileName,
				sizeof(originalFileName),
				&type))
			{
				if (type == k_EItemPreviewType_Image)
				{
					m_itemImagePreviewIndices.emplace_back(static_cast<size_t>(i));
				}
				else if (type == k_EItemPreviewType_YouTubeVideo)
				{
					m_itemVideoPreviewIndices.emplace_back(static_cast<size_t>(i));
				}
			}
		}

		ContinueAfterValidation();
	}
	else
	{
		PrintMessage("");
		ErrorMessage(userMessage);

		SetCursorVisible(true);
		m_bIsUpdateInProgress = false;
		NotifyFinished(false);
	}

	ugc->ReleaseQueryUGCRequest(result->m_handle);
}
