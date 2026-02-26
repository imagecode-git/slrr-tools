#include "ManagerForm.h"
#include "Main.h"
#include "UIState.h"
#include "UnitTests.h"

using namespace WorkshopInstaller;

void ManagerForm::InitializeForm(void)
{
#ifndef _DEBUG
	//todo: rename these menu items without breaking the designer
	toolStripMenuItem1->Visible = false;
	toolStripMenuItem1->Enabled = false;
#endif

	m_bFormInitialized = false;
	m_bUpdatingGrid = false;

	m_SteamTimer = gcnew Timer();
	m_SteamTimer->Interval = STEAM_TIMER_FREQ;
	m_SteamTimer->Tick += gcnew EventHandler(this, &ManagerForm::OnSteamTick);
	m_SteamTimer->Start();

	ItemsDataGrid->RowEnter += gcnew DataGridViewCellEventHandler(this, &ManagerForm::DataGridView_RowEnter);
	ItemsDataGrid->SortCompare += gcnew DataGridViewSortCompareEventHandler(this, &ManagerForm::DataGridView_SortCompare);
	ItemsDataGrid->ColumnHeaderMouseClick += gcnew DataGridViewCellMouseEventHandler(this, &ManagerForm::DataGridView_ColumnHeaderMouseClick);
	PageButton->Click += gcnew System::EventHandler(this, &ManagerForm::PageButton_Click);
	FilesButton->Click += gcnew System::EventHandler(this, &ManagerForm::FilesButton_Click);
	ActionButton->Click += gcnew System::EventHandler(this, &ManagerForm::ActionButton_Click);
	FormClosed += gcnew System::Windows::Forms::FormClosedEventHandler(this, &ManagerForm::OnManagerFormClosed);
	Shown += gcnew EventHandler(this, &ManagerForm::OnManagerFormShown);
	KeyDown += gcnew KeyEventHandler(this, &ManagerForm::OnKeyDown);

	toolStripMenuItem2->Click += gcnew System::EventHandler(this, &ManagerForm::aboutToolStripMenuItem_Click);
	toolStripMenuItem3->Click += gcnew System::EventHandler(this, &ManagerForm::downloadAllToolStripMenuItem_Click);
	dumpAllToolStripMenuItem->Click += gcnew System::EventHandler(this, &ManagerForm::dumpAllToolStripMenuItem_Click);
	unitTestToolStripMenuItem->Click += gcnew System::EventHandler(this, &ManagerForm::unitTestToolStripMenuItem_Click);
	
	//temp. hide and disable page/files buttons
	PageButton->Visible = false;
	PageButton->Enabled = false;

	FilesButton->Visible = false;
	FilesButton->Enabled = false;

	m_selectedWorkshopItemId = 0;

	ItemsDataGrid->Columns[COL_ID]->SortMode = DataGridViewColumnSortMode::Programmatic;
	ItemsDataGrid->Columns[COL_SIZE]->SortMode = DataGridViewColumnSortMode::Programmatic;

	m_sortState.Column = GridSortColumn::Title;
	m_sortState.Direction = ListSortDirection::Ascending;

	ItemsDataGrid->Columns[COL_OVERWRITTEN]->Visible = g_EnableOverwrittenColumn;
}

void ManagerForm::OnManagerFormShown(Object^ sender, EventArgs^ e)
{
	if (m_bFormInitialized)
		return;

	m_bFormInitialized = true;
	
	ForceForeground(); //this helps to restore app focus after user prompt
	BeginInvoke(gcnew Action(this, &ManagerForm::FocusDataGrid)); //focus the grid
	RefreshForm();
}

void ManagerForm::OnManagerFormClosed(Object^ sender, FormClosedEventArgs^ e)
{
	MAIN->Shutdown();
}

void ManagerForm::OnSteamTick(Object^ sender, EventArgs^ e)
{
	SteamAPI_RunCallbacks();
	MAIN->RefreshInstallerState();

	if (!m_bFormInitialized)
		return;

	//observe UI states
	if (UIState::NeedsFullUpdate.exchange(false))
	{
		RefreshForm();
	}	
	else
	{
		if (UIState::NeedsActionButtonUpdate.exchange(false))
			RefreshActionButton();

		if (UIState::NeedsDescriptionUpdate.exchange(false))
			RefreshDescription();

		if (UIState::NeedsGamePathLabelUpdate.exchange(false))
			RefreshGamePathLabel();

		if (UIState::NeedsDataGridUpdate.exchange(false))
			RefreshDataGrid();
	}
}

//refresh DataGridView, buttons, everything
void ManagerForm::RefreshForm()
{
	if (m_bFormInitialized && IsHandleCreated && Visible)
	{
		RefreshDataGrid();
		RefreshActionButton();
		RefreshDescription();
		RefreshGamePathLabel();
	}
}

void ManagerForm::RefreshActionButton()
{
	ActionButtonState state = ResolveActionButtonState(m_selectedWorkshopItemId);
	ApplyActionButtonState(state);
}

void ManagerForm::RefreshDescription()
{
	CWorkshopItem* item = MAIN->FindWorkshopItem(m_selectedWorkshopItemId);

	if (item)
	{
		//we also have DescriptionLabel in the project that is a fixed-height version of the DescriptionText, why it was introduced?
		DescriptionText->Text = gcnew String(item->GetDescription(), 0, (int)strlen(item->GetDescription()), Encoding::UTF8); //Steam default encoding is UTF8
	}
}

void ManagerForm::RefreshDataGrid()
{
	if (!GetSelectedItemIdFromDataGrid())
		m_selectedWorkshopItemId = 0;

	m_bUpdatingGrid = true;

	DataGridView^ grid = ItemsDataGrid;

	//0. preserve scrolling position
	int firstVisibleRow = -1;

	if (grid->FirstDisplayedScrollingRowIndex >= 0)
		firstVisibleRow = grid->FirstDisplayedScrollingRowIndex;

	//1. freeze the grid completely
	grid->SuspendLayout();

#ifdef FREEZE_DATAGRID_ON_REFRESH
	//grid->Enabled = false;
	//grid->Visible = false; //stop painting to prevent crashing on WM_PAINT
#endif

	//2. disable autosizing temporarily
	DataGridViewAutoSizeColumnsMode oldColumnsMode = grid->AutoSizeColumnsMode;
	grid->AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode::None;

	cli::array<DataGridViewAutoSizeColumnMode>^ oldColModes =
		gcnew cli::array<DataGridViewAutoSizeColumnMode>(grid->Columns->Count);

	for (int i = 0; i < grid->Columns->Count; i++)
	{
		oldColModes[i] = grid->Columns[i]->AutoSizeMode;
		grid->Columns[i]->AutoSizeMode =
			DataGridViewAutoSizeColumnMode::None;
	}

	//3. detach selection and scrolling state
	grid->CurrentCell = nullptr;
	grid->ClearSelection();
	grid->SelectionMode = DataGridViewSelectionMode::RowHeaderSelect;

	//4. only now we can mutate the rows
	grid->Rows->Clear();

	for each (UInt64 itemId in MAIN->GetKnownItemIds())
	{
		CWorkshopItem* item = MAIN->FindWorkshopItem(itemId);

		if (!item)
			continue;

#ifndef _DEBUG
		if (!item->m_bIsCompatible)
			continue;
#endif

		int rowIndex = grid->Rows->Add(); //Rows->Count may lie and crash the installer, Rows->Add() is more reliable
		DataGridViewRow^ row = grid->Rows[rowIndex];

		if (rowIndex >= 0)
		{
			bool bItemSubscribed = item->m_bIsSubscribed;
			bool bItemInstalled = item->m_bIsInstalled;
			bool bItemOverwritten = MAIN->IsItemMarkedOverwritten(itemId);

			row->Cells[COL_SUBSCRIBED]->Value = bItemSubscribed ? LOC_SUBSCRIBED_TRUE : LOC_SUBSCRIBED_FALSE;
			row->Cells[COL_INSTALLED]->Value = bItemInstalled ? LOC_INSTALLED_TRUE : LOC_INSTALLED_FALSE;
			row->Cells[COL_OVERWRITTEN]->Value = bItemOverwritten ? LOC_OVERWRITTEN_TRUE : LOC_OVERWRITTEN_FALSE;

			row->Cells[COL_SUBSCRIBED]->Tag = bItemSubscribed ? 1 : 0;
			row->Cells[COL_INSTALLED]->Tag = bItemInstalled ? 1 : 0;
			row->Cells[COL_OVERWRITTEN]->Tag = bItemOverwritten ? 1 : 0;
			
			row->Cells[COL_ID]->Value = Convert::ToString((long long)itemId, 10);
			row->Cells[COL_ID]->Tag = itemId;

			row->Cells[COL_TITLE]->Value = gcnew String(item->GetTitle());
			row->Cells[COL_AUTHOR]->Value = Convert::ToString((long long)item->GetAuthorID(), 10);
			
			double itemSize = item->GetSizeInMegabytes();
			row->Cells[COL_SIZE]->Value = String::Format("{0:0.000} {1}", item->GetSizeInMegabytes(), LOC_SIZE_MEGABYTES);
			row->Cells[COL_SIZE]->Tag = itemSize;

			Drawing::Color rowColor = Color::Black;

			//normally incompatible items should be only visible in debug mode
			if (!item->m_bIsCompatible)
				row->DefaultCellStyle->ForeColor = Color::Gray;

			if (item->m_bIsBroken)
				row->DefaultCellStyle->ForeColor = Color::Red;
		}
	}

	SortDataGridItems();

	//restore grid state
	for (int i = 0; i < grid->Columns->Count; i++)
		grid->Columns[i]->AutoSizeMode = oldColModes[i];

	grid->AutoSizeColumnsMode = oldColumnsMode;

	//unsuspend the grid
	grid->ResumeLayout();

#ifdef FREEZE_DATAGRID_ON_REFRESH
	//grid->Enabled = true;
	//grid->Visible = true;
#endif

	grid->SelectionMode = DataGridViewSelectionMode::FullRowSelect;

	if (firstVisibleRow >= 0 && firstVisibleRow < ItemsDataGrid->Rows->Count)
	{
		try
		{
			ItemsDataGrid->FirstDisplayedScrollingRowIndex = firstVisibleRow;
		}
		catch (Exception^ e)
		{
			DebugLog("exception on attempting to restore datagrid scrolling position: " + e->Message);
		}
	}

	m_bUpdatingGrid = false;

	//initial selection, this is done only once
	if (MAIN->AllWorkshopItemsResolved() && !m_selectedWorkshopItemId)
		m_selectedWorkshopItemId = GetItemIdFromRow(0);

	BeginInvoke(gcnew Action(this, &ManagerForm::FocusDataGrid)); //restore focus on the grid
	BeginInvoke(gcnew Action<UInt64>(this, &ManagerForm::SelectRowByItemId), m_selectedWorkshopItemId);
}

void ManagerForm::RefreshGamePathLabel()
{
	String^ gamePath = MAIN->GetGamePath();
	if (!gamePath)
		gamePath = LOC_GAME_PATH_UNDEFINED;

	String^ statusText = String::Format(LOC_GAME_PATH_LABEL, gamePath);
	BeginInvoke(gcnew Action<String^>(this, &ManagerForm::SetStatusText), statusText);
		
	//statusLabel->Text = String::Format(LOC_GAME_PATH_LABEL, gamePath);
}

void ManagerForm::SelectRowByIndex(int rowIndex)
{
	if (rowIndex < 0 || rowIndex >= ItemsDataGrid->Rows->Count)
		return;

	ItemsDataGrid->ClearSelection();
	ItemsDataGrid->Rows[rowIndex]->Selected = true;
	ItemsDataGrid->CurrentCell = ItemsDataGrid->Rows[rowIndex]->Cells[0];

	m_selectedWorkshopItemId = GetItemIdFromRow(rowIndex);
}

void ManagerForm::SelectRowByItemId(UInt64 itemId)
{
	for each (DataGridViewRow ^ row in ItemsDataGrid->Rows)
	{
		if (!row->Cells[COL_ID]->Value)
			continue;

		if (Convert::ToUInt64(row->Cells[COL_ID]->Value) == itemId)
		{
			ItemsDataGrid->ClearSelection();
			row->Selected = true;
			ItemsDataGrid->CurrentCell = row->Cells[0];

			//force scroll even if WinForms thinks it's visible, this prevents edge cases when sorting by itemId is active and scroll state is restored incorrectly
			ItemsDataGrid->FirstDisplayedScrollingRowIndex = row->Index;

			m_selectedWorkshopItemId = itemId;

			return;
		}
	}
}

void ManagerForm::SetStatusText(String^ text)
{
	statusLabel->Text = text;
}

void ManagerForm::FocusDataGrid()
{
	ItemsDataGrid->Focus();
}

void ManagerForm::ForceForeground()
{
	this->TopMost = true;
	this->Activate();
	this->BringToFront();
	this->TopMost = false;
}

void ManagerForm::CaptureCurrentSortState()
{
	DataGridViewColumn^ col = ItemsDataGrid->SortedColumn;
	if (!col)
		return;

	ListSortDirection dir =
		(ItemsDataGrid->SortOrder == SortOrder::Descending)
		? ListSortDirection::Descending
		: ListSortDirection::Ascending;

	UpdateSortStateFromColumn(col, dir);
}

void ManagerForm::SortDataGridItems()
{
	DataGridViewColumn^ col = nullptr;

	switch (m_sortState.Column)
	{
	case GridSortColumn::Id: col = ItemsDataGrid->Columns[COL_ID];					break;
	case GridSortColumn::Title: col = ItemsDataGrid->Columns[COL_TITLE];			break;
	case GridSortColumn::Size: col = ItemsDataGrid->Columns[COL_SIZE];				break;
	case GridSortColumn::Installed: col = ItemsDataGrid->Columns[COL_INSTALLED];	break;
	case GridSortColumn::Subscribed: col = ItemsDataGrid->Columns[COL_SUBSCRIBED];	break;
	}

	if (!col)
		return;

	ItemsDataGrid->Sort(col, m_sortState.Direction);
	col->HeaderCell->SortGlyphDirection =
		m_sortState.Direction == ListSortDirection::Ascending
		? SortOrder::Ascending
		: SortOrder::Descending;
}

void ManagerForm::UpdateSortStateFromColumn(DataGridViewColumn^ col, ListSortDirection dir)
{
	if (col == ItemsDataGrid->Columns[COL_ID])
		m_sortState.Column = GridSortColumn::Id;
	else if (col == ItemsDataGrid->Columns[COL_TITLE])
		m_sortState.Column = GridSortColumn::Title;
	else if (col == ItemsDataGrid->Columns[COL_SIZE])
		m_sortState.Column = GridSortColumn::Size;
	else if (col == ItemsDataGrid->Columns[COL_INSTALLED])
		m_sortState.Column = GridSortColumn::Installed;
	else if (col == ItemsDataGrid->Columns[COL_SUBSCRIBED])
		m_sortState.Column = GridSortColumn::Subscribed;
	else
		return; // unknown column, ignore

	m_sortState.Direction = dir;
}

UInt64 ManagerForm::GetSelectedItemIdFromDataGrid()
{
	if (ItemsDataGrid->SelectedRows->Count)
		return Convert::ToInt64(ItemsDataGrid->SelectedRows[0]->Cells[COL_ID]->Value);

	return 0;
}

UInt64 ManagerForm::GetItemIdFromRow(int rowIndex)
{
	if (!ItemsDataGrid->Rows->Count)
		return 0;

	if (rowIndex < 0 || rowIndex >= ItemsDataGrid->Rows->Count)
		return 0;

	Object^ tag = ItemsDataGrid->Rows[rowIndex]->Cells[COL_ID]->Tag;
	if (!tag)
		return 0;

	return safe_cast<UInt64>(tag);
}

void ManagerForm::DataGridView_RowEnter(Object^ sender, DataGridViewCellEventArgs^ e)
{
	//do nothing with the grid while its updating
	if (m_bUpdatingGrid)
		return;

	if (ItemsDataGrid->SelectedRows->Count == 0)
		return;

	m_selectedWorkshopItemId = GetSelectedItemIdFromDataGrid();
	RefreshDescription();
	RefreshActionButton();
}

//sort by size correctly, i.e. by comparing numbers, not texts
void ManagerForm::DataGridView_SortCompare(Object^ sender, DataGridViewSortCompareEventArgs^ e)
{
	if (e->Column->Index == COL_SIZE)
	{
		//we store the numbers in Tag
		double a = safe_cast<double>(ItemsDataGrid->Rows[e->RowIndex1]->Cells[COL_SIZE]->Tag);
		double b = safe_cast<double>(ItemsDataGrid->Rows[e->RowIndex2]->Cells[COL_SIZE]->Tag);

		e->SortResult = a.CompareTo(b);
		e->Handled = true;
	}
	else if (e->Column->Index == COL_ID)
	{
		UInt64 a = safe_cast<UInt64>(ItemsDataGrid->Rows[e->RowIndex1]->Cells[COL_ID]->Tag);
		UInt64 b = safe_cast<UInt64>(ItemsDataGrid->Rows[e->RowIndex2]->Cells[COL_ID]->Tag);

		e->SortResult = a.CompareTo(b);
		e->Handled = true;
	}
	else if (
		e->Column->Index == COL_INSTALLED ||
		e->Column->Index == COL_SUBSCRIBED ||
		e->Column->Index == COL_OVERWRITTEN)
	{
		int a = safe_cast<int>(ItemsDataGrid->Rows[e->RowIndex1]->Cells[e->Column->Index]->Tag);
		int b = safe_cast<int>(ItemsDataGrid->Rows[e->RowIndex2]->Cells[e->Column->Index]->Tag);

		e->SortResult = a.CompareTo(b);
		e->Handled = true;
	}
}

void ManagerForm::DataGridView_ColumnHeaderMouseClick(
	Object^ sender,
	DataGridViewCellMouseEventArgs^ e)
{
	DataGridViewColumn^ col = ItemsDataGrid->Columns[e->ColumnIndex];

	//programmatic columns: update state manually
	if (col->Index == COL_SIZE || col->Index == COL_ID)
	{
		ListSortDirection dir =
			(col->HeaderCell->SortGlyphDirection == SortOrder::Ascending)
			? ListSortDirection::Descending
			: ListSortDirection::Ascending;

		UpdateSortStateFromColumn(col, dir);

		//explicitly trigger sort
		ItemsDataGrid->Sort(col, dir);

		col->HeaderCell->SortGlyphDirection =
			(dir == ListSortDirection::Ascending)
			? SortOrder::Ascending
			: SortOrder::Descending;

		return;
	}

	//automatic columns: defer capture
	BeginInvoke(gcnew Action(this, &ManagerForm::CaptureCurrentSortState));
}

ActionButtonState ManagerForm::ResolveActionButtonState(UInt64 itemId)
{
	CWorkshopItem* item = MAIN->FindWorkshopItem(itemId);
	if (!item)
		return ActionButtonState::Disabled;

	uint32 state = SteamUGC()->GetItemState(itemId);

	bool bIsSubscribed = (state & k_EItemStateSubscribed) != 0;
	bool bIsDownloading = (state & k_EItemStateDownloading) != 0;
	bool bIsPending = (state & k_EItemStateDownloadPending) != 0;
	bool bIsSteamInstalled = (state & k_EItemStateInstalled) != 0;

	//removal always allowed if installed into game
	if (item->m_bIsInstalled)
		return ActionButtonState::Removable;

	//not subscribed and not installed -> nothing to do
	if (!bIsSubscribed)
		return ActionButtonState::Disabled;

	//Steam still downloading
	if (bIsDownloading || bIsPending)
		return ActionButtonState::Downloading;

	if (!bIsSteamInstalled)
		return ActionButtonState::Downloading;

	return ActionButtonState::Installable;
}

void ManagerForm::ApplyActionButtonState(ActionButtonState state)
{
	bool bIsDemoMode = MAIN->IsDemoMode();

	switch (state)
	{
    case ActionButtonState::Downloading:
		ActionButton->Text = LOC_DOWNLOADING;
		ActionButton->Enabled = false;
		break;

	case ActionButtonState::Installable:
		ActionButton->Text = LOC_INSTALL;
		ActionButton->Enabled = !bIsDemoMode;
		break;

	case ActionButtonState::Removable:
		ActionButton->Text = LOC_REMOVE;
		ActionButton->Enabled = !bIsDemoMode;
		break;

	default:
		ActionButton->Text = LOC_ACTION;
		ActionButton->Enabled = false;
		break;
	}
}

void ManagerForm::ActionButton_Click(Object^ sender, EventArgs^ e)
{
	if (!ActionButton->Enabled || ItemsDataGrid->SelectedRows->Count == 0)
	{
		return;
	}

	uint64 itemId = GetSelectedItemIdFromDataGrid();
	CWorkshopItem* item = MAIN->FindWorkshopItem(itemId);

	if (g_PromptUserOnManageItem)
	{
		String^ manageAction = item->m_bIsInstalled ? LOC_USER_PROMPT_MNG_REMOVE : LOC_USER_PROMPT_MNG_INSTALL;
		String^ userMessage = String::Format(LOC_USER_PROMPT_MNG_BODY, manageAction, gcnew String(item->GetTitle()));

		if (!MAIN->UserPromptYesNo(userMessage, LOC_USER_PROMPT_MNG_CAPTION))
			return;
	}

	if(item)
	{
		bool bNeedUpdate = MAIN->ManageWorkshopItem(itemId);
		if(bNeedUpdate)
			RefreshForm(); //refresh UI only if necessary
	}
}

void ManagerForm::FilesButton_Click(Object^ sender, EventArgs^ e)
{
	uint64 id_item = GetSelectedItemIdFromDataGrid();
	String^ strItemId = gcnew String("" + id_item);
	DirectoryInfo^ dir = gcnew DirectoryInfo(MAIN->GetWorkshopPath() + "\\" + strItemId);
	
	DebugLog("FilesButton_Click: " + dir->FullName);
	
	if (!dir->Exists)
	{
		return;
	}

	Process::Start("explorer.exe", dir->FullName);
}

//this function is supposed to open Steam Workshop page with a given item
void ManagerForm::PageButton_Click(Object^ sender, EventArgs^ e)
{
	uint64 id_item = GetSelectedItemIdFromDataGrid();
	ShellExecuteW(0, 0, L"steam://openurl/https://steamcommunity.com/sharedfiles/filedetails/?id=2584483962", 0, 0, SW_SHOW);
}

void ManagerForm::aboutToolStripMenuItem_Click(Object^ sender, EventArgs^ e)
{
	String^ DescriptionText = String::Format(LOC_VERSION_DESC, INSTALLER_VERSION);
	MAIN->InfoMessage(DescriptionText, LOC_VERSION_MSG_CAPTION);
}

void ManagerForm::downloadAllToolStripMenuItem_Click(Object^ sender, EventArgs^ e)
{
	MAIN->DownloadAllWorkshopItems();
}

void ManagerForm::dumpAllToolStripMenuItem_Click(Object^ sender, EventArgs^ e)
{
	MAIN->DumpWorkshopItemsInfo();
	MAIN->InfoMessage(String::Format(LOC_DBG_ITEMS_DUMPED, MAIN->GetNumWorkshopItems()), LOC_INFO_MSG_CAPTION);
}

void ManagerForm::unitTestToolStripMenuItem_Click(Object^ sender, EventArgs^ e)
{
	RunUnitTest(g_SelectedUnitTest);
}

void ManagerForm::OnKeyDown(Object^ sender, KeyEventArgs^ e)
{
#ifdef _DEBUG
	if (e->Control && e->KeyCode == Keys::Right)
	{
		CycleUnitTest(+1);
		e->Handled = true;
	}
	else if (e->Control && e->KeyCode == Keys::Left)
	{
		CycleUnitTest(-1);
		e->Handled = true;
	}
#endif
}

//leave this code ugly as is, otherwise the designer will crash again!
void ManagerForm::InitializeComponent(void)
{
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle1 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle2 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle9 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle10 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle11 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle3 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle4 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle5 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle6 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle7 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::Windows::Forms::DataGridViewCellStyle^ dataGridViewCellStyle8 = (gcnew System::Windows::Forms::DataGridViewCellStyle());
	System::ComponentModel::ComponentResourceManager^ resources = (gcnew System::ComponentModel::ComponentResourceManager(ManagerForm::typeid));
	this->ItemsGroup = (gcnew System::Windows::Forms::GroupBox());
	this->ItemsDataGrid = (gcnew System::Windows::Forms::DataGridView());
	this->SubscribedColumn = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
	this->InstalledColumn = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
	this->OverwrittenColumn = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
	this->IdColumn = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
	this->TitleColumn = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
	this->AuthorColumn = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
	this->SizeColumn = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
	this->SideGroup = (gcnew System::Windows::Forms::GroupBox());
	this->PageButton = (gcnew System::Windows::Forms::Button());
	this->FilesButton = (gcnew System::Windows::Forms::Button());
	this->DescriptionText = (gcnew System::Windows::Forms::TextBox());
	this->ActionButton = (gcnew System::Windows::Forms::Button());
	this->LoadingProgress = (gcnew System::Windows::Forms::ProgressBar());
	this->DescriptionLabel = (gcnew System::Windows::Forms::Label());
	this->menuStrip1 = (gcnew System::Windows::Forms::MenuStrip());
	this->toolStripMenuItem1 = (gcnew System::Windows::Forms::ToolStripMenuItem());
	this->toolStripMenuItem3 = (gcnew System::Windows::Forms::ToolStripMenuItem());
	this->dumpAllToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
	this->unitTestToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
	this->toolStripMenuItem2 = (gcnew System::Windows::Forms::ToolStripMenuItem());
	this->statusStrip1 = (gcnew System::Windows::Forms::StatusStrip());
	this->statusLabel = (gcnew System::Windows::Forms::ToolStripStatusLabel());
	this->ItemsGroup->SuspendLayout();
	(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->ItemsDataGrid))->BeginInit();
	this->SideGroup->SuspendLayout();
	this->menuStrip1->SuspendLayout();
	this->statusStrip1->SuspendLayout();
	this->SuspendLayout();
	// 
	// ItemsGroup
	// 
	this->ItemsGroup->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom)
		| System::Windows::Forms::AnchorStyles::Left)
		| System::Windows::Forms::AnchorStyles::Right));
	this->ItemsGroup->AutoSize = true;
	this->ItemsGroup->Controls->Add(this->ItemsDataGrid);
	this->ItemsGroup->Location = System::Drawing::Point(16, 56);
	this->ItemsGroup->Margin = System::Windows::Forms::Padding(4);
	this->ItemsGroup->Name = L"ItemsGroup";
	this->ItemsGroup->Padding = System::Windows::Forms::Padding(14, 12, 14, 12);
	this->ItemsGroup->Size = System::Drawing::Size(938, 649);
	this->ItemsGroup->TabIndex = 0;
	this->ItemsGroup->TabStop = false;
	this->ItemsGroup->Text = L"Items";
	// 
	// ItemsDataGrid
	// 
	this->ItemsDataGrid->AllowUserToAddRows = false;
	this->ItemsDataGrid->AllowUserToDeleteRows = false;
	this->ItemsDataGrid->AllowUserToOrderColumns = true;
	this->ItemsDataGrid->AllowUserToResizeRows = false;
	dataGridViewCellStyle1->BackColor = System::Drawing::SystemColors::ControlLight;
	this->ItemsDataGrid->AlternatingRowsDefaultCellStyle = dataGridViewCellStyle1;
	this->ItemsDataGrid->AutoSizeColumnsMode = System::Windows::Forms::DataGridViewAutoSizeColumnsMode::Fill;
	this->ItemsDataGrid->BackgroundColor = System::Drawing::SystemColors::Control;
	this->ItemsDataGrid->BorderStyle = System::Windows::Forms::BorderStyle::Fixed3D;
	this->ItemsDataGrid->ColumnHeadersBorderStyle = System::Windows::Forms::DataGridViewHeaderBorderStyle::None;
	dataGridViewCellStyle2->BackColor = System::Drawing::SystemColors::Control;
	dataGridViewCellStyle2->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Regular,
		System::Drawing::GraphicsUnit::Point, static_cast<System::Byte>(0)));
	dataGridViewCellStyle2->ForeColor = System::Drawing::SystemColors::WindowText;
	dataGridViewCellStyle2->SelectionBackColor = System::Drawing::SystemColors::Highlight;
	dataGridViewCellStyle2->SelectionForeColor = System::Drawing::SystemColors::HighlightText;
	dataGridViewCellStyle2->WrapMode = System::Windows::Forms::DataGridViewTriState::False;
	this->ItemsDataGrid->ColumnHeadersDefaultCellStyle = dataGridViewCellStyle2;
	this->ItemsDataGrid->ColumnHeadersHeight = 28;
	this->ItemsDataGrid->Columns->AddRange(gcnew cli::array< System::Windows::Forms::DataGridViewColumn^  >(7) {
		this->SubscribedColumn,
			this->InstalledColumn, this->OverwrittenColumn, this->IdColumn, this->TitleColumn, this->AuthorColumn, this->SizeColumn
	});
	dataGridViewCellStyle9->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleLeft;
	dataGridViewCellStyle9->BackColor = System::Drawing::SystemColors::Control;
	dataGridViewCellStyle9->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
		static_cast<System::Byte>(204)));
	dataGridViewCellStyle9->ForeColor = System::Drawing::SystemColors::ControlText;
	
	//system colors could be hard to read
	//dataGridViewCellStyle9->SelectionBackColor = System::Drawing::SystemColors::GradientActiveCaption;
	//dataGridViewCellStyle9->SelectionForeColor = System::Drawing::SystemColors::HighlightText;
	
	dataGridViewCellStyle9->SelectionBackColor = System::Drawing::Color::FromArgb(80, 80, 80);
	dataGridViewCellStyle9->SelectionForeColor = System::Drawing::Color::White;
	
	dataGridViewCellStyle9->WrapMode = System::Windows::Forms::DataGridViewTriState::False;
	this->ItemsDataGrid->DefaultCellStyle = dataGridViewCellStyle9;
	this->ItemsDataGrid->Dock = System::Windows::Forms::DockStyle::Fill;
	this->ItemsDataGrid->GridColor = System::Drawing::SystemColors::ScrollBar;
	this->ItemsDataGrid->Location = System::Drawing::Point(14, 36);
	this->ItemsDataGrid->Margin = System::Windows::Forms::Padding(4);
	this->ItemsDataGrid->MultiSelect = false;
	this->ItemsDataGrid->Name = L"ItemsDataGrid";
	this->ItemsDataGrid->ReadOnly = true;
	this->ItemsDataGrid->RowHeadersBorderStyle = System::Windows::Forms::DataGridViewHeaderBorderStyle::Single;
	dataGridViewCellStyle10->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleLeft;
	dataGridViewCellStyle10->BackColor = System::Drawing::SystemColors::Control;
	dataGridViewCellStyle10->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 7.875F, System::Drawing::FontStyle::Regular,
		System::Drawing::GraphicsUnit::Point, static_cast<System::Byte>(0)));
	dataGridViewCellStyle10->ForeColor = System::Drawing::SystemColors::WindowText;
	dataGridViewCellStyle10->SelectionBackColor = System::Drawing::SystemColors::Highlight;
	dataGridViewCellStyle10->SelectionForeColor = System::Drawing::SystemColors::HighlightText;
	dataGridViewCellStyle10->WrapMode = System::Windows::Forms::DataGridViewTriState::False;
	this->ItemsDataGrid->RowHeadersDefaultCellStyle = dataGridViewCellStyle10;
	this->ItemsDataGrid->RowHeadersVisible = false;
	this->ItemsDataGrid->RowHeadersWidth = 82;
	this->ItemsDataGrid->RowTemplate->Height = 28;
	this->ItemsDataGrid->ScrollBars = System::Windows::Forms::ScrollBars::Vertical;
	this->ItemsDataGrid->SelectionMode = System::Windows::Forms::DataGridViewSelectionMode::FullRowSelect;
	this->ItemsDataGrid->Size = System::Drawing::Size(910, 601);
	this->ItemsDataGrid->TabIndex = 0;
	// 
	// SubscribedColumn
	// 
	this->SubscribedColumn->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::AllCellsExceptHeader;
	dataGridViewCellStyle3->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleCenter;
	this->SubscribedColumn->DefaultCellStyle = dataGridViewCellStyle3;
	this->SubscribedColumn->FillWeight = 8;
	this->SubscribedColumn->HeaderText = L"S";
	this->SubscribedColumn->MinimumWidth = 15;
	this->SubscribedColumn->Name = L"SubscribedColumn";
	this->SubscribedColumn->ReadOnly = true;
	this->SubscribedColumn->ToolTipText = L"Subscribed";
	this->SubscribedColumn->Width = 15;
	// 
	// InstalledColumn
	// 
	this->InstalledColumn->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::AllCellsExceptHeader;
	dataGridViewCellStyle4->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleCenter;
	this->InstalledColumn->DefaultCellStyle = dataGridViewCellStyle4;
	this->InstalledColumn->FillWeight = 8;
	this->InstalledColumn->HeaderText = L"I";
	this->InstalledColumn->MinimumWidth = 15;
	this->InstalledColumn->Name = L"InstalledColumn";
	this->InstalledColumn->ReadOnly = true;
	this->InstalledColumn->ToolTipText = L"Installed";
	this->InstalledColumn->Width = 15;
	// 
	// OverwrittenColumn
	// 
	this->OverwrittenColumn->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::AllCellsExceptHeader;
	dataGridViewCellStyle11->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleCenter;
	this->OverwrittenColumn->DefaultCellStyle = dataGridViewCellStyle11;
	this->OverwrittenColumn->FillWeight = 8;
	this->OverwrittenColumn->HeaderText = L"O";
	this->OverwrittenColumn->MinimumWidth = 15;
	this->OverwrittenColumn->Name = L"OverwrittenColumn";
	this->OverwrittenColumn->ReadOnly = true;
	this->OverwrittenColumn->ToolTipText = L"Overwritten";
	this->OverwrittenColumn->Width = 15;
	// 
	// IdColumn
	// 
	this->IdColumn->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::AllCellsExceptHeader;
	dataGridViewCellStyle5->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleRight;
	this->IdColumn->DefaultCellStyle = dataGridViewCellStyle5;
	this->IdColumn->HeaderText = L"Item ID";
	this->IdColumn->MinimumWidth = 10;
	this->IdColumn->Name = L"IdColumn";
	this->IdColumn->ReadOnly = true;
	this->IdColumn->Width = 10;
	// 
	// TitleColumn
	// 
	this->TitleColumn->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::Fill;
	dataGridViewCellStyle6->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleLeft;
	dataGridViewCellStyle6->WrapMode = System::Windows::Forms::DataGridViewTriState::True;
	this->TitleColumn->DefaultCellStyle = dataGridViewCellStyle6;
	this->TitleColumn->FillWeight = 234.1628F;
	this->TitleColumn->HeaderText = L"Title";
	this->TitleColumn->MinimumWidth = 10;
	this->TitleColumn->Name = L"TitleColumn";
	this->TitleColumn->ReadOnly = true;
	// 
	// AuthorColumn
	// 
	this->AuthorColumn->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::ColumnHeader;
	dataGridViewCellStyle7->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleRight;
	this->AuthorColumn->DefaultCellStyle = dataGridViewCellStyle7;
	this->AuthorColumn->HeaderText = L"Author ID";
	this->AuthorColumn->MinimumWidth = 10;
	this->AuthorColumn->Name = L"AuthorColumn";
	this->AuthorColumn->ReadOnly = true;
	this->AuthorColumn->Visible = false;
	this->AuthorColumn->Width = 149;
	// 
	// SizeColumn
	// 
	this->SizeColumn->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::AllCellsExceptHeader;
	dataGridViewCellStyle8->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleRight;
	this->SizeColumn->DefaultCellStyle = dataGridViewCellStyle8;
	this->SizeColumn->HeaderText = L"Size";
	this->SizeColumn->MinimumWidth = 10;
	this->SizeColumn->Name = L"SizeColumn";
	this->SizeColumn->ReadOnly = true;
	this->SizeColumn->Width = 10;
	// 
	// SideGroup
	// 
	this->SideGroup->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom)
		| System::Windows::Forms::AnchorStyles::Right));
	this->SideGroup->AutoSize = true;
	this->SideGroup->Controls->Add(this->PageButton);
	this->SideGroup->Controls->Add(this->FilesButton);
	this->SideGroup->Controls->Add(this->DescriptionText);
	this->SideGroup->Controls->Add(this->ActionButton);
	this->SideGroup->Location = System::Drawing::Point(948, 56);
	this->SideGroup->Margin = System::Windows::Forms::Padding(4);
	this->SideGroup->MinimumSize = System::Drawing::Size(358, 533);
	this->SideGroup->Name = L"SideGroup";
	this->SideGroup->Padding = System::Windows::Forms::Padding(14, 12, 14, 12);
	this->SideGroup->Size = System::Drawing::Size(358, 649);
	this->SideGroup->TabIndex = 1;
	this->SideGroup->TabStop = false;
	this->SideGroup->Text = L"Description";
	// 
	// PageButton
	// 
	this->PageButton->Dock = System::Windows::Forms::DockStyle::Bottom;
	this->PageButton->Location = System::Drawing::Point(14, 394);
	this->PageButton->Margin = System::Windows::Forms::Padding(4);
	this->PageButton->Name = L"PageButton";
	this->PageButton->Size = System::Drawing::Size(330, 81);
	this->PageButton->TabIndex = 3;
	this->PageButton->Text = L"View Page";
	this->PageButton->UseVisualStyleBackColor = true;
	// 
	// FilesButton
	// 
	this->FilesButton->Dock = System::Windows::Forms::DockStyle::Bottom;
	this->FilesButton->Location = System::Drawing::Point(14, 475);
	this->FilesButton->Margin = System::Windows::Forms::Padding(4);
	this->FilesButton->Name = L"FilesButton";
	this->FilesButton->Size = System::Drawing::Size(330, 81);
	this->FilesButton->TabIndex = 2;
	this->FilesButton->Text = L"View Files";
	this->FilesButton->UseVisualStyleBackColor = true;
	// 
	// DescriptionText
	// 
	this->DescriptionText->Dock = System::Windows::Forms::DockStyle::Fill;
	this->DescriptionText->Location = System::Drawing::Point(14, 36);
	this->DescriptionText->Margin = System::Windows::Forms::Padding(4, 0, 4, 0);
	this->DescriptionText->Multiline = true;
	this->DescriptionText->Name = L"DescriptionText";
	this->DescriptionText->ReadOnly = true;
	this->DescriptionText->ScrollBars = System::Windows::Forms::ScrollBars::Vertical;
	this->DescriptionText->Size = System::Drawing::Size(330, 520);
	this->DescriptionText->TabIndex = 1;
	this->DescriptionText->TabStop = false;
	this->DescriptionText->Text = L"Description is not available for this workshop item";
	// 
	// ActionButton
	// 
	this->ActionButton->Dock = System::Windows::Forms::DockStyle::Bottom;
	this->ActionButton->Enabled = false;
	this->ActionButton->Location = System::Drawing::Point(14, 556);
	this->ActionButton->Margin = System::Windows::Forms::Padding(4);
	this->ActionButton->Name = L"ActionButton";
	this->ActionButton->Size = System::Drawing::Size(330, 81);
	this->ActionButton->TabIndex = 0;
	this->ActionButton->Text = L"Action";
	this->ActionButton->UseVisualStyleBackColor = true;
	// 
	// LoadingProgress
	// 
	this->LoadingProgress->Dock = System::Windows::Forms::DockStyle::Top;
	this->LoadingProgress->Location = System::Drawing::Point(7, 19);
	this->LoadingProgress->Margin = System::Windows::Forms::Padding(2);
	this->LoadingProgress->Name = L"LoadingProgress";
	this->LoadingProgress->Size = System::Drawing::Size(165, 15);
	this->LoadingProgress->TabIndex = 1;
	// 
	// DescriptionLabel
	// 
	this->DescriptionLabel->AutoSize = true;
	this->DescriptionLabel->Dock = System::Windows::Forms::DockStyle::Fill;
	this->DescriptionLabel->Location = System::Drawing::Point(7, 19);
	this->DescriptionLabel->Margin = System::Windows::Forms::Padding(2, 0, 2, 0);
	this->DescriptionLabel->MaximumSize = System::Drawing::Size(160, 0);
	this->DescriptionLabel->MinimumSize = System::Drawing::Size(160, 0);
	this->DescriptionLabel->Name = L"DescriptionLabel";
	this->DescriptionLabel->Size = System::Drawing::Size(160, 26);
	this->DescriptionLabel->TabIndex = 1;
	this->DescriptionLabel->Text = L"Description is not available for this workshop item";
	// 
	// menuStrip1
	// 
	this->menuStrip1->BackColor = System::Drawing::SystemColors::Control;
	this->menuStrip1->GripMargin = System::Windows::Forms::Padding(2, 2, 0, 2);
	this->menuStrip1->ImageScalingSize = System::Drawing::Size(32, 32);
	this->menuStrip1->Items->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(2) {
		this->toolStripMenuItem1,
			this->toolStripMenuItem2
	});
	this->menuStrip1->Location = System::Drawing::Point(14, 12);
	this->menuStrip1->Name = L"menuStrip1";
	this->menuStrip1->Size = System::Drawing::Size(1296, 40);
	this->menuStrip1->TabIndex = 2;
	this->menuStrip1->Text = L"menuStrip1";
	// 
	// toolStripMenuItem1
	// 
	this->toolStripMenuItem1->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(3) {
		this->toolStripMenuItem3,
			this->dumpAllToolStripMenuItem, this->unitTestToolStripMenuItem
	});
	this->toolStripMenuItem1->Name = L"toolStripMenuItem1";
	this->toolStripMenuItem1->Size = System::Drawing::Size(106, 36);
	this->toolStripMenuItem1->Text = L"Debug";
	// 
	// toolStripMenuItem3
	// 
	this->toolStripMenuItem3->Name = L"toolStripMenuItem3";
	this->toolStripMenuItem3->Size = System::Drawing::Size(286, 44);
	this->toolStripMenuItem3->Text = L"Download all";
	// 
	// dumpAllToolStripMenuItem
	// 
	this->dumpAllToolStripMenuItem->Name = L"dumpAllToolStripMenuItem";
	this->dumpAllToolStripMenuItem->Size = System::Drawing::Size(286, 44);
	this->dumpAllToolStripMenuItem->Text = L"Dump all";
	// 
	// unitTestToolStripMenuItem
	// 
	this->unitTestToolStripMenuItem->Name = L"unitTestToolStripMenuItem";
	this->unitTestToolStripMenuItem->Size = System::Drawing::Size(286, 44);
	this->unitTestToolStripMenuItem->Text = L"Unit test";
	// 
	// toolStripMenuItem2
	// 
	this->toolStripMenuItem2->Name = L"toolStripMenuItem2";
	this->toolStripMenuItem2->Size = System::Drawing::Size(99, 36);
	this->toolStripMenuItem2->Text = L"About";
	// 
	// statusStrip1
	// 
	this->statusStrip1->ImageScalingSize = System::Drawing::Size(32, 32);
	this->statusStrip1->Items->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(1) { this->statusLabel });
	this->statusStrip1->Location = System::Drawing::Point(14, 721);
	this->statusStrip1->Name = L"statusStrip1";
	this->statusStrip1->Size = System::Drawing::Size(1296, 42);
	this->statusStrip1->SizingGrip = false;
	this->statusStrip1->TabIndex = 4;
	this->statusStrip1->Text = L"statusStrip1";
	// 
	// statusLabel
	// 
	this->statusLabel->Name = L"statusLabel";
	this->statusLabel->Size = System::Drawing::Size(1281, 32);
	this->statusLabel->Spring = true;
	this->statusLabel->Text = L"Game path: [not set]";
	this->statusLabel->TextAlign = System::Drawing::ContentAlignment::MiddleLeft;
	// 
	// ManagerForm
	// 
	this->AutoScaleDimensions = System::Drawing::SizeF(12, 25);
	this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
	this->ClientSize = System::Drawing::Size(1324, 775);
	this->Controls->Add(this->statusStrip1);
	this->Controls->Add(this->SideGroup);
	this->Controls->Add(this->ItemsGroup);
	this->Controls->Add(this->menuStrip1);
	this->Icon = (cli::safe_cast<System::Drawing::Icon^>(resources->GetObject(L"$this.Icon")));
	this->KeyPreview = true;
	this->MainMenuStrip = this->menuStrip1;
	this->Margin = System::Windows::Forms::Padding(4);
	this->MinimumSize = System::Drawing::Size(1350, 846);
	this->Name = L"ManagerForm";
	this->Padding = System::Windows::Forms::Padding(14, 12, 14, 12);
	this->SizeGripStyle = System::Windows::Forms::SizeGripStyle::Hide;
	this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
	this->Text = L"Street Legal Workshop Installer";
	this->ItemsGroup->ResumeLayout(false);
	(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->ItemsDataGrid))->EndInit();
	this->SideGroup->ResumeLayout(false);
	this->SideGroup->PerformLayout();
	this->menuStrip1->ResumeLayout(false);
	this->menuStrip1->PerformLayout();
	this->statusStrip1->ResumeLayout(false);
	this->statusStrip1->PerformLayout();
	this->ResumeLayout(false);
	this->PerformLayout();

}