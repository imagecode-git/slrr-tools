#pragma once

#include <functional> //std::function

#include "WorkshopItem.h"

#include "steam/steam_api.h"
#pragma comment(lib, "steam/steam_api")

struct SteamResultPolicy
{
    bool success;
    bool fatal; //warning message if false, error message if true
    const char* message;
};

enum class WorkshopManageAction
{
    Create = 0,
    Modify = 1,
    Delete = 2,
    Info = 3,
    Unknown
};

enum class WorkshopManageResult
{
    Continue, //can proceed to the next stage
    Retry, //interrupted
    Abort, //ended by user
    Error
};

static const EnumStringPair<WorkshopManageAction> WorkshopUploaderActionList[] =
{
    { WorkshopManageAction::Create,  "create" },
    { WorkshopManageAction::Modify,  "modify" },
    { WorkshopManageAction::Delete,  "delete" },
    { WorkshopManageAction::Info,    "info" }
};

class WorkshopManageTask
{
public:
    WorkshopManageTask(uint32 steamAppId, WorkshopManageAction action, WorkshopItem item, std::function<void(WorkshopManageTask*, bool)> onFinished)
        : m_steamAppId(steamAppId),
          m_workshopAction(action),
          m_workshopItem(std::move(item)),
          m_onFinished(std::move(onFinished))
    {}

    void Start(); //calls SteamUGC(), binds callback, etc.
    void ContinueAfterValidation();
    void Tick();

    WorkshopItem& GetWorkshopItem();
    WorkshopManageAction GetManageAction() const;

private:
    void NotifyFinished(bool bSuccess); //tells MainModule that this item manage task is complete
    void BuildUGCUpdateRequest();

    //UGC callbacks
    void OnWorkshopItemCreated(CreateItemResult_t* result, bool bIOFailure);
    void OnWorkshopItemSubmitted(SubmitItemUpdateResult_t* result, bool bIOFailure); //submit, modify, update
    void OnWorkshopItemDeleted(DeleteItemResult_t* result, bool bIOFailure);
    void OnWorkshopItemDetailsQueryCompleted(SteamUGCQueryCompleted_t* result, bool bIOFailure);

    void ResetUpdateProgress();

    SteamResultPolicy InterpretSteamResult(EResult steamResult);
    void QueryWorkshopItemDetails(PublishedFileId_t itemId);

    std::function<void(WorkshopManageTask*, bool)> m_onFinished;

    uint32 m_steamAppId;
    WorkshopItem m_workshopItem;
    WorkshopManageAction m_workshopAction = WorkshopManageAction::Unknown;
    bool m_IsNewlyCreatedItem = false; //item created, but later failed to update, this indicates that rollback delete is required
    bool m_IsRollbackTask = false;

    //these vectors are required to properly update item previews in BuildUGCUpdateRequest()
    std::vector<size_t> m_itemImagePreviewIndices;
    std::vector<size_t> m_itemVideoPreviewIndices;

    UGCUpdateHandle_t m_SteamUGCUpdateHandle = k_UGCUpdateHandleInvalid;
    SteamAPICall_t m_SubmitCallResult;

    //progress status
    bool m_bIsUpdateInProgress = false; //allows Tick() to post status
    uint64 m_lastUploadProcessed = 0;
    uint64 m_lastUploadTotal = 0;

    CCallResult<WorkshopManageTask, CreateItemResult_t> m_SteamCreateCall;
    CCallResult<WorkshopManageTask, SubmitItemUpdateResult_t> m_SteamSubmitCall;
    CCallResult<WorkshopManageTask, DeleteItemResult_t> m_SteamDeleteCall;
    CCallResult<WorkshopManageTask, SteamUGCQueryCompleted_t> m_SteamItemIdQueryCall;
};
