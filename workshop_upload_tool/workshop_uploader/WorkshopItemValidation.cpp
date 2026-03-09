#include "stdafx.h"
#include "WorkshopItemValidation.h"
#include "WorkshopItem.h"

using namespace std;
using namespace WorkshopItemLimits;
using MessageType = WorkshopItemValidationMessage::Type;

static void NotifyUser(const string& param, const vector<string>& values)
{
    PrintMessage("");
    WarningMessage(LOC_AUTO_CORRECT + param);

    for (const auto& value : values)
        PrintMessage(value);
}

static void NotifyUser(const string& param, const string& value)
{
    NotifyUser(param, vector<string>{ value });
}

bool BaseValidationPolicy::HasWarnings() const
{
    return any_of(messages.begin(), messages.end(),
        [](const WorkshopItemValidationMessage& m)
        {
            return m.type == WorkshopItemValidationMessage::Type::Warning;
        });
}

bool BaseValidationPolicy::HasErrors() const
{
    return any_of(messages.begin(), messages.end(),
        [](const WorkshopItemValidationMessage& m)
        {
            return m.type == WorkshopItemValidationMessage::Type::Error;
        });
}

void BaseValidationPolicy::OnItemIdInvalid(WorkshopItem& item, const string& strItemId)
{
    messages.push_back({ MessageType::Error, string(LOC_IV_INVALID_ITEM_ID) + strItemId });
}

void BaseValidationPolicy::OnTitleEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_NO_TITLE });
}

void BaseValidationPolicy::OnTitleTooLong(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_TITLE_TOO_LONG });
}

void BaseValidationPolicy::OnDescriptionEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_NO_DESC });
}

void BaseValidationPolicy::OnDescriptionTooLong(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_DESC_TOO_LONG });
}

void BaseValidationPolicy::OnVisibilityNotSet(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_NO_VISIBILITY });
}

void BaseValidationPolicy::OnContentDirMissing(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Warning, string(LOC_IV_NO_CONTENT_DIR) + "\n" + path});
}

void BaseValidationPolicy::OnContentDirHasNoFiles(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Warning, string(LOC_IV_CONTENT_DIR_EMPTY) + "\n" + path});
}

void BaseValidationPolicy::OnContentDirEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_CONTENT_DIR_ERROR });
}

void BaseValidationPolicy::OnPreviewImageEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_NO_PREVIEW_IMG_PATH });
}

void BaseValidationPolicy::OnPreviewImageMissing(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Warning, string(LOC_IV_FILE_NOT_FOUND) + path});
}

void BaseValidationPolicy::OnPreviewImageInvalid(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Warning, string(LOC_IV_IMAGE_FORMAT_ERROR) + path });
}

void BaseValidationPolicy::OnCategoriesEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_NO_CATEGORIES});
}

void BaseValidationPolicy::OnCategoryEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_NO_CAT_EMPTY_ERR });
}

void BaseValidationPolicy::OnScreenshotsEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_SCREENSHOT_PATH_ERR });
}

void BaseValidationPolicy::OnTooManyScreenshots(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_TOO_MANY_SCREENSHOTS });
}

void BaseValidationPolicy::OnScreenshotEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_NO_SHOTS_EMPTY_ERR });
}

void BaseValidationPolicy::OnScreenshotMissing(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Warning, string(LOC_IV_FILE_NOT_FOUND) + path });
}

void BaseValidationPolicy::OnScreenshotInvalid(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Warning, string(LOC_IV_IMAGE_FORMAT_ERROR) + path });
}

void BaseValidationPolicy::OnVideoUrlsEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_VIDEO_URL_NOT_DEF });
}

void BaseValidationPolicy::OnVideoUrlEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_VIDEO_URL_EMPTY });
}

void BaseValidationPolicy::OnUpdateCommentEmpty(WorkshopItem& item)
{
    //do nothing, empty comment is mostly fine
}

//policy that will auto-correct input params when needed
void AutoCorrectValidationPolicy::OnTitleEmpty(WorkshopItem& item)
{
    item.SetTitle(ITEM_DEFAULT_TITLE);

    NotifyUser(LOC_WII_TITLE, item.GetTitle());
}

void AutoCorrectValidationPolicy::OnDescriptionEmpty(WorkshopItem& item)
{
    item.SetDescription(ITEM_DEFAULT_DESCRIPTION);

    NotifyUser(LOC_WII_DESCRIPTION, item.GetDescription());
}

void AutoCorrectValidationPolicy::OnVisibilityNotSet(WorkshopItem& item)
{
    ERemoteStoragePublishedFileVisibility defVisibility = k_ERemoteStoragePublishedFileVisibilityPrivate;
    item.SetVisibility(defVisibility); //default: private

    NotifyUser(LOC_WII_VISIBILITY, item.GetVisibilityString());
}

void AutoCorrectValidationPolicy::OnContentDirEmpty(WorkshopItem& item)
{
    string contentDir = ResolveRelPath(ITEM_DEFAULT_CONTENT_DIR);
    item.SetContentDir(contentDir);

    NotifyUser(LOC_WII_CONTENT_DIR, item.GetContentDir());
}

void AutoCorrectValidationPolicy::OnPreviewImageEmpty(WorkshopItem& item)
{
    string previewImagePath = ResolveRelPath(ITEM_DEFAULT_PREVIEW_IMAGE);
    item.SetPreviewImagePath(previewImagePath);

    NotifyUser(LOC_WII_PREVIEW_IMG, item.GetPreviewImagePath());
}

void AutoCorrectValidationPolicy::OnScreenshotsEmpty(WorkshopItem& item)
{
    item.LoadScreenshotsFromDirectory(ITEM_DEFAULT_SCREENSHOT_DIR);

    NotifyUser(LOC_WII_SCREENSHOTS, item.GetScreenshots());
}

void AutoCorrectValidationPolicy::OnUpdateCommentEmpty(WorkshopItem& item)
{
    string strUpdateComment = ITEM_DEFAULT_UPDATE_COMMENT;

    if (SteamUser())
    {
        CSteamID steamUserId = SteamUser()->GetSteamID();
        uint64 steamId64 = steamUserId.ConvertToUint64();

        strUpdateComment = ITEM_COMMENT_MODIFIED_BY + string(" ") + to_string(steamId64);
    }

    item.SetUpdateComment(strUpdateComment);

    NotifyUser(LOC_WII_UPDATE_COMMENT, item.GetUpdateComment());
}

//policy for mode create, does not require itemId verification and initial update comment
void CreateValidationPolicy::OnItemIdInvalid(WorkshopItem& item, const string& strItemId) {}

void CreateValidationPolicy::OnTitleEmpty(WorkshopItem& item)
{
    if (!UploaderConfig::Instance().bCreateDefaults)
    {
        BaseValidationPolicy::OnTitleEmpty(item);
        return;
    }

    AutoCorrectValidationPolicy::OnTitleEmpty(item);
}

void CreateValidationPolicy::OnDescriptionEmpty(WorkshopItem& item)
{
    if (!UploaderConfig::Instance().bCreateDefaults)
    {
        BaseValidationPolicy::OnDescriptionEmpty(item);
        return;
    }

    AutoCorrectValidationPolicy::OnDescriptionEmpty(item);
}

void CreateValidationPolicy::OnVisibilityNotSet(WorkshopItem& item)
{
    if (!UploaderConfig::Instance().bCreateDefaults)
    {
        BaseValidationPolicy::OnVisibilityNotSet(item);
        return;
    }

    AutoCorrectValidationPolicy::OnVisibilityNotSet(item);
}

void CreateValidationPolicy::OnContentDirMissing(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Error, string(LOC_IV_NO_CONTENT_DIR) + "\n" + path });
}

void CreateValidationPolicy::OnContentDirHasNoFiles(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Error, string(LOC_IV_CONTENT_DIR_EMPTY) + "\n" + path });
}

void CreateValidationPolicy::OnContentDirEmpty(WorkshopItem& item)
{
    if (!UploaderConfig::Instance().bCreateDefaults)
    {
        messages.push_back({ MessageType::Error, LOC_IV_CONTENT_DIR_ERROR });
        return;
    }

    AutoCorrectValidationPolicy::OnContentDirEmpty(item);
}

void CreateValidationPolicy::OnPreviewImageEmpty(WorkshopItem& item)
{
    if (!UploaderConfig::Instance().bCreateDefaults)
    {
        messages.push_back({ MessageType::Error, LOC_IV_NO_PREVIEW_IMG_PATH });
        return;
    }

    AutoCorrectValidationPolicy::OnPreviewImageEmpty(item);
}

void CreateValidationPolicy::OnPreviewImageMissing(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Error, string(LOC_IV_FILE_NOT_FOUND) + path });
}

void CreateValidationPolicy::OnPreviewImageInvalid(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Error, string(LOC_IV_IMAGE_FORMAT_ERROR) + path });
}

void CreateValidationPolicy::OnScreenshotsEmpty(WorkshopItem& item)
{
    if (!UploaderConfig::Instance().bCreateDefaults)
    {
        BaseValidationPolicy::OnScreenshotsEmpty(item);
        return;
    }

    AutoCorrectValidationPolicy::OnScreenshotsEmpty(item);
}

void CreateValidationPolicy::OnUpdateCommentEmpty(WorkshopItem& item)
{
    //no warnings or errors if initial release comment is empty
    if (UploaderConfig::Instance().bCreateDefaults)
    {
        AutoCorrectValidationPolicy::OnUpdateCommentEmpty(item);
    }
}

//policy for mode delete, it mostly does nothing as only itemId is required for delete
void DeleteValidationPolicy::OnTitleEmpty(WorkshopItem& item) {}
void DeleteValidationPolicy::OnTitleTooLong(WorkshopItem& item) {}
void DeleteValidationPolicy::OnDescriptionEmpty(WorkshopItem& item) {}
void DeleteValidationPolicy::OnDescriptionTooLong(WorkshopItem& item) {}
void DeleteValidationPolicy::OnVisibilityNotSet(WorkshopItem& item) {}
void DeleteValidationPolicy::OnContentDirMissing(WorkshopItem& item, const string& path) {}
void DeleteValidationPolicy::OnContentDirHasNoFiles(WorkshopItem& item, const string& path) {}
void DeleteValidationPolicy::OnContentDirEmpty(WorkshopItem& item) {}
void DeleteValidationPolicy::OnPreviewImageEmpty(WorkshopItem& item) {}
void DeleteValidationPolicy::OnPreviewImageMissing(WorkshopItem& item, const string& path) {}
void DeleteValidationPolicy::OnPreviewImageInvalid(WorkshopItem& item, const string& path) {}
void DeleteValidationPolicy::OnCategoriesEmpty(WorkshopItem& item) {}
void DeleteValidationPolicy::OnCategoryEmpty(WorkshopItem& item) {}
void DeleteValidationPolicy::OnScreenshotsEmpty(WorkshopItem& item) {}
void DeleteValidationPolicy::OnTooManyScreenshots(WorkshopItem& item) {}
void DeleteValidationPolicy::OnScreenshotEmpty(WorkshopItem& item) {}
void DeleteValidationPolicy::OnScreenshotMissing(WorkshopItem& item, const string& path) {}
void DeleteValidationPolicy::OnScreenshotInvalid(WorkshopItem& item, const string& path) {}
void DeleteValidationPolicy::OnVideoUrlsEmpty(WorkshopItem& item) {}
void DeleteValidationPolicy::OnVideoUrlEmpty(WorkshopItem& item) {}