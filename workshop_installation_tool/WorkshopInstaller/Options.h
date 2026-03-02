#pragma once

//hardcoded options
#define USE_STEAM_INSTALL_INFO //this tells us whether or not we use information provided by SteamUGC()->GetItemInstallInfo()
#define DUMP_LOGS //print debug logs
#define USE_MUTEX //if set, only one instance of the installer is allowed to run
#define USE_FICTIVE_JVM_CLASS_FILES //generates respective 0kb .class files if the item only has .java files
//#define FREEZE_DATAGRID_ON_REFRESH //locks datagrid on RefreshDataGrid()

#define CONFIG_FILE_MASK "workshop_installer_config.ini"

//user adujstable options
extern bool g_EnableOverwrittenColumn;
extern bool g_PromptUserOnManageItem;
extern bool g_UseFictiveJavaFiles;
extern bool g_UseFictiveClassFiles;

//do NOT use namespace std here! all CLI declarations stay fully qualified

#ifndef _DEBUG
//disable warning in release build:
//'ex': unreferenced local variable
#pragma warning(disable : 4101)
#endif

inline System::String^ GetConfigFilePath()
{
	return System::IO::Path::Combine(System::AppDomain::CurrentDomain->BaseDirectory, CONFIG_FILE_MASK);
}

inline bool ReadConfigValue(System::String^ key, System::String^% outValue)
{
	outValue = nullptr;

	if (System::String::IsNullOrWhiteSpace(CONFIG_FILE_MASK) ||
		System::String::IsNullOrWhiteSpace(key) ||
		!System::IO::File::Exists(GetConfigFilePath()))
		return false;

	cli::array<System::String^>^ lines;

	try
	{
		lines = System::IO::File::ReadAllLines(CONFIG_FILE_MASK, System::Text::Encoding::UTF8);
	}
	catch (System::Exception^ ex)
	{
#ifdef _DEBUG
		throw gcnew System::Exception("ReadConfigValue exception: " + ex->Message);
#endif

		return false;
	}

	for each (System::String^ rawLine in lines)
	{
		if (System::String::IsNullOrWhiteSpace(rawLine))
			continue;

		System::String^ line = rawLine->Trim();

		//comments
		if (line->StartsWith("#") || line->StartsWith(";"))
			continue;

		int eq = line->IndexOf('=');
		if (eq <= 0)
			continue;

		System::String^ k = line->Substring(0, eq)->Trim();
		if (!k->Equals(key, System::StringComparison::OrdinalIgnoreCase))
			continue;

		System::String^ v = line->Substring(eq + 1)->Trim();

		//strip quotes
		if (v->Length >= 2 &&
			((v->StartsWith("\"") && v->EndsWith("\"")) ||
				(v->StartsWith("'") && v->EndsWith("'"))))
		{
			v = v->Substring(1, v->Length - 2);
		}

		outValue = v;
		return true;
	}

	return false;
}

inline uint64 ReadConfigUInt64(System::String^ key, uint64 defaultValue)
{
	System::String^ value;
	if (!ReadConfigValue(key, value))
		return defaultValue;

	System::UInt64 result;
	if (System::UInt64::TryParse(value, result))
		return result;

	return defaultValue;
}

inline int ReadConfigInt(System::String^ key, int defaultValue)
{
	System::String^ value;
	if (!ReadConfigValue(key, value))
		return defaultValue;

	int result;
	if (System::Int32::TryParse(value, result))
		return result;

	return defaultValue;
}

inline bool ReadConfigBool(System::String^ key, bool defaultValue)
{
	System::String^ value;
	if (!ReadConfigValue(key, value))
		return defaultValue;

	if (value->Equals("1") || value->Equals("true", System::StringComparison::OrdinalIgnoreCase))
		return true;

	if (value->Equals("0") || value->Equals("false", System::StringComparison::OrdinalIgnoreCase))
		return false;

	return defaultValue;
}

inline System::String^ ReadConfigString(System::String^ key, System::String^ defaultValue)
{
	System::String^ value;
	if (!ReadConfigValue(key, value))
		return defaultValue;

	return value;
}