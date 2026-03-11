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

The uploader accepts parameters from two sources:

1. Command-line parameters
You can run the uploader from cmd.exe or a .bat file and provide parameters directly.

Example:
workshop_uploader -mode create -title "My workshop item"

Both short and verbose forms are supported:

-m		or	-mode		(operation mode)
-t		or	-title		(item title)

2. Configuration file
Parameters can also be defined in the file: workshop_uploader_config.ini
In the this file, parameter names are written without the leading dash (-):

mode = create
title = My workshop item
preview = workshop_preview.jpg

Parameter priority:

If both sources are present, the uploader uses the following priority:

	1. Configuration file parameters
	2. Command-line parameters
	
If the configuration file contains at least one parameter, command-line parameters are ignored.
If the file has no parameters, the uploader automatically uses the command-line parameters instead.

Parameter list:

-mode: (mode, -m)
uploader mode, accepted values are "create", "modify", "delete", "info";

-item-id: (item-id, -id)
workshop item ID (unsigned 64-bit integer);

-title: (title, -t)
item title (max 128 bytes);

-description: (description, -d)
item description (max 8192 bytes);

-visibility: (visibility, -v)
"public", "private", "friends", "unlisted";

-category: (category, -c)
comma-separated category list;

-preview: (preview, -p)
relative path to the item preview image file, JPG or PNG, max 1024 kb;

-screenshots: (screenshots, -sc)
relative path to the folder containing screenshots (max 50 screenshots, JPG or PNG, max 1024 kb per file);

-video-urls: (video-urls, -yt)
comma-separated YouTube video URL's list;

-content: (content, -f)
relative path to the content directory;

-comment: (comment, -uc)
release notes (modify mode only);

-no-confirm:  (no-confirm, -nc)
skip confirmation prompt (true/false);

-no-wait: (no-wait, -nw)
skip "Press any key" on exit (true/false);

-create-defaults: (create-defaults, -cdf)
this option is only for create mode; automatic correction of missing or invalid workshop item fields before submission.

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

Additional options:

This option is available only in create mode:

-create-defaults "true"

When enabled, the uploader applies an auto-correct policy that fills in missing parameters with default values.
This allows creating a workshop item even if some parameters were not explicitly provided.
If this option is disabled, all required parameters must be supplied by the user.

Modify mode:

In modify mode, the uploader supports partial updates.
You may specify only the parameters that need to be changed. All other item properties remain unchanged.

Example:

-mode modify
-title "Updated title"

In this example only the item title will be updated.
Warnings about parameters that are not set are suppressed in modify mode, since missing values are expected when performing partial updates.