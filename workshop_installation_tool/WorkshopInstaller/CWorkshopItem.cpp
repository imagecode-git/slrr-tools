#include "CWorkshopItem.h"
#include "Main.h"
#include "UIState.h"

CWorkshopItem::CWorkshopItem()
{
	m_pchTitle = NULL;
	m_pchDescription = NULL;
	
	m_pchTitle_broken = NULL;
	m_pchDescription_broken = NULL;
	
	m_LoadState = WorkshopItemLoadState::Undefined;

	SetItemId(0);
	SetAuthorID(0);
	SetSize(0);
	SetTitle(LOC_DEFAULT_ITEM_TITLE);
	SetDescription(LOC_DEFAULT_ITEM_DESC);

#ifdef USE_STEAM_INSTALL_INFO
	SetSizeOnDisk(0);
	SetTimeStamp(0);
#endif

	m_u64RequestTime = 0;
	m_n32TimeUpdated = 0;

	m_bIsInstalled = false;
	m_bIsBroken = false;
	m_bIsSubscribed = false;
	m_bIsDownloaded = false;
	m_bIsCompatible = false;

	m_bDownloadInProgress = false;
}

CWorkshopItem::~CWorkshopItem()
{
	delete[] m_pchTitle;
	delete[] m_pchTitle_broken;

	delete[] m_pchDescription;
	delete[] m_pchDescription_broken;
}

void CWorkshopItem::SetItemId(PublishedFileId_t id)
{
	m_publishedFileId = id;
}

void CWorkshopItem::SetTitle(char* c)
{
	if (!c)
		return;

	delete[] m_pchTitle;
	delete[] m_pchTitle_broken;

	m_pchTitle = CopyString(c);

	char buffer[MAX_TITLE];
	buffer[0] = '\0';

	strcat_s(buffer, MAX_TITLE, LOC_PREFIX_BROKEN_INSTALL);
	strcat_s(buffer, MAX_TITLE, c);

	m_pchTitle_broken = CopyString(buffer);
}

void CWorkshopItem::SetDescription(const char* c)
{
	if (!c)
		return;

	delete[] m_pchDescription;
	delete[] m_pchDescription_broken;

	m_pchDescription = CopyString(c);
	m_pchDescription_broken = CopyString(LOC_DESC_BROKEN_INSTALL);
}

void CWorkshopItem::SetAuthorID(uint64 id)
{
	m_u64Author = id;
}

void CWorkshopItem::SetSize(int32 size)
{
	m_n32Size = size;
}

#ifdef USE_STEAM_INSTALL_INFO
void CWorkshopItem::SetSizeOnDisk(uint64 size)
{
	m_u64SizeOnDisk = size;
}

void CWorkshopItem::SetTimeStamp(int32 time)
{
	m_n32TimeStamp = time;
}
#endif

PublishedFileId_t CWorkshopItem::GetItemId()
{
	return m_publishedFileId;
}

char* CWorkshopItem::GetTitle()
{
	if (m_bIsBroken)
		return m_pchTitle_broken;

	return m_pchTitle;
}

char* CWorkshopItem::GetDescription()
{
	if (m_bIsBroken)
		return m_pchDescription_broken;

	return m_pchDescription;
}

uint64 CWorkshopItem::GetAuthorID()
{
	return m_u64Author;
}

int CWorkshopItem::GetSize()
{
	return m_n32Size;
}

double CWorkshopItem::GetSizeInMegabytes()
{
	return (m_n32Size/1024.0f)/1024.0f;
}

int32 CWorkshopItem::GetTimeUpdated()
{
	return m_n32TimeUpdated;
}

CWorkshopItem::WorkshopItemLoadState CWorkshopItem::GetLoadState()
{
	return m_LoadState;
}

bool CWorkshopItem::IsLoaded()
{
	return (m_LoadState == WorkshopItemLoadState::Completed);
}

bool CWorkshopItem::IsUpToDate()
{
	uint64 itemId = GetItemId();
	uint32 state = SteamUGC()->GetItemState(itemId);

	//if Steam itself says it needs update, trust it
	if (state & k_EItemStateNeedsUpdate)
		return false;

	//if not subscribed, Steam cannot update it anyway
	if (!m_bIsSubscribed)
		return true;

	//no local timestamp, cannot compare, consider up to date
	if (GetTimeStamp() == 0)
		return true;

	if (GetTimeStamp() < GetTimeUpdated()) //heuristic check, comparing server time to local install time
	{
		DebugLog("itemId " + itemId + " needs update");
		DebugLog("GetTimeStamp(): " + TimeToString(GetTimeStamp()));
		DebugLog("GetTimeUpdated(): " + TimeToString(GetTimeUpdated()));
		DebugLog("");

		m_bIsDownloaded = false;
		return false;
	}

	return true;
}

#ifdef USE_STEAM_INSTALL_INFO
uint64 CWorkshopItem::GetSizeOnDisk()
{
	return m_u64SizeOnDisk;
}

int32 CWorkshopItem::GetTimeStamp()
{
	return m_n32TimeStamp;
}

bool CWorkshopItem::TryGetInstallDir(char* outBuffer, size_t bufferSize)
{
	if (!outBuffer || bufferSize == 0)
		return false;

	uint64 sizeOnDisk = 0;
	uint32 timeStamp = 0;

	bool bInstallInfoOk = SteamUGC()->GetItemInstallInfo(
		GetItemId(),
		&sizeOnDisk,
		outBuffer,
		(uint32)bufferSize,
		&timeStamp
	);

	if (!bInstallInfoOk || outBuffer[0] == '\0')
		return false;

	return true;
}

bool CWorkshopItem::UpdateUGCInstallInfo()
{
	char installDir[MAX_PATH] = {};
	uint64 sizeOnDisk = 0;
	uint32 timeStamp = 0;

	bool bResult = SteamUGC()->GetItemInstallInfo(
		GetItemId(),
		&sizeOnDisk,
		installDir,
		sizeof(installDir),
		&timeStamp
	);

	if (bResult)
	{
		SetSizeOnDisk(sizeOnDisk);
		SetTimeStamp((int32)timeStamp);
	}

	return bResult;
}
#endif

CWorkshopItem* CWorkshopItem::Clone()
{
	CWorkshopItem* clone = new CWorkshopItem();

	clone->SetItemId(GetItemId());
	clone->SetAuthorID(GetAuthorID());
	clone->SetTitle(GetTitle());
	clone->SetDescription(GetDescription());
	clone->SetSize(GetSize());

	clone->m_bIsInstalled = m_bIsInstalled;
	clone->m_bIsSubscribed = m_bIsSubscribed;

	return clone;
}

inline char* CWorkshopItem::CopyString(const char* src)
{
	size_t len = strlen(src) + 1;
	char* ptr = new char[len];
	memcpy(ptr, src, len);

	return ptr;
}

void CWorkshopItem::OnUGCDetails(SteamUGCRequestUGCDetailsResult_t* pResult, bool bIOFailure)
{
	if (bIOFailure || !pResult)
	{
		m_LoadState = WorkshopItemLoadState::Failed;
		return;
	}

	AppId_t appId = pResult->m_details.m_nCreatorAppID;
	if (MAIN->GetSteamAppId() == appId)
		m_bIsCompatible = true;
	else
		m_bIsCompatible = false;

	uint64 itemId = pResult->m_details.m_nPublishedFileId;
	DebugLog("OnUGCDetails(" + itemId + ")");
	DebugLog("   m_nCreatorAppID: " + appId);

	if (!pResult->m_details.m_bBanned && m_bIsCompatible)
	{
		SetTitle(pResult->m_details.m_rgchTitle);
		SetDescription(pResult->m_details.m_rgchDescription);
		SetItemId(itemId);
		SetAuthorID(pResult->m_details.m_ulSteamIDOwner);
		SetSize(pResult->m_details.m_nFileSize);

		m_n32TimeUpdated = pResult->m_details.m_rtimeUpdated;

		if (!IsUpToDate() || !m_bIsDownloaded)
		{
			SteamUGC()->DownloadItem(itemId, false);
			m_bDownloadInProgress = true;

			m_LoadState = WorkshopItemLoadState::Requesting;
		}
		else
			m_LoadState = WorkshopItemLoadState::Completed;

		UIState::NeedsFullUpdate = true;
	}
	else
	{
		DebugLog("CWorkshopItem::OnUGCDetails()");

		if(m_bIsCompatible)
			DebugLog("   " + itemId + " is incompatible with Street Legal! (wrong m_nCreatorAppID)");

		if(pResult->m_details.m_bBanned)
			DebugLog("   " + itemId + " is banned!");

		m_LoadState = WorkshopItemLoadState::Failed;
		return;
	}
}

bool CWorkshopItem::ForceDownload()
{
	bool bResult = SteamUGC()->DownloadItem(GetItemId(), false);
	m_bDownloadInProgress = true;
	m_bIsDownloaded = false;

	UIState::NeedsFullUpdate = true;

	return bResult;
}