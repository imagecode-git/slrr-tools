#include "stdafx.h"
#include "WorkshopItem.h"

static constexpr auto	kImageExtensionJpg = "jpg";
static constexpr auto	kImageExtensionJpeg = "jpeg";
static constexpr auto	kImageExtensionPng = "png";

using namespace std;

WorkshopItem::WorkshopItem()
{
	Reset();
}

WorkshopItem::~WorkshopItem()
{

}

PublishedFileId_t WorkshopItem::GetItemId()
{
	return m_itemId;
}

ERemoteStoragePublishedFileVisibility WorkshopItem::GetVisibility()
{
	return m_itemVisibility;
}

bool WorkshopItem::IsVisibilitySet()
{
	return (m_itemVisibility ==
		k_ERemoteStoragePublishedFileVisibilityPublic ||
		k_ERemoteStoragePublishedFileVisibilityFriendsOnly ||
		k_ERemoteStoragePublishedFileVisibilityPrivate ||
		k_ERemoteStoragePublishedFileVisibilityUnlisted);
}

bool WorkshopItem::IsSupportedImage(const string& filePath)
{
	if (filePath.empty())
		return false;

	size_t dotPos = filePath.find_last_of('.');
	if (dotPos == std::string::npos || dotPos + 1 >= filePath.size())
		return false;

	const char* fileExt = filePath.c_str() + dotPos + 1;

	return	_stricmp(fileExt, kImageExtensionJpg) == 0 ||
			_stricmp(fileExt, kImageExtensionJpeg) == 0 ||
			_stricmp(fileExt, kImageExtensionPng) == 0;
}

bool WorkshopItem::HasValidPreviewImage()
{
	const string& filePath = m_itemPreviewImagePath;

	return !filePath.empty() && FileExists(filePath) && IsSupportedImage(filePath);
}

bool WorkshopItem::HasValidContentDir()
{
	return !m_itemContentDir.empty() && DirectoryExists(m_itemContentDir) && DirectoryHasFiles(m_itemContentDir);
}

//utilizes IWorkshopValidationPolicy to revert values to defaults if they're missing or broken
void WorkshopItem::ValidateForSubmission(IWorkshopValidationPolicy& policy)
{
	const string& itemContentDir = GetContentDir();
	const string& itemPreviewImagePath = GetPreviewImagePath();

	const vector<string>& itemCategories = GetCategories();
	const vector<string>& itemScreenshots = GetScreenshots();
	const vector<string>& itemVideoUrls = GetVideoUrls();

	//itemId
	if (GetItemId() == 0)
	{
		string strItemId = to_string(GetItemId());
		policy.OnItemIdInvalid(*this, strItemId);
	}	

	//title
	if (GetTitle().empty())
		policy.OnTitleEmpty(*this);

	if (GetTitle().size() > WorkshopItemLimits::kMaxTitleBytes)
		policy.OnTitleTooLong(*this);

	//description
	if (GetDescription().empty())
		policy.OnDescriptionEmpty(*this);

	if (GetDescription().size() > WorkshopItemLimits::kMaxDescriptionBytes)
		policy.OnDescriptionTooLong(*this);

	//visibility
	if (!IsVisibilitySet())
		policy.OnVisibilityNotSet(*this);

	//content dir
	if (itemContentDir.empty())
	{
		policy.OnContentDirEmpty(*this);
	}
	else
	{
		if (!DirectoryExists(itemContentDir))
			policy.OnContentDirMissing(*this, itemContentDir);
		else
			if (!DirectoryHasFiles(itemContentDir))
				policy.OnContentDirHasNoFiles(*this, itemContentDir);
	}

	//preview image
	if (itemPreviewImagePath.empty())
	{
		policy.OnPreviewImageEmpty(*this);
	}
	else
	{
		if (!FileExists(GetPreviewImagePath()))
			policy.OnPreviewImageMissing(*this, itemPreviewImagePath);
		else
		{
			if (!IsSupportedImage(itemPreviewImagePath))
				policy.OnPreviewImageInvalid(*this, itemPreviewImagePath);
		}
	}

	//categories
	if (itemCategories.empty())
	{
		policy.OnCategoriesEmpty(*this);
	}
	else
	{
		for (const string& category : itemCategories)
		{
			if (category.empty())
				policy.OnCategoryEmpty(*this);
		}
	}

	//screenshots
	if (itemScreenshots.empty())
	{
		policy.OnScreenshotEmpty(*this);
	}
	else
	{
		if (itemScreenshots.size() >= WorkshopItemLimits::kMaxScreenshots)
			policy.OnTooManyScreenshots(*this);

		for (const string& screenshot : itemScreenshots)
		{
			if (!FileExists(screenshot))
			{
				policy.OnScreenshotMissing(*this, screenshot);
			}
			else
			{
				if (!IsSupportedImage(screenshot))
					policy.OnScreenshotInvalid(*this, screenshot);
			}
		}
	}
	//videos
	if (itemVideoUrls.empty())
	{
		policy.OnVideoUrlsEmpty(*this);
	}
	else
	{
		for (const string& videoUrl : itemVideoUrls)
		{
			if(videoUrl.empty())
				policy.OnVideoUrlEmpty(*this);
		}	
	}
}

bool WorkshopItem::LoadScreenshotsFromDirectory(const string& directory)
{
	m_itemScreenshots.clear();

	string normalizedPath = NormalizePath(directory); //normalize to fix edge cases with the .JPG or .jPG extensions
	if (!DirectoryExists(normalizedPath))
	{
		DebugLog("LoadScreenshotsFromDirectory: directory not found: " + normalizedPath);
		return false;
	}

	vector<string> allFiles;
	FindFiles(normalizedPath, "*.*", allFiles);

	//sort before filtering and limiting
	sort(allFiles.begin(), allFiles.end());

	for (const auto& filePath : allFiles)
	{
		if (m_itemScreenshots.size() >= WorkshopItemLimits::kMaxScreenshots)
			break;

		if (IsSupportedImage(filePath))
			m_itemScreenshots.emplace_back(filePath);
	}

	return !m_itemScreenshots.empty();
}

void WorkshopItem::SetItemId(PublishedFileId_t newItemId)
{
	m_itemId = newItemId;
}

void WorkshopItem::SetTitle(string newTitle)
{
	m_itemTitle = newTitle;
}

void WorkshopItem::SetDescription(string newDescription)
{
	m_itemDescription = newDescription;
}

void WorkshopItem::SetContentDir(string newDir)
{
	m_itemContentDir = newDir;
}

void WorkshopItem::SetPreviewImagePath(string newPath)
{
	m_itemPreviewImagePath = newPath;
}

void WorkshopItem::SetUpdateComment(string comment)
{
	m_itemUpdateComment = comment;
}

void WorkshopItem::SetVideoUrls(std::vector<std::string>& newVideoUrls)
{
	m_itemVideoUrls = newVideoUrls;
}

void WorkshopItem::SetCategories(std::vector<std::string>& newCategories)
{
	m_itemCategories = newCategories;
}

void WorkshopItem::SetScreenshots(std::vector<std::string>& newScreenshots)
{
	m_itemScreenshots = newScreenshots;
}

const std::vector<std::string>& WorkshopItem::GetScreenshots()
{
	return m_itemScreenshots;
}

void WorkshopItem::SetVisibility(ERemoteStoragePublishedFileVisibility newVisibility)
{
	m_itemVisibility = newVisibility;
}

const std::string& WorkshopItem::GetTitle()
{
	return m_itemTitle;
}

const std::string& WorkshopItem::GetDescription()
{
	return m_itemDescription;
}

const std::string& WorkshopItem::GetContentDir()
{
	return m_itemContentDir;
}

const std::string& WorkshopItem::GetPreviewImagePath()
{
	return m_itemPreviewImagePath;
}

const std::string& WorkshopItem::GetUpdateComment()
{
	return m_itemUpdateComment;
}

const std::vector<std::string>& WorkshopItem::GetCategories()
{
	return m_itemCategories;
}

const std::vector<std::string>& WorkshopItem::GetVideoUrls()
{
	return m_itemVideoUrls;
}

void WorkshopItem::Reset()
{
	SetItemId(0);
	SetVisibility(k_ERemoteStoragePublishedFileVisibilityPrivate);
	SetTitle("");
	SetDescription("");
	SetPreviewImagePath("");
	SetContentDir("");
	SetUpdateComment("");
}

void WorkshopItem::DebugDumpItemInfo()
{
	DebugLog("DebugDumpItemInfo BEGIN");
	DebugLog("");

	DebugLog("itemId: " + to_string(GetItemId()));
	DebugLog("title: " + GetTitle());
	DebugLog("description: " + GetDescription());
	DebugLog("contentDir: " + GetContentDir());
	DebugLog("previewImagePath: " + GetPreviewImagePath());
	DebugLog("updateComment: " + GetUpdateComment());

	string strVisibility = "m_itemVisibility: ";
	strVisibility += EnumParamToString(m_itemVisibility, WorkshopItemVisibilityModesList, ARRAY_SIZE(WorkshopItemVisibilityModesList));
	
	DebugLog(strVisibility);
	DebugLog("");

	DebugLog("screenshots:");
	DebugLogStrings(GetScreenshots());
	DebugLog("");

	DebugLog("videoUrls:");
	DebugLogStrings(GetVideoUrls());
	DebugLog("");

	DebugLog("categories:");
	DebugLogStrings(GetCategories());
	DebugLog("");

	DebugLog("DebugDumpItemInfo END");
}