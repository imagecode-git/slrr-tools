#include "InstallForm.h"
#include "Helpers.h"

using namespace WorkshopInstaller;

void InstallForm::LoadForm()
{
	this->Show();
	this->Update();
}

void InstallForm::UnloadForm()
{
	this->Hide();
}