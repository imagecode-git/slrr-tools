#pragma once
#include <string>
#include <vector>
#include <algorithm> //any_of

class WorkshopItem; //forward declaration

struct WorkshopItemValidationMessage
{
	enum class Type
	{
		Error, //blocks the uploader
		Warning //user is prompted to continue
	};

	Type type;
	std::string message;
};

//policies are implemented by WorkshopItem::ValidateForSubmission
struct IWorkshopValidationPolicy
{
	virtual ~IWorkshopValidationPolicy() = default;
	virtual const std::vector<WorkshopItemValidationMessage>& GetMessages() const = 0;
	virtual bool HasWarnings() const = 0;
	virtual bool HasErrors() const = 0;

	//itemId
	virtual void OnItemIdInvalid(WorkshopItem&, const std::string&) = 0;

	//title
	virtual void OnTitleEmpty(WorkshopItem&) = 0;
	virtual void OnTitleTooLong(WorkshopItem&) = 0;

	//description
	virtual void OnDescriptionEmpty(WorkshopItem&) = 0;
	virtual void OnDescriptionTooLong(WorkshopItem&) = 0;

	//visibility
	virtual void OnVisibilityNotSet(WorkshopItem&) = 0;

	//content dir
	virtual void OnContentDirEmpty(WorkshopItem&) = 0;
	virtual void OnContentDirMissing(WorkshopItem&, const std::string&) = 0;
	virtual void OnContentDirHasNoFiles(WorkshopItem&, const std::string&) = 0;

	//preview image
	virtual void OnPreviewImageEmpty(WorkshopItem&) = 0;
	virtual void OnPreviewImageMissing(WorkshopItem&, const std::string&) = 0;
	virtual void OnPreviewImageInvalid(WorkshopItem&, const std::string&) = 0;

	//categories
	virtual void OnCategoriesEmpty(WorkshopItem&) = 0;
	virtual void OnCategoryEmpty(WorkshopItem&) = 0;

	//screenshots
	virtual void OnScreenshotsEmpty(WorkshopItem&) = 0;
	virtual void OnTooManyScreenshots(WorkshopItem&) = 0;
	virtual void OnScreenshotEmpty(WorkshopItem&) = 0;
	virtual void OnScreenshotMissing(WorkshopItem&, const std::string&) = 0;
	virtual void OnScreenshotInvalid(WorkshopItem&, const std::string&) = 0;

	//videos
	virtual void OnVideoUrlsEmpty(WorkshopItem&) = 0;
	virtual void OnVideoUrlEmpty(WorkshopItem&) = 0;
};

//this policy only validates workshop item
class BaseValidationPolicy : public IWorkshopValidationPolicy
{
protected:
	std::vector<WorkshopItemValidationMessage> messages;

public:

	const std::vector<WorkshopItemValidationMessage>& GetMessages() const override
	{
		return messages;
	}

	bool HasWarnings() const override;
	bool HasErrors() const override;

	//itemId
	void OnItemIdInvalid(WorkshopItem&, const std::string&) override;

	//title
    void OnTitleEmpty(WorkshopItem&) override;
    void OnTitleTooLong(WorkshopItem&) override;

	//description
    void OnDescriptionEmpty(WorkshopItem&) override;
    void OnDescriptionTooLong(WorkshopItem&) override;

	//visibility
	void OnVisibilityNotSet(WorkshopItem&) override;

	//content dir
	void OnContentDirEmpty(WorkshopItem&) override;
	void OnContentDirMissing(WorkshopItem&, const std::string&) override;
	void OnContentDirHasNoFiles(WorkshopItem&, const std::string&) override;

	//preview image
	void OnPreviewImageEmpty(WorkshopItem&) override;
	void OnPreviewImageMissing(WorkshopItem&, const std::string&) override;
	void OnPreviewImageInvalid(WorkshopItem&, const std::string&) override;

	//categories
	void OnCategoriesEmpty(WorkshopItem&) override;
	void OnCategoryEmpty(WorkshopItem&) override;

	//screenshots
	void OnScreenshotsEmpty(WorkshopItem&) override;
	void OnTooManyScreenshots(WorkshopItem&) override;
	void OnScreenshotEmpty(WorkshopItem&) override;
	void OnScreenshotMissing(WorkshopItem&, const std::string&) override;
	void OnScreenshotInvalid(WorkshopItem&, const std::string&) override;

	//videos
	void OnVideoUrlsEmpty(WorkshopItem&) override;
	void OnVideoUrlEmpty(WorkshopItem&) override;
};

//this policy restores default when necessary, only overrides rules it wants to repair or downgrade
class AutoCorrectValidationPolicy : public BaseValidationPolicy
{
public:

	//title
	void OnTitleEmpty(WorkshopItem&) override;
	void OnTitleTooLong(WorkshopItem&) override;

	//description
	void OnDescriptionEmpty(WorkshopItem&) override;
	void OnDescriptionTooLong(WorkshopItem&) override;

	//visibility
	void OnVisibilityNotSet(WorkshopItem&) override;

	//content dir
	void OnContentDirMissing(WorkshopItem&, const std::string&) override;

	//preview image
	void OnPreviewImageEmpty(WorkshopItem&) override;
	void OnPreviewImageMissing(WorkshopItem&, const std::string&) override;
	void OnPreviewImageInvalid(WorkshopItem&, const std::string&) override;

	//screenshots
	void OnScreenshotsEmpty(WorkshopItem&) override;
};

class CreateValidationPolicy : public AutoCorrectValidationPolicy
{
public:

	//itemId
	void OnItemIdInvalid(WorkshopItem&, const std::string&) override;
};

class DeleteValidationPolicy : public BaseValidationPolicy
{
public:

	//title
	void OnTitleEmpty(WorkshopItem&) override;
	void OnTitleTooLong(WorkshopItem&) override;

	//description
	void OnDescriptionEmpty(WorkshopItem&) override;
	void OnDescriptionTooLong(WorkshopItem&) override;

	//visibility
	void OnVisibilityNotSet(WorkshopItem&) override;

	//content dir
	void OnContentDirEmpty(WorkshopItem&) override;
	void OnContentDirMissing(WorkshopItem&, const std::string&) override;
	void OnContentDirHasNoFiles(WorkshopItem&, const std::string&) override;

	//preview image
	void OnPreviewImageEmpty(WorkshopItem&) override;
	void OnPreviewImageMissing(WorkshopItem&, const std::string&) override;
	void OnPreviewImageInvalid(WorkshopItem&, const std::string&) override;

	//categories
	void OnCategoriesEmpty(WorkshopItem&) override;
	void OnCategoryEmpty(WorkshopItem&) override;

	//screenshots
	void OnScreenshotsEmpty(WorkshopItem&) override;
	void OnTooManyScreenshots(WorkshopItem&) override;
	void OnScreenshotEmpty(WorkshopItem&) override;
	void OnScreenshotMissing(WorkshopItem&, const std::string&) override;
	void OnScreenshotInvalid(WorkshopItem&, const std::string&) override;

	//videos
	void OnVideoUrlsEmpty(WorkshopItem&) override;
	void OnVideoUrlEmpty(WorkshopItem&) override;
};