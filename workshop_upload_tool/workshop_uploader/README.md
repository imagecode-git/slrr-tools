============================================

ImageCode workshop upload tool

Copyright ImageCode LLC.

This tool is licensed under the MIT License.
See the LICENCE.md file for details.

============================================

Command-line tool for creating, modifying and deleting Steam workshop items for:

Street Legal Racing: Redline (Steam App ID 497180)

Street Legal 1: REVision (Steam App ID 1571280)

How to use the tool?

Use .bat files to run the tool or explore examples to see how it works:

1. example_create.bat - shows how to create workshop item
2. example_modify.bat - here you can discover how to modify your workshop item
3. example_delete.bat - tell you how to delete your workshop item
4. example_info.bat - in this example the uploader runs in the info mode
5. example_options.bat - the last example that you should explore, shows available options of the uploader

These .bat files can be modified with text editors (for example, Notepad++ or Windows Notepad).

Example command:

workshop_uploader ^
-mode "create" ^
-title "My Car Mod" ^
-description "Improved suspension, better collision meshes." ^
-visibility "public" ^
-category "car" ^
-preview "preview.jpg" ^
-screenshots "screenshots_folder" ^
-video-urls "https://www.youtube.com/watch?v=VIDEO_ID" ^
-content "content" ^
-comment "Initial release"

Syntax is: workshop_uploader -parameter "value"

Rules:

- Token "workshop_uploader" is the name of the workshop_uploader.exe and it always goes first;
- Every parameter requires a value;
- The ^ symbol is a new line indicator - it tells the uploader that another command line is expected below;
- Values must be enclosed in double quotes;
- Multiple values must be separated using the comma symbol.

Parameters:

-mode: uploader mode, accepted values are "create", "modify", "delete", "info";
-item-id: workshop item ID (unsigned 64-bit integer);
-title: item title (max 128 bytes);
-description: item description (max 8192 bytes);
-visibility: "public", "private", "friends";
-category: comma-separated category list;
-preview: relative path to the item preview image file, JPG or PNG, max 1024 kb;
-screenshots: relative path to the folder containing screenshots (max 50 screenshots, JPG or PNG, max 1024 kb per file);
-content: relative path to the content directory;
-comment: release notes (modify mode only);
-no-confirm: skip confirmation prompt (true/false);
-no-wait: skip "Press any key" on exit (true/false);
-auto-defaults: automatic correction of missing or invalid workshop item fields before submission.

Important: It's NOT recommended to use non-latin symbols in file or folder names, application may not identify them or just crash.
Use only english names to avoid errors. Also make sure that application has no limitations on connecting to the internet before run.
If you can't upload files, disable your anti-virus software and firewalls.

Not allowed in create mode:

- Empty or non-existent content path;
- Non-existent or missing preview image.

Categories:

- Multiple categories are allowed;
- Use comma symbol to separate the categories.

YouTube URL's:

- Accepted format is https://www.youtube.com/watch?v=VIDEO_ID;
- The uploader extracts and submits only the 11-character video ID;
- Invalid or malformed URLs may result in empty video entries and validation warnings;
- Multiple URL's are allowed (separated by comma symbols).

Visibility:

- If not explicitly set, defaults to "private";
- Use "public" to make your workshop item visible to anyone.

Interactive selection:

- If the -item-id parameter is omitted while using modify or delete modes, all published items are listed;
- The interactive selection feature supports pagination if you have 50+ workshop items published for a single game.

Pagination controls:

- Next page: [n] key;
- Previous page: [p] key;
- Select item: [s] key;
- Quit: [q] key.

Default values:

- The uploader uses auto-correct item validation policy;
- For instance, if no item title is set, the default one "Workshop item" is applied;
- You should override parameter values before submission.

Interruption handling:

- The uploader captures the Ctrl+C termination event, it stops active async tasks and displays error message;
- Do not interrupt during content upload or update operations.

Automation:

Disable interactive prompts and "Press any key" to use the uploader in a scripted execution:

-no-confirm "true"
-no-wait "true"

