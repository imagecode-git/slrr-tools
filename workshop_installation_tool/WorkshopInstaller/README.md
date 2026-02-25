============================================

ImageCode workshop installer

Copyright ImageCode LLC.

This tool is licensed under the MIT License.
See the LICENCE.md file for details.

============================================

GUI tool for installing and removing Steam workshop items for:

Street Legal Racing: Redline (Steam App ID 497180)

Street Legal 1: REVision (Steam App ID 1571280)

What this tool can do?

- View all subscribed workshop items;
- Install workshop items into the game directory;
- Remove installed workshop items safely;
- Detect file conflicts between installed items;
- Restore original files using automatic backup and uninstall logs;
- Recover from interrupted installations when possible.

The installer operates directly on the game directory and maintains uninstall logs for every installed item.

How it works:

1. The installer reads the Steam App ID from steam_appid.txt. It queries Steam for subscribed Workshop items and
resolves each item’s cached installation directory.

2. The installer attempts to locate the game directory by:

- Checking its own directory;
- Querying Steam for install location;
- Asking the user to manually select the directory (if necessary).

A valid game directory must contain:

- File system.rpk;
- Recognizable executable names (StreetLegal_Redline, SL_REVision, etc.).

3. Installation: files are copied from the Steam Workshop cache to the game directory. Existing files are backed up before being replaced.
A per-item uninstall log is created. A sentinel file is created during installation to detect interruptions.

4. Uninstallation: The uninstall log is read, installed files are removed, backups are restored, empty directories are cleaned up,
uninstall log and sentinel files are removed.

Conflict detection:

1. Before installation, the installer:

- Builds an ownership map of installed files;
- Detects whether files to be installed overwrite files from other Workshop items;
- Displays a conflict dialog listing affected items and files.

2. During uninstall, the installer:

- Prevents removal if other items depend on the same files;
- Displays blocking items if dynamic ownership is detected;
- Optional configuration enables an “Overwritten” indicator column in the UI.

Backup system:

Backups are created automatically before overwriting files.

Backup types:

Simple backups
file.ext -> file.ext.bak<itemId>

Centralized backups (special folders)
music/ and save/ files are redirected to dedicated backup subfolders:

music/_music.bak<itemId>/...

save/_save.bak<itemId>/...

Backups are restored during uninstall.

JVM Files Handling:

Special handling applies to:

- .java files
- .class files

The installer:

- Backs up original JVM-related files;
- Removes stale .class files when replacing .java sources;
- Ensures consistency between source and compiled files.

This prevents desynchronization between script sources and compiled bytecode.

Interrupted Install Recovery:

If installation is interrupted (e.g., crash, forced termination):

- A sentinel file remains in the uninstall directory;
- On next launch, the installer detects the interrupted state;
- The installation is verified;
- Broken installations are marked and can be reinstalled.

Requirements:

- Steam must be running;
- User must be logged in;
- The game must not be running during install/remove operations;
- Microsoft Visual C++ v140 runtime may be required.

Limitations:

- Non-latin characters in file paths may cause unexpected behavior;
- If Windows Explorer or another process locks a directory, uninstall may fail;
- Installation is currently not fully atomic (delete + copy operations are used);
- Workshop uninstall logs are shared across Steam users on the same system.

Configuration options:

The installer reads configuration options from the workshop_installer_config.ini file:

- enable_overwritten_column (true/false): toggles "O" column (slow);
- user_prompt_on_manage_item (true/false): toggles user prompt before install/remove item;
- game_path (string): forces custom game path.

Safety recommendations:

- Do not interrupt installation while files are being copied;
- Close the game before installing or removing items;
- Avoid manually modifying uninstall logs or deleting backup files;
- Keep Steam workshop cache intact.