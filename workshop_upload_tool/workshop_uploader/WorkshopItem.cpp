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

bool WorkshopItem::HasTitle() const
{
	return !m_itemTitle.empty();
}

bool WorkshopItem::HasDescription() const
{
	return !m_itemDescription.empty();
}

bool WorkshopItem::HasVisibility() const
{
	return	m_itemVisibility == k_ERemoteStoragePublishedFileVisibilityPublic ||
			m_itemVisibility == k_ERemoteStoragePublishedFileVisibilityFriendsOnly ||
			m_itemVisibility == k_ERemoteStoragePublishedFileVisibilityPrivate ||
			m_itemVisibility == k_ERemoteStoragePublishedFileVisibilityUnlisted;
}

bool WorkshopItem::HasCategories() const
{
	return !m_itemCategories.empty();
}

bool WorkshopItem::HasScreenshots() const
{
	return !m_itemScreenshots.empty();
}

bool WorkshopItem::HasVideoUrls() const
{
	return !m_itemVideoUrls.empty();
}

bool WorkshopItem::HasUpdateFields() const
{
	using Fn = bool (WorkshopItem::*)() const;

	static const Fn checks[] =
	{
		&WorkshopItem::HasTitle,
		&WorkshopItem::HasDescription,
		&WorkshopItem::HasVisibility,
		&WorkshopItem::HasValidContentDir,
		&WorkshopItem::HasValidPreviewImage,
		&WorkshopItem::HasCategories,
		&WorkshopItem::HasScreenshots,
		&WorkshopItem::HasVideoUrls
	};

	for (auto fn : checks)
		if ((this->*fn)())
			return true;

	return false;
}

bool WorkshopItem::IsSupportedImage(const string& filePath) const
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

bool WorkshopItem::HasValidPreviewImage() const
{
	const string& filePath = m_itemPreviewImagePath;

	return !filePath.empty() && FileExists(filePath) && IsSupportedImage(filePath);
}

bool WorkshopItem::HasValidContentDir() const
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
	if (!HasVisibility())
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
	if (!HasCategories())
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
	if (!HasScreenshots())
	{
		policy.OnScreenshotsEmpty(*this);
	}
	else
	{
		if (itemScreenshots.size() >= WorkshopItemLimits::kMaxScreenshots)
			policy.OnTooManyScreenshots(*this);

		for (const string& screenshot : itemScreenshots)
		{
			if (screenshot.empty()) //user provides multiple screenshot dirs and some of them is empty string
			{
				policy.OnScreenshotEmpty(*this);
			}
			else if (!FileExists(screenshot))
			{
				policy.OnScreenshotMissing(*this, screenshot);
			}
			else if (!IsSupportedImage(screenshot))
			{
				policy.OnScreenshotInvalid(*this, screenshot);
			}
		}
	}
	//videos
	if (!HasVideoUrls())
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

	//update comment
	if (GetUpdateComment().empty())
		policy.OnUpdateCommentEmpty(*this);
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

void WorkshopItem::SetVideoUrls(vector<string>& newVideoUrls)
{
	m_itemVideoUrls = newVideoUrls;
}

void WorkshopItem::SetCategories(vector<string>& newCategories)
{
	m_itemCategories = newCategories;
}

void WorkshopItem::SetScreenshots(vector<string>& newScreenshots)
{
	m_itemScreenshots = newScreenshots;
}

const vector<string>& WorkshopItem::GetScreenshots()
{
	return m_itemScreenshots;
}

void WorkshopItem::SetVisibility(ERemoteStoragePublishedFileVisibility newVisibility)
{
	m_itemVisibility = newVisibility;
}

const string& WorkshopItem::GetTitle()
{
	return m_itemTitle;
}

const string& WorkshopItem::GetDescription()
{
	return m_itemDescription;
}

const string& WorkshopItem::GetContentDir()
{
	return m_itemContentDir;
}

const string& WorkshopItem::GetPreviewImagePath()
{
	return m_itemPreviewImagePath;
}

const string& WorkshopItem::GetUpdateComment()
{
	return m_itemUpdateComment;
}

const vector<string>& WorkshopItem::GetCategories()
{
	return m_itemCategories;
}

const vector<string>& WorkshopItem::GetVideoUrls()
{
	return m_itemVideoUrls;
}

string WorkshopItem::GetVisibilityString() const
{
	const char* pchVisibility = EnumParamToString(m_itemVisibility, WorkshopItemVisibilityModesList, ARRAY_SIZE(WorkshopItemVisibilityModesList));
	return string(pchVisibility);
}

void WorkshopItem::Reset()
{
	SetItemId(0);
	SetVisibility((ERemoteStoragePublishedFileVisibility)-1); //sentinel
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
	DebugLog("itemVisibility: " + GetVisibilityString());
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