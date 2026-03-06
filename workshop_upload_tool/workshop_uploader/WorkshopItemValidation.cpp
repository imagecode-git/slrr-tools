#include "stdafx.h"
#include "WorkshopItemValidation.h"
#include "WorkshopItem.h"

using namespace std;
using namespace WorkshopItemLimits;
using MessageType = WorkshopItemValidationMessage::Type;

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
    messages.push_back({ MessageType::Warning, string(LOC_IV_CONTENT_DIR_EMPTY) + path });
}

void BaseValidationPolicy::OnContentDirEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Warning, LOC_IV_CONTENT_DIR_EMPTY });
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
    BaseValidationPolicy::OnTitleEmpty(item);
    item.SetTitle(ITEM_DEFAULT_TITLE);
}

void AutoCorrectValidationPolicy::OnTitleTooLong(WorkshopItem& item)
{
    BaseValidationPolicy::OnTitleTooLong(item);
    item.SetTitle(item.GetTitle().substr(0, kMaxTitleBytes));
}

void AutoCorrectValidationPolicy::OnDescriptionEmpty(WorkshopItem& item)
{
    BaseValidationPolicy::OnDescriptionEmpty(item);
    item.SetDescription(ITEM_DEFAULT_DESCRIPTION);
}

void AutoCorrectValidationPolicy::OnDescriptionTooLong(WorkshopItem& item)
{
    BaseValidationPolicy::OnDescriptionTooLong(item);
    item.SetDescription(item.GetDescription().substr(0, kMaxDescriptionBytes));
}

void AutoCorrectValidationPolicy::OnVisibilityNotSet(WorkshopItem& item)
{
    BaseValidationPolicy::OnVisibilityNotSet(item);
    item.SetVisibility(k_ERemoteStoragePublishedFileVisibilityPrivate); //default: private
}

void AutoCorrectValidationPolicy::OnContentDirMissing(WorkshopItem& item, const string& path)
{
    BaseValidationPolicy::OnContentDirMissing(item, path);
    item.SetContentDir(ITEM_DEFAULT_CONTENT_DIR);
}

void AutoCorrectValidationPolicy::OnPreviewImageEmpty(WorkshopItem& item)
{
    BaseValidationPolicy::OnPreviewImageEmpty(item);
    item.SetPreviewImagePath(ITEM_DEFAULT_PREVIEW_IMAGE);
}

void AutoCorrectValidationPolicy::OnPreviewImageMissing(WorkshopItem& item, const string& path)
{
    BaseValidationPolicy::OnPreviewImageMissing(item, path);
    item.SetPreviewImagePath(ITEM_DEFAULT_PREVIEW_IMAGE);
}

void AutoCorrectValidationPolicy::OnPreviewImageInvalid(WorkshopItem& item, const string& path)
{
    BaseValidationPolicy::OnPreviewImageInvalid(item, path);
    item.SetPreviewImagePath(ITEM_DEFAULT_PREVIEW_IMAGE);
}

void AutoCorrectValidationPolicy::OnScreenshotsEmpty(WorkshopItem& item)
{
    BaseValidationPolicy::OnScreenshotsEmpty(item);
    item.LoadScreenshotsFromDirectory(ITEM_DEFAULT_SCREENSHOT_DIR);
}

void AutoCorrectValidationPolicy::OnUpdateCommentEmpty(WorkshopItem& item)
{
    BaseValidationPolicy::OnUpdateCommentEmpty(item);

    string strUpdateComment = ITEM_DEFAULT_UPDATE_COMMENT;

    if (SteamUser())
    {
        CSteamID steamUserId = SteamUser()->GetSteamID();
        uint64 steamId64 = steamUserId.ConvertToUint64();

        strUpdateComment = ITEM_COMMENT_MODIFIED_BY + string(" ") + to_string(steamId64);
    }

    item.SetUpdateComment(strUpdateComment);
}

//policy for mode create, does not require itemId verification and initial update comment
void CreateValidationPolicy::OnItemIdInvalid(WorkshopItem& item, const string& strItemId) {}
void CreateValidationPolicy::OnUpdateCommentEmpty(WorkshopItem& item) {}

void CreateValidationPolicy::OnContentDirMissing(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Error, string(LOC_IV_NO_CONTENT_DIR) + "\n" + path });
}

void CreateValidationPolicy::OnContentDirHasNoFiles(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Error, string(LOC_IV_CONTENT_DIR_EMPTY) + path });
}

void CreateValidationPolicy::OnContentDirEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Error, LOC_IV_CONTENT_DIR_EMPTY });
}

void CreateValidationPolicy::OnPreviewImageEmpty(WorkshopItem& item)
{
    messages.push_back({ MessageType::Error, LOC_IV_NO_PREVIEW_IMG_PATH });
}

void CreateValidationPolicy::OnPreviewImageMissing(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Error, string(LOC_IV_FILE_NOT_FOUND) + path });
}

void CreateValidationPolicy::OnPreviewImageInvalid(WorkshopItem& item, const string& path)
{
    messages.push_back({ MessageType::Error, string(LOC_IV_IMAGE_FORMAT_ERROR) + path });
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