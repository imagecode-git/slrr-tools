#pragma once

#include "steam/steam_api.h"
#include "Options.h"
#pragma comment(lib, "steam/steam_api")

class CWorkshopItem
{
public:
	CWorkshopItem();
	~CWorkshopItem();

	enum WorkshopItemLoadState
	{
		Undefined,
		Requesting,	//RequestUGCDetails sent
		Completed,	//metadata received
		Failed		//explicit failure or timeout
	};

	CCallResult<CWorkshopItem, SteamUGCRequestUGCDetailsResult_t> m_SteamCallResult;

	//set
	void SetItemId(PublishedFileId_t id);
	void SetTitle(char* c);
	void SetDescription(const char* c);
	void SetAuthorID(uint64 id);
	void SetSize(int32 size);

	void SetSizeOnDisk(uint64 size);
	void SetTimeStamp(int32 time);

	//get
	PublishedFileId_t GetItemId();
	char* GetTitle();
	char* GetDescription();
	uint64 GetAuthorID();
	int32 GetSize();
	double GetSizeInMegabytes();
	int32 GetTimeUpdated(); //server update time
	WorkshopItemLoadState GetLoadState();

	bool TryGetInstallDir(char* outBuffer, size_t bufferSize);

	bool IsLoaded();
	bool IsUpToDate();
	bool ForceDownload();

#ifdef USE_STEAM_INSTALL_INFO
	uint64 GetSizeOnDisk();
	int32 GetTimeStamp(); //local installation update time

	bool UpdateUGCInstallInfo();
#endif

	bool m_bIsInstalled;
	bool m_bIsBroken; //needs reinstall
	bool m_bIsSubscribed;
	bool m_bIsDownloaded; //if true, Steam UGC has fully cached this item
	bool m_bIsCompatible; //false if this item belongs to an incompatible Steam AppId (another game)

	uint64 m_u64RequestTime;

	WorkshopItemLoadState m_LoadState;

	CWorkshopItem* Clone();

	void OnUGCDetails(SteamUGCRequestUGCDetailsResult_t* pResult, bool bIOFailure);
	bool m_bDownloadInProgress;

private:
	char* m_pchTitle;
	char* m_pchDescription;
	uint64 m_u64Author;
	int32 m_n32Size;
	int32 m_n32TimeUpdated;

	char* m_pchTitle_broken;
	char* m_pchDescription_broken;

#ifdef USE_STEAM_INSTALL_INFO
	uint64 m_u64SizeOnDisk;
	int32 m_n32TimeStamp;
#endif
	
	PublishedFileId_t m_publishedFileId;

	//helpers
	inline char* CopyString(const char* src);
};
