#pragma once

#include <atomic>

//ManagerForm observes this
struct UIState
{
	static std::atomic<bool> NeedsActionButtonUpdate; //update action button only
	static std::atomic<bool> NeedsDescriptionUpdate; //update item description
	static std::atomic<bool> NeedsDataGridUpdate; //update the grid
	static std::atomic<bool> NeedsGamePathLabelUpdate; //update game path label under the datagrid
	static std::atomic<bool> NeedsFullUpdate; //update the entire form
};