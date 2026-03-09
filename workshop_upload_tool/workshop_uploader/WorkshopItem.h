#pragma once

#include <string>
#include <vector>
#include <algorithm> //sort
#include "Environment.h"
#include "Helpers.h"
#include "WorkshopItemValidation.h"

#include "steam/steam_api.h"
#pragma comment(lib, "steam/steam_api")

#define ITEM_DEFAULT_TITLE "Workshop item"
#define ITEM_DEFAULT_DESCRIPTION "This item has been uploaded with the ImageCode workshop uploader."
#define ITEM_DEFAULT_CONTENT_DIR "workshop_content"
#define ITEM_DEFAULT_PREVIEW_IMAGE "workshop_preview.jpg"
#define ITEM_DEFAULT_SCREENSHOT_DIR "workshop_screenshots"
#define ITEM_DEFAULT_UPDATE_COMMENT ""

#define ITEM_COMMENT_MODIFIED_BY "Modified by"

#define DEF_ITEM_LANGUAGE "english"
#define DEF_ITEM_METADATA "uploader=imagecode"

namespace WorkshopItemLimits
{
	constexpr size_t kMaxTitleBytes = 128; //Steam constraints, max 128-byte title
	constexpr size_t kMaxDescriptionBytes = 8192; //max 8kb description
	constexpr size_t kWin32MaxPath = MAX_PATH;
	constexpr size_t kMaxScreenshots = 50;
	constexpr size_t kMaxPossiblePreviews = 100; //must be always intentionally greater than kMaxScreenshots
}

static const EnumStringPair<ERemoteStoragePublishedFileVisibility> WorkshopItemVisibilityModesList[] =
{
	{ k_ERemoteStoragePublishedFileVisibilityPublic,		"public" },
	{ k_ERemoteStoragePublishedFileVisibilityFriendsOnly,	"friends" },
	{ k_ERemoteStoragePublishedFileVisibilityPrivate,		"private" },
	{ k_ERemoteStoragePublishedFileVisibilityUnlisted,		"unlisted" }
};

class WorkshopItem
{
public:
	WorkshopItem();
	~WorkshopItem();

	PublishedFileId_t GetItemId();
	ERemoteStoragePublishedFileVisibility GetVisibility();
	std::string GetVisibilityString() const;

	//these are immutable and return references, not copies
	const std::string& GetTitle();
	const std::string& GetDescription();
	const std::string& GetContentDir();
	const std::string& GetPreviewImagePath();
	const std::string& GetUpdateComment();
	const std::vector<std::string>& GetCategories();
	const std::vector<std::string>& GetVideoUrls();
	const std::vector<std::string>& GetScreenshots();

	//we accept weak setters that validate nothing, but eventually we call AreInputParamsValid()
	void SetItemId(PublishedFileId_t newItemId);
	void SetTitle(std::string newTitle);
	void SetDescription(std::string newDescription);
	void SetContentDir(std::string newDir);
	void SetPreviewImagePath(std::string newPath);
	void SetUpdateComment(std::string comment);
	void SetCategories(std::vector<std::string>& newCategories);
	void SetScreenshots(std::vector<std::string>& newScreenshots);
	void SetVideoUrls(std::vector<std::string>& newVideoUrls);
	void SetVisibility(ERemoteStoragePublishedFileVisibility newVisibility);

	bool LoadScreenshotsFromDirectory(const std::string& directory);
	
	//validation
	bool IsSupportedImage(const std::string& filePath) const;
	bool HasTitle() const;
	bool HasDescription() const;
	bool HasVisibility() const;
	bool HasCategories() const;
	bool HasScreenshots() const;
	bool HasVideoUrls() const;
	bool HasValidPreviewImage() const;
	bool HasValidContentDir() const;
	void ValidateForSubmission(IWorkshopValidationPolicy& policy);

	void Reset();
	void DebugDumpItemInfo();

private:
	PublishedFileId_t m_itemId;
	std::string m_itemTitle;
	std::string m_itemDescription;
	std::string m_itemContentDir;
	std::string m_itemPreviewImagePath;
	std::string m_itemUpdateComment;
	
	ERemoteStoragePublishedFileVisibility m_itemVisibility;

	std::vector<std::string> m_itemCategories;
	std::vector<std::string> m_itemScreenshots;
	std::vector<std::string> m_itemVideoUrls;
};