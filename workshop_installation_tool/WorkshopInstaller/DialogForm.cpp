#include "DialogForm.h"
#include "Helpers.h"

using namespace WorkshopInstaller;

DialogForm::DialogForm()
{
	InitializeComponent();

	ActionButton->Text	= LOC_DIALOG_ACTION_OK;
	CancelButton->Text	= LOC_DIALOG_ACTION_CANCEL;
}

void DialogForm::InitializeForm(void)
{
}

void DialogForm::SetButtonLayout(DialogButtons newLayout)
{
	switch (newLayout)
	{
	case DialogButtons::OK:
		CancelButton->Visible = false;
		CancelButton->Enabled = false;
		CancelButton->Dock = DockStyle::None; //this will collapse the container so we don't see empty space after the button
		break;

	case DialogButtons::OKCancel:
		CancelButton->Visible = true;
		CancelButton->Enabled = true;
		CancelButton->Dock = DockStyle::Right;
		break;
	}
}

void DialogForm::SetHeading(String^ text)
{
	TextStart->Text = text;
}

void DialogForm::SetDescription(String^ text)
{
	TextMain->Text = text;
}

void DialogForm::SetFooter(String^ text)
{
	TextEnd->Text = text;
}