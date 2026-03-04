#pragma once
#include "Main.h"
#include "Locale.h"

//DEBUG CHECK:
//1. set wrong SteamID in steam_appid.txt [OK]
//2. remove steam_appid.txt [OK]
//3. subscribe to 100 items and try to break loading by disconnecting from the internet in the middle of loading process [OK]
//4. simulate missing Steam callbacks [OK]
//5. simulate slow network [OK] or no network [OK]
//6. check sorting in the DataGridView [OK]
//7. file exceptions during install/uninstall [OK]
//8. install mods that only contain .java [OK]
//9. install mods that only contain .class (better to create them artifically) [OK]
//10. install mods that contain both .java/.class (artificial items are best) [OK]
//11. install conflicting mods [OK]
//12. install mods that replace music or save folder [OK]
//13. install mod A, then install B overwriting A, try uninstalling A. expected: cannot uninstall A, can uninstall B [OK]
//14. install mod, then manually delete installed file, run uninstall: must not crash [OK]
//15. install mod with a deep directory tree (6-10 levels deep, better to make an artificial one) [OK]
//16. subscribe to item, install, then unsubscribe: must appear in the table [OK]
//17. install heavy mod, something over 100MB in size [OK]
//18. put into SL1 folder and see where mods will be installed into [OK]
//19. brute-force ItemDataGrid, must never crash! [OK]
//20. brute-force loading items, they must not hang! [OK]
//21. edge case: set game dir to C:\, installer should keep shooting exceptions or do other crazy things [OK]
//22. create a fake installed item and try to uninstall it [OK]
//23. simulate mid-installation crash, the installer may think that the item has been installed [OK]
//24. install a mod that has javas with sources, respective classes and classes with NO sources [OK]
//25. install a mod that will replace system folder and all classes inside [OK]
//26. install SL1 mod [OK]
//27. simulate wrong item cache path issue and try installing/uninstalling items [OK]
//28. try to break game paths and perform any normal operation to create edge cases like leftover files or fake install/uninstall [OK]
//29. check various edge cases of VerifyInstallation() [OK]
//30. check if sentinel file remains undeleted under some circumstances [OK]
//31. simulate various install/uninstall corruptions or interruptions [OK]
//32. check multiple instances running [OK]
//33. try to rename Steam dir and run from game dir, try renaming SL2 exe [OK]
//34. try to rename Steam dir and run with SL1 [OK]
//35. force download on broken item [OK]
//36. simulate unsubscribed and broken item (uninstall file with random numbers + sentinel file) [OK]
//37. simulate broken item with incorrect name (garbage name + sentinel file) [OK]
//38. simulate broken and subscribed item (uninstall file of a subscribed item + sentinel file) [OK]
//39. simulate uninstall log with a unicode name to crash the installer [OK]
//40. unsubscribe from all items, run with no installed items (empty grid test) [OK]
//41. unsubscribe from all items, add several installed items [OK]
//42. simulate read-only sentinel file [OK]
//43. simulate orphan sentinel file [OK]
//44. simulate uninstall log + sentinel file pair with a mismatching ITEM key in the sentinel file [OK]
//45. simulate sentinel file with a garbage ITEM key [OK]
//46. subscribe to item, install it, unsubscribe, restart the installer, run VerifyInstallation(), must return true [OK]
//47. subscribe to item, get a broken install (access violation), unsubscribe, restart the installer, run VerifyInstallation(), metadata must be preserved [OK]
//48. install/remove unsubscribed items with sorting applied, try all columns, scrolling must not break, no edge cases must occur [OK]
//49. datagrid must not break on refresh after sorting [OK]
//50. simulate complex file ownership graphs (install item A, then conflicting item B, then conflicting items C, D, E), then try uninstalling items A, B, C and D in random order
//51. install an item that only has .class files and is replacing files with existing .java sources - installer must create their backups [OK]
//52. install an item that has .java sources, but missing their .class counterparts - installer must create fictive 0kb .class files if legacy option is enabled [OK]
//53. create a synthetic workshop item that will create folder X and file Y inside. go to folder X, then try to remove the item [OK]

//how to use the debugger:
//1. in VS switch to debug configuration to access the debugger
//2. "Unit test" button runs tests from enum class UnitTest, use Ctrl+Left or Ctrl+Right to cycle between tests (you'll see their names in status bar)
//3. use DUMP_LOGS in Options.h to print logs to Debug.log
//4. Debug.log resides either next to the installer or in the game path, depending on user configuration
//5. you might see some warning message boxes that only appear in the debug mode, that's fine

//known issues:
//1. syncing subscribed items could be slow sometimes, seems to be Steam UGC limitation
//2. GetGamePath() may cause freeze in some edge cases

public enum class UnitTest //public to allow .ToString()
{
	SimulateNotCachedItem,
	SimulateBrokenItem,
	SimulateNotSubscribedItem,
	SimulateEmptyDataGrid,
	RunVerifyInstallation,
	RunRefreshDataGrid,
	RunGetItemCachePath,
	RunBuildFileOwnerMap,
	RunIsItemMarkedOverwritten,
	CheckFileConflicts,
	_count //sentinel value that indicates the last enum
};

extern UnitTest g_SelectedUnitTest;

void RunUnitTest(UnitTest testId);
void CycleUnitTest(int delta);
void SetStatus(String^ text);