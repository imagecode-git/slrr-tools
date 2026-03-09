#include "stdafx.h"
#include "Options.h"

/*
 * DEBUG TEST:
 *
 * 1. exe: dry run (no cmd, display help info) [OK]
 * 2. bat: partial cmd params [OK]
 * 3. bat: no cmd params [OK]
 * 4. create item -> success [OK]
 * 5. create item -> interrupted by user [OK]
 * 6. create item -> Alt+F4 (external interruption simulation) [OK]
 * 7. create item -> error/warning [OK]
 * 8. edit item -> no params (uploader must list all items and ask for prompt) [OK]
 * 9. edit item -> success [OK]
 * 10. edit item -> error/warning [OK]
 * 11. edit item -> interrupted by user [OK]
 * 12. edit item -> Alt+F4 [OK]
 * 13. edit item -> no params [OK]
 * 14. delete item -> success [OK]
 * 15. delete item -> interrupted by user [OK]
 * 16. delete item -> Alt+F4 [OK]
 * 17. delete item -> error/warning [OK]
 * 18. delete item -> no params [OK]
 * 19. run with Steam disabled
 * 20. cut internet while running [FAIL] - hangs on "Uploading images..." when creating new item, leaves a garbage broken item
 * 21. invalid params [OK]
 * 22. test with SL1 [OK]
 * 23. debug prints while debug.log is read-only
 * 24. itemId is set, but the item is non-existent (mode modify, itemId 3628393889) [OK]
 * 25. itemId is set to 0 via config [OK]
 * 26. delete all published items and try to call the list [OK]
 * 27. mutex must not allow multiple app executions [OK]
 * 28. try to upload 50+ screenshots [OK]
 * 29. try to use a 8kb+ description via cmd - can't run via cmd (8191 chars Windows CLI constraint) [OK]
 * 30. simulate a 8kb+ description via code
 * 31. 5kb description [OK]
 * 32. description with special characters
 * 33. chinese/cyrillic description [FAIL] -> config parser is required instead of argc/argv, data gets corrupted before coming into the uploader
 * 34. check -no-confirm parameter [OK]
 * 35. try to break ParseBool() with incorrect params [OK]
 * 36. make sure exit from the program is the same under any circumstances [OK]
 * 37. delete non-existing predefined itemId 3669194643 [OK]
 * 38. negative itemId in cmd config [OK]
 * 39. test uppercase, lowercase or mixed case input for ParseBool() [OK]
 * 40. test whitespaces against ParseBool() and ParseUint64() [OK]
 * 41. create item that has 2MB+ images in the screenshots folder [OK]
 * 42. go to pagination ("Enter key:") and press Ctrl+C [OK]
 * 43. unit tests for helper functions [OK]
 * 44. test pagination in all uploader modes [OK]
 * 45. spam create 50 items stress test [OK]
 * 46. spam delete 50 items stress test [OK]
 * 47. info mode no pagination [OK]
 * 48. upload under a blackhole internet connection (silent packet loss) [OK]
 */

/*
MODE CHECK:
1. create mode, -create-defaults OFF

- example bat [OK]
- comments set [OK]
- comments empty [OK]
- empty categories [OK]
- empty params [OK]
- title only [OK]
- content only [OK]
- content only empty [OK]
- content + preview [OK]
- preview + content empty [OK]
- visibility not set [OK] -> defaults to hidden
- screenshots not set [OK] -> did not brick the uploader
- videos not set [OK]

2. create mode, -create-defaults ON

- example bat [OK]
- empty params [OK]
- preview not set [OK]
- content not set [OK]
- videos not set [OK]
- screenshots not set [OK]
- comment not set [OK]
- title not set [OK]
- empty categories [OK]
- visibility not set [OK]
- description not set [OK]
- only strict params set [OK]
- strict params + screenshots [OK]
- strict params + title + description [OK]

3. test visibility "unlisted" [OK]

4. test incorrect visibility [OK]

5. modify mode

- title only [OK]
- preview only [OK]
- screenshots only [OK]
- videos only [OK]
- mixed media: screenshots + videos [OK]
- only mode is set, no data to change [OK]
- description only [OK]
- categories only [OK]
- visibility only [OK]
- content only [OK]
- preview only [OK]
- preview + screenshots [OK]
- preview + videos [OK]
- preview + screenshots + videos [OK]
- comment only [OK] no change since comment only
- content + comment [OK]
- videos + comment [OK] no content change -> Steam ignores comment
- pagination [OK]

6. delete mode

 - auto-defaults OFF [OK]
 - auto-defaults ON [OK]
 - pagination [OK]
 - id is set [OK]
 - id not set [OK]
 - screenshots + preview [OK]
 - random params payload [OK]
 - mode only [OK]
 
7. info mode
 
 - auto-defaults OFF [OK]
 - auto-defaults ON [OK]
 - pagination [OK]
 - random params payload [OK]
*/

#ifdef RUN_TESTS
#include <windows.h>
#include <cstdio>
#include <string>
#include "Helpers.h"

#define VERIFY_EQ(actual, expected) \
    do { \
        auto a = (actual); \
        auto e = (expected); \
        std::cout << "Input:    " << #actual << "\n"; \
        std::cout << "Expected: " << e << "\n"; \
        std::cout << "Actual:   " << a << "\n\n"; \
        if (a != e) { \
            PrintMessage("FAIL\n", ConsoleTextColor::Red); \
        } else { \
            PrintMessage("OK\n", ConsoleTextColor::Green); \
        } \
        PrintMessage(""); \
    } while(0)

#define VERIFY_BOOL(actual, expected) \
    do { \
        bool a = (actual); \
        bool e = (expected); \
        std::cout << "Input:    " << #actual << "\n"; \
        std::cout << "Expected: " << (e ? "true" : "false") << "\n"; \
        std::cout << "Actual:   " << (a ? "true" : "false") << "\n\n"; \
        if (a != e) { \
            PrintMessage("FAIL\n", ConsoleTextColor::Red); \
        } else { \
            PrintMessage("OK\n", ConsoleTextColor::Green); \
        } \
        PrintMessage(""); \
    } while(0)

#define VERIFY_UINT64(input, expectedSuccess, expectedValue) \
    do { \
        uint64 result = 0; \
        bool success = ParseUint64(input, result); \
        std::cout << "Input:    \"" << input << "\"\n"; \
        std::cout << "Expected:    \"" << expectedValue << "\"\n"; \
        if (success) \
            std::cout << "Value:    " << result << "\n"; \
        std::cout << "Success:  " << (success ? "true" : "false") << "\n"; \
        std::cout << "\n"; \
        if (success != expectedSuccess || \
           (success && result != expectedValue)) { \
            PrintMessage("FAIL\n", ConsoleTextColor::Red); \
        } else { \
            PrintMessage("OK\n", ConsoleTextColor::Green); \
        } \
        PrintMessage(""); \
    } while(0)

#define GROUP_ID(id) \
    do { \
        PrintMessage(""); \
        WarningMessage("===== TEST GROUP: " + std::string(id) + " ====="); \
        PrintMessage(""); \
    } while(0)

static void Test_ParseVideoUrl()
{
    GROUP_ID("standard watch variations");
    VERIFY_EQ(ParseVideoUrl("http://www.youtube.com/watch?v=dQw4w9WgXcQ"), "dQw4w9WgXcQ");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?v=dQw4w9WgXcQ"), "dQw4w9WgXcQ");
    VERIFY_EQ(ParseVideoUrl("youtube.com/watch?v=dQw4w9WgXcQ"), "dQw4w9WgXcQ");

    GROUP_ID("short URL variations");
    VERIFY_EQ(ParseVideoUrl("http://youtu.be/dQw4w9WgXcQ"), "dQw4w9WgXcQ");
    VERIFY_EQ(ParseVideoUrl("youtu.be/dQw4w9WgXcQ?t=43"), "dQw4w9WgXcQ");

    GROUP_ID("additional parameters");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?feature=youtu.be&v=dQw4w9WgXcQ"), "dQw4w9WgXcQ");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?v=dQw4w9WgXcQ&list=PL123"), "dQw4w9WgXcQ");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?v=dQw4w9WgXcQ#t=10"), "dQw4w9WgXcQ");

    GROUP_ID("subdomain variants");
    VERIFY_EQ(ParseVideoUrl("https://m.youtube.com/watch?v=dQw4w9WgXcQ"), "dQw4w9WgXcQ");
    VERIFY_EQ(ParseVideoUrl("https://music.youtube.com/watch?v=dQw4w9WgXcQ"), "dQw4w9WgXcQ");

    GROUP_ID("invalid length edge cases");
    VERIFY_EQ(ParseVideoUrl("https://youtu.be/dQw4w9WgXc"), "");
    VERIFY_EQ(ParseVideoUrl("https://youtu.be/dQw4w9WgXcQQ"), "");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?v=dQw4w9WgXc"), "");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?v=dQw4w9WgXcQQ"), "");

    GROUP_ID("invalid characters");
    VERIFY_EQ(ParseVideoUrl("https://youtu.be/dQw4w9WgXc!"), "");
    VERIFY_EQ(ParseVideoUrl("https://youtu.be/dQw4w9WgXc*"), "");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?v=dQw4w9WgXc$"), "");

    GROUP_ID("multiple v= parameters");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?v=AAAAAAAAAAA&v=dQw4w9WgXcQ"), "AAAAAAAAAAA");

    GROUP_ID("embedded URL variants");
    VERIFY_EQ(ParseVideoUrl("text before https://youtu.be/dQw4w9WgXcQ text after"), "dQw4w9WgXcQ");
    VERIFY_EQ(ParseVideoUrl("url=https://youtube.com/watch?v=dQw4w9WgXcQ&x=1"), "dQw4w9WgXcQ");

    GROUP_ID("broken inputs");
    VERIFY_EQ(ParseVideoUrl(""), "");
    VERIFY_EQ(ParseVideoUrl("youtube"), "");
    VERIFY_EQ(ParseVideoUrl("watch?v="), "");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?x=1&y=2"), "");
    VERIFY_EQ(ParseVideoUrl("https://youtu.be/"), "");

    //these are hard to pass
    GROUP_ID("trailing garbage after ID");
    VERIFY_EQ(ParseVideoUrl("https://youtu.be/dQw4w9WgXcQextra"), "");
    VERIFY_EQ(ParseVideoUrl("https://youtube.com/watch?v=dQw4w9WgXcQextra"), "");

    GROUP_ID("case sensitivity check");
    VERIFY_EQ(ParseVideoUrl("HTTPS://YOUTU.BE/dQw4w9WgXcQ"), "dQw4w9WgXcQ");
}

static void Test_ParseBool()
{
    GROUP_ID("true literals");
    VERIFY_BOOL(ParseBool("true"), true);
    VERIFY_BOOL(ParseBool("TRUE"), true);
    VERIFY_BOOL(ParseBool("True"), true);
    VERIFY_BOOL(ParseBool("  true  "), true);
    VERIFY_BOOL(ParseBool("1"), true);

    GROUP_ID("false literals");
    VERIFY_BOOL(ParseBool("false"), false);
    VERIFY_BOOL(ParseBool("FALSE"), false);
    VERIFY_BOOL(ParseBool("False"), false);
    VERIFY_BOOL(ParseBool("  false  "), false);
    VERIFY_BOOL(ParseBool("0"), false);

    GROUP_ID("invalid inputs");
    VERIFY_BOOL(ParseBool(""), false);
    VERIFY_BOOL(ParseBool("   "), false);
    VERIFY_BOOL(ParseBool("truee"), false);
    VERIFY_BOOL(ParseBool("test!"), false);
    VERIFY_BOOL(ParseBool("2"), false);
    VERIFY_BOOL(ParseBool("-1"), false);
}

static void Test_ParseUint64()
{
    GROUP_ID("valid numbers");
    VERIFY_UINT64("0", true, 0ULL);
    VERIFY_UINT64("1", true, 1ULL);
    VERIFY_UINT64("42", true, 42ULL);
    VERIFY_UINT64("000123", true, 123ULL);
    VERIFY_UINT64("  123", true, 123ULL);
    VERIFY_UINT64("123  ", true, 123ULL);

    GROUP_ID("max boundary");
    VERIFY_UINT64("18446744073709551615", true, 18446744073709551615ULL); //UINT64_MAX
    VERIFY_UINT64("18446744073709551614", true, 18446744073709551614ULL);

    GROUP_ID("overflow");
    VERIFY_UINT64("18446744073709551616", false, 0ULL); //UINT64_MAX + 1
    VERIFY_UINT64("99999999999999999999", false, 0ULL);

    GROUP_ID("invalid characters");
    VERIFY_UINT64("", false, 0ULL);
    VERIFY_UINT64(" ", false, 0ULL);
    VERIFY_UINT64("abc", false, 0ULL);
    VERIFY_UINT64("12a3", false, 0ULL);
    VERIFY_UINT64("123.4", false, 0ULL);
    VERIFY_UINT64("-1", false, 0ULL);
    VERIFY_UINT64("+1", false, 0ULL);

    GROUP_ID("edge formatting");
    VERIFY_UINT64("  00000000000000000001  ", true, 1ULL);
    VERIFY_UINT64("01", true, 1ULL);
}

void RunUnitTests()
{
    WarningMessage("------ BEGIN UNIT TESTS ------");
    PrintMessage("");

    Test_ParseVideoUrl();
    Test_ParseBool();
    Test_ParseUint64();

    PrintMessage("");
    WarningMessage("------ END UNIT TESTS ------");
    
    system("pause");
}
#endif