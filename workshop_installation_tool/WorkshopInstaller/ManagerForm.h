#pragma once

#define COL_SUBSCRIBED	0
#define COL_INSTALLED	1
#define COL_OVERWRITTEN	2
#define COL_ID			3
#define COL_TITLE		4
#define COL_AUTHOR		5
#define COL_SIZE		6

#define STEAM_TIMER_FREQ 50 //50Hz is plenty

namespace WorkshopInstaller
{
	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	enum class GridSortColumn
	{
		Id,
		Title,
		Size,
		Installed,
		Subscribed
	};

	value struct GridSortState
	{
		GridSortColumn Column;
		ListSortDirection Direction;
	};

	enum class ActionButtonState
	{
		None,
		Downloading,
		Installable,
		Removable,
		Disabled,
		Broken
	};

	public ref class ManagerForm : public System::Windows::Forms::Form
	{
	public:
		static ManagerForm^ GetInstance()
		{
			if (!instance) instance = gcnew ManagerForm();
			return instance;
		}

		ManagerForm(void)
		{
			InitializeComponent();
			InitializeForm();
		}

		void InitializeForm(void);
		
		void RefreshForm(void);
		void RefreshActionButton(void);
		void RefreshDescription(void);
		void RefreshDataGrid();
		void RefreshGamePathLabel();
		
		void SortDataGridItems();
		void UpdateSortStateFromColumn(DataGridViewColumn^ col, ListSortDirection dir);

		//deferred actions
		void SelectRowByIndex(int rowIndex);
		void SelectRowByItemId(UInt64 itemId);
		void FocusDataGrid(void);

		void SetStatusText(String^ text);

	private: System::Windows::Forms::DataGridViewTextBoxColumn^ SubscribedColumn;
	private: System::Windows::Forms::DataGridViewTextBoxColumn^ InstalledColumn;
	private: System::Windows::Forms::DataGridViewTextBoxColumn^ OverwrittenColumn;
	private: System::Windows::Forms::DataGridViewTextBoxColumn^ IdColumn;
	private: System::Windows::Forms::DataGridViewTextBoxColumn^ TitleColumn;
	private: System::Windows::Forms::DataGridViewTextBoxColumn^ AuthorColumn;
	private: System::Windows::Forms::DataGridViewTextBoxColumn^ SizeColumn;
	private: System::Windows::Forms::ToolStripMenuItem^ unitTestToolStripMenuItem;
	public:
		void ForceForeground(void);
		void CaptureCurrentSortState(void);

		UInt64 GetSelectedItemIdFromDataGrid(); //returns itemId of the workshop item selected in the DataGridView table
		UInt64 GetItemIdFromRow(int rowIndex);

		void DataGridView_RowEnter(Object^ sender, DataGridViewCellEventArgs^ e);
		void DataGridView_SortCompare(Object^ sender, DataGridViewSortCompareEventArgs^ e);
		void DataGridView_ColumnHeaderMouseClick(Object^ sender, DataGridViewCellMouseEventArgs^ e);






	public:
		void ActionButton_Click(Object^ sender, EventArgs^ e);
		void FilesButton_Click(Object^ sender, EventArgs^ e);
		void PageButton_Click(Object^ sender, EventArgs^ e);
		void OnManagerFormShown(Object^ sender, EventArgs^ e);
		void OnManagerFormClosed(Object^ sender, FormClosedEventArgs^ e);

		void aboutToolStripMenuItem_Click(Object^ sender, EventArgs^ e);
		void downloadAllToolStripMenuItem_Click(Object^ sender, EventArgs^ e);
		void dumpAllToolStripMenuItem_Click(Object^ sender, EventArgs^ e);
		void unitTestToolStripMenuItem_Click(Object^ sender, EventArgs^ e);
		
		void OnKeyDown(Object^ sender, KeyEventArgs^ e); //runs in debug build only

		void OnSteamTick(Object^ sender, EventArgs^ e);

	protected:
		~ManagerForm()
		{
			if (components)
			{
				delete components;
			}
		}

	private:
		void InitializeComponent();

		static ManagerForm^ instance;
		UInt64 m_selectedWorkshopItemId;
		bool m_bFormInitialized;
		bool m_initialSortApplied;
		bool m_bUpdatingGrid;
		Timer^ m_SteamTimer;
		GridSortState m_sortState;

		ActionButtonState ResolveActionButtonState(UInt64 itemId);
		void ApplyActionButtonState(ActionButtonState state);

	private: System::Windows::Forms::GroupBox^ ItemsGroup;
	private: System::Windows::Forms::GroupBox^ SideGroup;
	private: System::Windows::Forms::Button^ ActionButton;
	private: System::Windows::Forms::Label^ DescriptionLabel;
	private: System::Windows::Forms::TextBox^ DescriptionText;
	private: System::Windows::Forms::DataGridView^ ItemsDataGrid;
	private: System::Windows::Forms::Button^ PageButton;
	private: System::Windows::Forms::Button^ FilesButton;

	private: System::Windows::Forms::Label^ LoadingLabel;
	private: System::Windows::Forms::ProgressBar^ LoadingProgress;
	private: System::Windows::Forms::StatusStrip^ statusStrip1;
	private: System::Windows::Forms::ToolStripStatusLabel^ statusLabel;
	private: System::Windows::Forms::MenuStrip^ menuStrip1;
	private: System::Windows::Forms::ToolStripMenuItem^ toolStripMenuItem1;
	private: System::Windows::Forms::ToolStripMenuItem^ toolStripMenuItem3;
	private: System::Windows::Forms::ToolStripMenuItem^ toolStripMenuItem2;
		   System::ComponentModel::Container^ components;
	private: System::Windows::Forms::ToolStripMenuItem^ dumpAllToolStripMenuItem;
};
}
