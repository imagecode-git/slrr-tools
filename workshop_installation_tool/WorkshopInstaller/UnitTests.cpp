#pragma once
#include "UnitTests.h"

UnitTest g_SelectedUnitTest = UnitTest::RunRefreshDataGrid;

void RunUnitTest(UnitTest testId)
{
	ManagerForm^ form = ManagerForm::GetInstance();
	if (!form)
	{
		MAIN->WarningMessage("UT: failed to retrieve ManagerForm");
		return;
	}

	CWorkshopItem* selectedItem = nullptr;
	uint64 selectedItemId = form->GetSelectedItemIdFromDataGrid();
	
	if (selectedItemId)
		selectedItem = MAIN->FindWorkshopItem(selectedItemId);

	switch (testId)
	{
	case UnitTest::SimulateNotCachedItem:
		if (selectedItem)
		{
			selectedItem->m_bDownloadInProgress = true;
			selectedItem->m_bIsDownloaded = false;

			UIState::NeedsFullUpdate = true;
		}
		break;

	case UnitTest::SimulateBrokenItem:
		MAIN->MarkInstallationBroken(selectedItemId, true);
		UIState::NeedsFullUpdate = true;

		break;

	case UnitTest::SimulateNotSubscribedItem:
		if (selectedItem)
		{
			selectedItem->m_bIsSubscribed = false;
			UIState::NeedsFullUpdate = true;
		}
		break;

	case UnitTest::SimulateEmptyDataGrid:
		for each(auto itemId in MAIN->GetKnownItemIds())
		{
			CWorkshopItem* item = MAIN->FindWorkshopItem(itemId);
			if (item)
				MAIN->RemoveWorkshopItemByPtr(item);
		}
		UIState::NeedsFullUpdate = true;
		break;

	case UnitTest::RunVerifyInstallation: //braces here to create a block scope and initialize bResult
	{
		bool bResult = MAIN->VerifyInstallation(selectedItemId);
		SetStatus("[UT] RunVerifyInstallation: " + bResult);
		break;
	}

	case UnitTest::RunRefreshDataGrid:
		UIState::NeedsDataGridUpdate = true;
		SetStatus("[UT] RunRefreshDataGrid is trying to refresh the grid...");
		break;

	case UnitTest::RunGetItemCachePath:
		if (selectedItem)
			MAIN->InfoMessage(MAIN->GetItemCachePath(selectedItem), "[UT] GetItemCachePath()");
		else
			MAIN->InfoMessage("selectedItem is nullptr!", "[UT] GetItemCachePath()");
		
		break;

	case UnitTest::RunBuildFileOwnerMap:
		MAIN->BuildFileOwnerMap();
		SetStatus("[UT] BuildFileOwnerMap has finished the job, check debug log");

	case UnitTest::RunIsItemMarkedOverwritten:
	{
		bool bResult = MAIN->IsItemMarkedOverwritten(selectedItemId, true);
		String^ strMessage = strMessage = "[UT] IsItemMarkedOverwritten() has been called with result: " + (bResult ? "true" : "false");

		SetStatus(strMessage);
		break;
	}

	case UnitTest::CheckFileConflicts:
	{
		List<String^>^ allFilesList = MAIN->GetFilesToInstall(selectedItemId, MAIN->FC_ALL_FILES);

		//conflict-relevant subset
		List<String^>^ conflictRelevantFiles = gcnew List<String^>();
		for each (String ^ relPath in allFilesList) //checking against all files, including .java/.class
		{
			if (IsInstallPayloadFile(relPath))
				conflictRelevantFiles->Add(relPath);
		}

		HashSet<String^>^ conflictedItemIds =
			gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

		//this makes uninstall log idempotent, prevents duplicate entries
		HashSet<String^>^ writtenFiles =
			gcnew HashSet<String^>(StringComparer::OrdinalIgnoreCase);

		if(MAIN->CheckFileConflicts(selectedItemId, conflictRelevantFiles))
			MAIN->WarningMessage("No file conlifcts detected for itemId " + selectedItemId);

		SetStatus("[UT] CheckFileConflicts has finished the job");
		break;
	}

	default:
		MAIN->WarningMessage("Unknown unit test ID");
		break;
	}
}

void CycleUnitTest(int delta)
{
	int value = static_cast<int>(g_SelectedUnitTest);
	int count = static_cast<int>(UnitTest::_count);

	//normalize in case value is invalid (_count or garbage)
	if (value < 0 || value >= count)
		value = 0;

	value = (value + delta + count) % count;

	g_SelectedUnitTest = static_cast<UnitTest>(value);
	SetStatus("CycleUnitTest: " + g_SelectedUnitTest.ToString());
}


void SetStatus(String^ text)
{
	ManagerForm::GetInstance()->SetStatusText(text); //debug only! normally it's done via deferred action for stability
}