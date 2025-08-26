#include "theme_manager.h"
#include "ini.h"
#include "XCursor.h"
#include "wincur.h"
#include "xz_unzip.h"
#include "bzip_unzip.h"
#include "gzip_unzip.h"
#include "zip_unzip.h"
#include "tar_unzip.h"

#include <setupapi.h>
#include <shlwapi.h>
#include <shlobj_core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "shlwapi.lib")

#define XZ_MAGIC	"\xFD\x37\x7A\x58\x5A"
#define BZ2_MAGIC	"\x42\x5A\x68"
#define ZIP_MAGIC	"\x50\x4B\x03\x04"
#define GZIP_MAGIC	"\x1F\x8B"
#define TAR_MAGIC	"ustar"

static wchar_t g_baseDirectory[MAX_PATH] = { 0 };

static const wchar_t* g_indexToName[] = {
    L"Arrow", L"Help", L"AppStarting", L"Wait", L"Crosshair", L"IBeam", L"NWPen", L"No",
    L"SizeNS", L"SizeWE", L"SizeNWSE", L"SizeNESW", L"SizeAll", L"UpArrow", L"Hand", L"Pin", L"Person"
};

void ThemeManager_Initialize(const wchar_t* baseDirectory) {
    if (baseDirectory) {
        wcscpy_s(g_baseDirectory, MAX_PATH, baseDirectory);
    }
}

void ThemeManager_Cleanup() {

}

const wchar_t* ThemeManager_IconIndexToName(size_t index)
{
    return g_indexToName[index];
}

const wchar_t* ThemeManager_GetErrorMessage(ThemeResult result) {
    switch (result) {
    case THEME_RESULT_SUCCESS: return L"Success";
    case THEME_RESULT_ERROR_FILE_NOT_FOUND: return L"File not found";
    case THEME_RESULT_ERROR_INVALID_FORMAT: return L"Invalid file format";
    case THEME_RESULT_ERROR_MEMORY: return L"Memory allocation failed";
    case THEME_RESULT_ERROR_EXTRACTION: return L"Archive extraction failed";
    case THEME_RESULT_ERROR_CONVERSION: return L"Cursor conversion failed";
    case THEME_RESULT_ERROR_IO: return L"I/O operation failed";
    default: return L"Unknown error";
    }
}

ArchiveType ThemeManager_GetArchiveType(const wchar_t* path) {
    FILE* f = _wfopen(path, L"rb");
    if (!f) return ARCHIVE_TYPE_UNKNOWN;

    char buf[5];
    if (!fread(buf, 1, 5, f)) {
        fclose(f);
        return ARCHIVE_TYPE_UNKNOWN;
    }

    ArchiveType type = ARCHIVE_TYPE_UNKNOWN;
    if (memcmp(buf, XZ_MAGIC, 5) == 0) type = ARCHIVE_TYPE_XZ;
    else if (memcmp(buf, BZ2_MAGIC, 3) == 0) type = ARCHIVE_TYPE_BZ2;
    else if (memcmp(buf, ZIP_MAGIC, 4) == 0) type = ARCHIVE_TYPE_ZIP;
    else if (memcmp(buf, GZIP_MAGIC, 2) == 0) type = ARCHIVE_TYPE_GZIP;
    else {
        fseek(f, 257, SEEK_SET);
        if (fread(buf, 1, 5, f) == 5 && memcmp(buf, TAR_MAGIC, 5) == 0) {
            type = ARCHIVE_TYPE_TAR;
        }
    }

    fclose(f);
    return type;
}

wchar_t* ThemeManager_CharToWchar(const char* str) {
    if (!str) return NULL;

    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (len == 0) return NULL;

    wchar_t* wstr = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!wstr) return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, len) == 0) {
        free(wstr);
        return NULL;
    }

    return wstr;
}

wchar_t* ThemeManager_StringToLower(wchar_t* str) {
    if (!str) return NULL;
    int len = (int)wcslen(str);
    LCMapStringW(LOCALE_INVARIANT, LCMAP_LOWERCASE, str, len, str, len);
    str[len] = L'\0';
    return str;
}

bool ThemeManager_DeleteDirectoryRecursive(const wchar_t* path) {
    WIN32_FIND_DATAW findData;
    TCHAR searchPath[MAX_PATH];
    TCHAR fullPath[MAX_PATH];

    swprintf_s(searchPath, MAX_PATH, L"%s\\*", path);
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0)
            continue;

        swprintf_s(fullPath, MAX_PATH, L"%s\\%s", path, findData.cFileName);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ThemeManager_DeleteDirectoryRecursive(fullPath);
        }
        else {
            DeleteFileW(fullPath);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return RemoveDirectoryW(path) != 0;
}

void ThemeManager_FreeTheme(Theme* theme) {
    if (!theme) return;

    if (theme->name) {
        free(theme->name);
        theme->name = NULL;
    }
    if (theme->description) {
        free(theme->description);
        theme->description = NULL;
    }
}

ThemeType ThemeManager_DetectThemeType(const wchar_t* path) {
    TCHAR testPath[MAX_PATH];

    swprintf_s(testPath, MAX_PATH, L"%s\\install.inf", path);
    if (GetFileAttributesW(testPath) != INVALID_FILE_ATTRIBUTES) {
        return THEME_TYPE_WINDOWS;
    }

    swprintf_s(testPath, MAX_PATH, L"%s\\index.theme", path);
    if (GetFileAttributesW(testPath) != INVALID_FILE_ATTRIBUTES) {
        return THEME_TYPE_XCURSOR;
    }

    swprintf_s(testPath, MAX_PATH, L"%s\\cursor.theme", path);
    if (GetFileAttributesW(testPath) != INVALID_FILE_ATTRIBUTES) {
        return THEME_TYPE_XCURSOR;
    }

    swprintf_s(testPath, MAX_PATH, L"%s\\cursors", path);
    DWORD attrs = GetFileAttributesW(testPath);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return THEME_TYPE_XCURSOR;
    }

    return THEME_TYPE_UNKNOWN;
}

static int ThemeINI_Handler(void* user, const char* section, const char* name, const char* value) {
    Theme* theme = (Theme*)user;
    char* tmp = (char*)value;

    if (tmp[0] == '"') tmp++;
    if (strlen(tmp) > 0 && tmp[strlen(tmp) - 1] == '"') {
        tmp[strlen(tmp) - 1] = '\0';
    }

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("General", "Name")) {
        theme->name = ThemeManager_CharToWchar(tmp);
    }
    else if (MATCH("General", "Description")) {
        theme->description = ThemeManager_CharToWchar(tmp);
    }
    else if (strcmp(section, "Cursors") == 0) {
        int cursorIndex = -1;

        if (strcmp(name, "Arrow") == 0) cursorIndex = 0;
        else if (strcmp(name, "Help") == 0) cursorIndex = 1;
        else if (strcmp(name, "AppStarting") == 0) cursorIndex = 2;
        else if (strcmp(name, "Wait") == 0) cursorIndex = 3;
        else if (strcmp(name, "Crosshair") == 0) cursorIndex = 4;
        else if (strcmp(name, "IBeam") == 0) cursorIndex = 5;
        else if (strcmp(name, "NWPen") == 0) cursorIndex = 6;
        else if (strcmp(name, "No") == 0) cursorIndex = 7;
        else if (strcmp(name, "SizeNS") == 0) cursorIndex = 8;
        else if (strcmp(name, "SizeWE") == 0) cursorIndex = 9;
        else if (strcmp(name, "SizeNWSE") == 0) cursorIndex = 10;
        else if (strcmp(name, "SizeNESW") == 0) cursorIndex = 11;
        else if (strcmp(name, "SizeAll") == 0) cursorIndex = 12;
        else if (strcmp(name, "UpArrow") == 0) cursorIndex = 13;
        else if (strcmp(name, "Hand") == 0) cursorIndex = 14;
        else if (strcmp(name, "Pin") == 0) cursorIndex = 15;
        else if (strcmp(name, "Person") == 0) cursorIndex = 16;

        if (cursorIndex >= 0 && cursorIndex < 17) {
            wchar_t* filenameW = ThemeManager_CharToWchar(tmp);
            wcscpy_s(theme->Icons[cursorIndex], MAX_PATH, filenameW);
            free(filenameW);
        }
    }

    return 1;
}

static int XCursorThemeINI_Handler(void* user, const char* section, const char* name, const char* value) {
    Theme* theme = (Theme*)user;
    char* tmp = (char*)value;

    if (tmp[0] == '"') tmp++;
    if (strlen(tmp) > 0 && tmp[strlen(tmp) - 1] == '"') tmp[strlen(tmp) - 1] = '\0';

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("Icon Theme", "Name")) {
        theme->name = ThemeManager_CharToWchar(tmp);
    }
    else if (MATCH("Icon Theme", "Comment") || MATCH("Icon Theme", "Description")) {
        theme->description = ThemeManager_CharToWchar(tmp);
    }

    return 1;
}

ThemeResult ThemeManager_ProcessTheme(const wchar_t* path, Theme* theme)
{
    char themeIniPathA[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, path, -1, themeIniPathA, MAX_PATH, NULL, NULL);
    if (ini_parse(themeIniPathA, ThemeINI_Handler, theme) < 0)
        return THEME_RESULT_ERROR_INVALID_FORMAT;

    return THEME_RESULT_SUCCESS;
}

typedef struct { const wchar_t* name; int prio; } NameEntry;
typedef struct { const NameEntry* names; int index; } Mapping;


static const NameEntry default_names[] = { {L"default",3}, {L"left_ptr",2}, {NULL,0} };
static const NameEntry help_names[] = { {L"help",3}, {L"question_arrow",2}, {NULL,0} };
static const NameEntry progress_names[] = { {L"progress",3}, {L"left_ptr_watch",2}, {NULL,0} };
static const NameEntry wait_names[] = { {L"wait",3}, {L"watch",2}, {NULL,0} };
static const NameEntry crosshair_names[] = { {L"crosshair",3}, {L"cross",2}, {NULL,0} };
static const NameEntry text_names[] = { {L"text",3}, {L"ibeam",2}, {L"xterm",1}, {NULL,0} };
static const NameEntry pencil_names[] = { {L"pencil",3}, {L"pen",2}, {NULL,0} };
static const NameEntry notallowed_names[] = { {L"not-allowed",3}, {L"crossed_circle",2}, {NULL,0} };
static const NameEntry nsresize_names[] = { {L"ns-resize",3}, {L"sb_v_double_arrow",2}, {L"size_ver",1}, {NULL,0} };
static const NameEntry ewresize_names[] = { {L"ew-resize",3}, {L"sb_h_double_arrow",2}, {L"size_hor",1}, {NULL,0} };
static const NameEntry nwse_names[] = { {L"nwse-resize",3}, {L"size_fdiag",2}, {L"bd_double_arrow",1}, {NULL,0} };
static const NameEntry nesw_names[] = { {L"nesw-resize",3}, {L"size_bdiag",2}, {L"fd_double_arrow",1}, {NULL,0} };
static const NameEntry move_names[] = { {L"all-scroll",3}, {L"fleur",2}, { L"move",1 }, {NULL,0} };
static const NameEntry uparrow_names[] = { {L"up-arrow",3}, {L"up",2}, {L"up_arrow",1}, {L"sb_up_arrow",1}, {NULL,0} };
static const NameEntry hand_names[] = { {L"hand",5}, {L"pointer",3}, {L"link",2}, {NULL,0} };

static const Mapping mapping[] = {
    { default_names, 0 },
    { help_names, 1 },
    { progress_names, 2 },
    { wait_names, 3 },
    { crosshair_names, 4 },
    { text_names, 5 },
    { pencil_names, 6 },
    { notallowed_names, 7 },
    { nsresize_names, 8 },
    { ewresize_names, 9 },
    { nwse_names, 10 },
    { nesw_names, 11 },
    { move_names, 12 },
    { uparrow_names, 13 },
    { hand_names, 14 }
};

static int find_priority(const NameEntry* names, const wchar_t* fileName)
{
    int i;
    for (i = 0; names[i].name != NULL; i++)
        if (wcscmp(fileName, names[i].name) == 0)
            return names[i].prio;
    return INT_MIN;
}

static int MapXCursorName(const wchar_t* fileName, wchar_t* defaultIcons[17], bool dupped[17])
{
    int i;
    for (i = 0; i < (int)(sizeof(mapping) / sizeof(mapping[0])); i++)
    {
        const NameEntry* names = mapping[i].names;
        int idx = mapping[i].index;

        int prio_new = find_priority(names, fileName);
        if (prio_new == INT_MIN) continue;

        if (!dupped[idx])
        {
            defaultIcons[idx] = _wcsdup(fileName);
            dupped[idx] = true;
        }
        else
        {
            int prio_cur = find_priority(names, defaultIcons[idx]);
            if (prio_new > prio_cur)
            {
                free(defaultIcons[idx]);
                defaultIcons[idx] = _wcsdup(fileName);
            }
        }
        return true;
    }
    return false;
}

static wchar_t* ConvertXCursorToWindows(const wchar_t* xcurPath, const wchar_t* outDir) {
    XCurFile file = { 0 };
    XCursor cursor = { 0 };
    wchar_t* resultPath = NULL;
    TCHAR outPath[MAX_PATH];

    unsigned char* finalData = NULL;
    size_t outSize = 0;

    uint32_t** frames = NULL;
    unsigned int* delays = NULL;
    int* xhots = NULL;
    int* yhots = NULL;
    FILE* f = NULL;

    if (!xcur_read_file(xcurPath, &file)) goto cleanup;
    if (!xcur_read_xcurosr(&file, &cursor, 0, 64)) goto cleanup;
    if (!cursor.frames || cursor.nframes < 1) goto cleanup;

    if (cursor.animated) {
        frames = malloc(sizeof(uint32_t*) * cursor.nframes);
        delays = malloc(sizeof(unsigned int) * cursor.nframes);
        xhots = malloc(sizeof(int) * cursor.nframes);
        yhots = malloc(sizeof(int) * cursor.nframes);

        if (!frames || !delays || !xhots || !yhots) goto cleanup;

        for (size_t i = 0; i < cursor.nframes; i++) {
            XImage image = cursor.frames[i];
            delays[i] = image.delay;
            frames[i] = (uint32_t*)image.pixels;
            xhots[i] = image.xhot;
            yhots[i] = image.yhot;
        }

        swprintf_s(outPath, MAX_PATH, L"%s%s.ani", outDir, PathFindFileNameW(xcurPath));
        finalData = wincur_create_ANI_from_frames(
            (const uint32_t**)frames, delays, cursor.nframes,
            cursor.frames[0].width, cursor.frames[0].height,
            xhots, yhots, &outSize
        );
    }
    else {
        XImage image = cursor.frames[0];
        swprintf_s(outPath, MAX_PATH, L"%s%s.cur", outDir, PathFindFileNameW(xcurPath));
        finalData = wincur_create_ARGB_as_CUR(
            (uint32_t*)image.pixels,
            image.width, image.height,
            image.xhot, image.yhot, &outSize
        );
    }

    if (!finalData) goto cleanup;

    f = _wfopen(outPath, L"wb");
    if (!f) goto cleanup;

    fwrite(finalData, 1, outSize, f);
    fclose(f);
    f = NULL;

    resultPath = _wcsdup(outPath);

cleanup:
    if (f) fclose(f);
    free(finalData);
    free(frames);
    free(delays);
    free(xhots);
    free(yhots);
    xcur_free_cursor(&cursor);
    xcur_close_file(&file);

    return resultPath;
}

static void CopyThemeCursors(Theme* theme, const wchar_t* baseDirPath, wchar_t* iconsPaths[17], wchar_t* outDir) {
    TCHAR tmp[MAX_PATH];
    TCHAR tmp2[MAX_PATH];
    BYTE hash[16];

    HashData((BYTE*)baseDirPath, (DWORD)(wcslen(baseDirPath) * sizeof(TCHAR)), hash, 16);

    TCHAR outDirName[MAX_PATH] = { 0 };
    swprintf_s(outDirName, MAX_PATH, L"%s\\themes\\", g_baseDirectory);
    size_t baseDirLen = wcslen(outDirName);
    for (size_t i = 0; i < 16; i++) {
        swprintf_s(&outDirName[baseDirLen + i * 2], MAX_PATH - baseDirLen - i * 2, L"%02x", hash[i]);
    }

    swprintf_s(tmp, MAX_PATH, L"%s\\cursors", outDirName);
    SHCreateDirectoryExW(NULL, outDirName, NULL);
    SHCreateDirectoryExW(NULL, tmp, NULL);

    swprintf_s(tmp, MAX_PATH, L"%s\\theme.ini", outDirName);

    FILE* f = _wfopen(tmp, L"w");
    if (!f) return;

    fwprintf(f, L"[General]\n");
    if (theme->name)
        fwprintf(f, L"Name = %s\n", theme->name);
    if (theme->description)
        fwprintf(f, L"Description = %s\n", theme->description);

    fwprintf(f, L"[Cursors]\n");
    for (size_t i = 0; i < 17; i++) {
        fwprintf(f, L"%s = %s\n", g_indexToName[i], theme->Icons[i]);
        if (iconsPaths[i] && iconsPaths[i][0] != L'%' && wcslen(iconsPaths[i]) != 0) {
            swprintf_s(tmp, MAX_PATH, L"%s\\cursors\\%s", outDirName, theme->Icons[i]);
            swprintf_s(tmp2, MAX_PATH, L"%s\\%s", baseDirPath, iconsPaths[i]);
            CopyFileW(tmp2, tmp, FALSE);
        }
    }

    fclose(f);

    if (outDir) {
        wcscpy_s(outDir, MAX_PATH, outDirName);
    }
}

ThemeResult ThemeManager_ProcessWindowsTheme(const wchar_t* path, Theme* theme) {
    bool dupped[17] = { 0 };
    wchar_t* defaultIcons[] = {
        L"%SystemRoot%\\cursors\\aero_arrow.cur",
        L"%SystemRoot%\\cursors\\aero_helpsel.cur",
        L"%SystemRoot%\\cursors\\aero_working.ani",
        L"%SystemRoot%\\cursors\\aero_busy.ani",
        L"", // crosshair
        L"", // text  
        L"%SystemRoot%\\cursors\\aero_pen.cur",
        L"%SystemRoot%\\cursors\\aero_unavail.cur",
        L"%SystemRoot%\\cursors\\aero_ns.cur",
        L"%SystemRoot%\\cursors\\aero_ew.cur",
        L"%SystemRoot%\\cursors\\aero_nwse.cur",
        L"%SystemRoot%\\cursors\\aero_nesw.cur",
        L"%SystemRoot%\\cursors\\aero_move.cur",
        L"%SystemRoot%\\cursors\\aero_up.cur",
        L"%SystemRoot%\\cursors\\aero_link.cur",
        L"%SystemRoot%\\cursors\\aero_pin.cur",
        L"%SystemRoot%\\cursors\\aero_person.cur",
    };

    wchar_t* iconsLocations[17] = { 0 };
    TCHAR buffer[1024];
    swprintf_s(buffer, 1024, L"%s\\install.inf", path);

    HINF hInf = SetupOpenInfFileW(buffer, NULL, INF_STYLE_WIN4, NULL);
    if (hInf == INVALID_HANDLE_VALUE) {
        return THEME_RESULT_ERROR_INVALID_FORMAT;
    }

    INFCONTEXT context;
    if (SetupFindFirstLineW(hInf, L"Scheme.Reg", NULL, &context)) {
        do {
            DWORD requiredSize = 0;
            SetupGetLineTextW(&context, NULL, NULL, NULL, NULL, 0, &requiredSize);
            if (requiredSize == 0) continue;

            TCHAR* lineBuffer = (TCHAR*)malloc(requiredSize * sizeof(TCHAR));
            if (!lineBuffer) break;

            if (SetupGetLineTextW(&context, NULL, NULL, NULL, lineBuffer, requiredSize, NULL)) {
                SetupGetStringFieldW(&context, 3, NULL, 0, &requiredSize);
                if (requiredSize > 0) {
                    TCHAR* nameBuffer = (TCHAR*)malloc(requiredSize * sizeof(TCHAR));
                    if (nameBuffer && SetupGetStringFieldW(&context, 3, nameBuffer, requiredSize, NULL)) {
                        theme->name = _wcsdup(nameBuffer);
                    }
                    free(nameBuffer);
                }

                SetupGetStringFieldW(&context, 5, NULL, 0, &requiredSize);
                if (requiredSize > 0) {
                    TCHAR* cursorsBuffer = (TCHAR*)malloc(requiredSize * sizeof(TCHAR));
                    if (cursorsBuffer && SetupGetStringFieldW(&context, 5, cursorsBuffer, requiredSize, NULL)) {
                        TCHAR* context_ptr = NULL;
                        TCHAR* token = wcstok_s(cursorsBuffer, L",", &context_ptr);
                        size_t index = 0;
                        while (token && index < 17) {
                            defaultIcons[index] = _wcsdup(token);
                            dupped[index] = true;
                            index++;
                            token = wcstok_s(NULL, L",", &context_ptr);
                        }
                    }
                    free(cursorsBuffer);
                }
            }

            free(lineBuffer);
        } while (SetupFindNextLine(&context, &context));
    }

    if (SetupFindFirstLineW(hInf, L"Scheme.Cur", NULL, &context)) {
        do {
            DWORD requiredSize = 0;
            SetupGetLineTextW(&context, NULL, NULL, NULL, NULL, 0, &requiredSize);
            if (requiredSize == 0) continue;

            TCHAR* buffer = (TCHAR*)malloc(requiredSize * sizeof(TCHAR));
            if (!buffer) break;

            if (SetupGetLineTextW(&context, NULL, NULL, NULL, buffer, requiredSize, NULL)) {
                for (size_t i = 0; i < 17; i++) {
                    if (defaultIcons[i]) {
                        size_t iconLen = wcslen(defaultIcons[i]);
                        size_t bufLen = wcslen(buffer);
                        if (bufLen <= iconLen) {
                            wchar_t* suffix = defaultIcons[i] + (iconLen - bufLen);
                            if (wcscmp(suffix, buffer) == 0) {
                                iconsLocations[i] = suffix;
                                break;
                            }
                        }
                    }
                }
            }

            free(buffer);
        } while (SetupFindNextLine(&context, &context));
    }

    for (size_t i = 0; i < 17; i++) {
        if (!iconsLocations[i]) iconsLocations[i] = defaultIcons[i];
        if (dupped[i]) {
            wcscpy(theme->Icons[i], PathFindFileNameW(iconsLocations[i]));
        }
        else {
            wcscpy(theme->Icons[i], iconsLocations[i]);
        }
    }

    CopyThemeCursors(theme, path, iconsLocations, NULL);

    for (size_t i = 0; i < 17; i++) {
        if (dupped[i]) free(defaultIcons[i]);
    }

    SetupCloseInfFile(hInf);
    return THEME_RESULT_SUCCESS;
}

ThemeResult ThemeManager_ProcessXCursorTheme(const wchar_t* path, Theme* theme) {
    bool dupped[17] = { 0 };
    wchar_t* defaultIcons[] = {
        L"%SystemRoot%\\cursors\\aero_arrow.cur",
        L"%SystemRoot%\\cursors\\aero_helpsel.cur",
        L"%SystemRoot%\\cursors\\aero_working.ani",
        L"%SystemRoot%\\cursors\\aero_busy.ani",
        L"", // crosshair
        L"", // text
        L"%SystemRoot%\\cursors\\aero_pen.cur",
        L"%SystemRoot%\\cursors\\aero_unavail.cur",
        L"%SystemRoot%\\cursors\\aero_ns.cur",
        L"%SystemRoot%\\cursors\\aero_ew.cur",
        L"%SystemRoot%\\cursors\\aero_nwse.cur",
        L"%SystemRoot%\\cursors\\aero_nesw.cur",
        L"%SystemRoot%\\cursors\\aero_move.cur",
        L"%SystemRoot%\\cursors\\aero_up.cur",
        L"%SystemRoot%\\cursors\\aero_link.cur",
        L"%SystemRoot%\\cursors\\aero_pin.cur",
        L"%SystemRoot%\\cursors\\aero_person.cur",
    };

    TCHAR tmp[MAX_PATH];

    swprintf_s(tmp, MAX_PATH, L"%s\\index.theme", path);
    FILE* f = _wfopen(tmp, L"r");
    if (!f) {
        swprintf_s(tmp, MAX_PATH, L"%s\\cursor.theme", path);
        f = _wfopen(tmp, L"r");
    }

    if (f) {
        ini_parse_file(f, XCursorThemeINI_Handler, theme);
        fclose(f);
    }

    swprintf_s(tmp, MAX_PATH, L"%s\\cursors\\*", path);
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(tmp, &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findData.cFileName, L".") == 0 ||
                wcscmp(findData.cFileName, L"..") == 0)
                continue;

            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                swprintf_s(tmp, MAX_PATH, L"%s\\cursors\\%s", path, findData.cFileName);
                if (xcur_correct_file(tmp)) {
                    MapXCursorName(findData.cFileName, defaultIcons, dupped);
                }
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }

    swprintf_s(tmp, MAX_PATH, L"%s\\cursors\\converted\\", path);
    SHCreateDirectoryExW(NULL, tmp, NULL);

    for (size_t i = 0; i < 17; i++) {
        if (dupped[i]) {
            TCHAR iconPath[MAX_PATH];
            swprintf_s(iconPath, MAX_PATH, L"%s\\cursors\\%s", path, defaultIcons[i]);
            wchar_t* outName = ConvertXCursorToWindows(iconPath, tmp);
            if (outName) {
                wcscpy(theme->Icons[i], PathFindFileNameW(outName));
                free(defaultIcons[i]);

                TCHAR relativePath[MAX_PATH];
                swprintf_s(relativePath, MAX_PATH, L"%s", &outName[wcslen(path)]);
                defaultIcons[i] = _wcsdup(relativePath);
                free(outName);
            }
        }
        else {
            wcscpy(theme->Icons[i], defaultIcons[i]);
        }
    }

    TCHAR outDir[MAX_PATH];
    CopyThemeCursors(theme, path, defaultIcons, outDir);

    swprintf_s(tmp, MAX_PATH, L"%s\\xcursors", outDir);
    SHCreateDirectoryExW(NULL, tmp, NULL);

    for (size_t i = 0; i < 17; i++) {
        if (dupped[i]) {
            TCHAR srcPath[MAX_PATH], destPath[MAX_PATH];
            swprintf_s(srcPath, MAX_PATH, L"%s\\cursors\\%s", path, theme->Icons[i]);
            swprintf_s(destPath, MAX_PATH, L"%s\\xcursors\\%s", outDir, theme->Icons[i]);

            wchar_t* ext = wcsrchr(destPath, L'.');
            if (ext) *ext = L'\0';

            ext = wcsrchr(srcPath, L'.');
            if (ext) *ext = L'\0';

            CopyFileW(srcPath, destPath, FALSE);
            free(defaultIcons[i]);
        }
    }

    return THEME_RESULT_SUCCESS;
}

ThemeResult ThemeManager_ExtractArchive(const wchar_t* archivePath, wchar_t* outPath) {
    ArchiveType type = ThemeManager_GetArchiveType(archivePath);
    TCHAR tempDir[MAX_PATH];

    if (!GetTempPathW(MAX_PATH, tempDir)) {
        return THEME_RESULT_ERROR_IO;
    }

    TCHAR tempFile[MAX_PATH];
    if (!GetTempFileNameW(tempDir, L"CSY", 0, tempFile)) {
        return THEME_RESULT_ERROR_IO;
    }

    DeleteFileW(tempFile);
    wchar_t* ext = wcsrchr(tempFile, L'.');
    if (ext) *ext = L'\0';

    if (!CreateDirectoryW(tempFile, NULL)) {
        return THEME_RESULT_ERROR_IO;
    }

    wcscpy_s(outPath, MAX_PATH, tempFile);

    int depth = 0;
    TCHAR currentFile[MAX_PATH];
    wcscpy_s(currentFile, MAX_PATH, archivePath);

    while (true) {
        TCHAR extractPath[MAX_PATH];
        swprintf_s(extractPath, MAX_PATH, L"%s\\extracted%d", tempFile, depth);

        Byte* output = NULL;
        size_t outputSize = 0;
        int result = 0;

        switch (type) {
        case ARCHIVE_TYPE_XZ:
            result = decompress_xz_file(currentFile, &output, &outputSize);
            break;
        case ARCHIVE_TYPE_BZ2:
            result = decompress_bz2_file(currentFile, &output, &outputSize);
            break;
        case ARCHIVE_TYPE_GZIP:
            result = decompress_gzip_file(currentFile, &output, &outputSize);
            break;
        case ARCHIVE_TYPE_ZIP:
            CreateDirectoryW(extractPath, NULL);
            result = unzip_to_folder(currentFile, extractPath);
            goto extraction_complete;
        case ARCHIVE_TYPE_TAR:
            CreateDirectoryW(extractPath, NULL);
            result = extract_tar(currentFile, extractPath);
            goto extraction_complete;
        default:
            return THEME_RESULT_ERROR_INVALID_FORMAT;
        }

        if (result != 0) {
            return THEME_RESULT_ERROR_EXTRACTION;
        }

        if (type != ARCHIVE_TYPE_ZIP && type != ARCHIVE_TYPE_TAR) {
            FILE* f = _wfopen(extractPath, L"wb");
            if (!f) {
                free(output);
                return THEME_RESULT_ERROR_IO;
            }
            fwrite(output, 1, outputSize, f);
            fclose(f);
            wcscpy_s(currentFile, MAX_PATH, extractPath);
            free(output);
        }

        type = ThemeManager_GetArchiveType(extractPath);
        if (type == ARCHIVE_TYPE_UNKNOWN) break;

        depth++;
        if (depth > 10) break;
    }

extraction_complete:
    return THEME_RESULT_SUCCESS;
}

ThemeResult ThemeManager_ImportFromArchive(const wchar_t* archivePath, Themes* themes) {
    TCHAR tempDir[MAX_PATH];
    ThemeResult result = ThemeManager_ExtractArchive(archivePath, tempDir);
    if (result != THEME_RESULT_SUCCESS) {
        return result;
    }

    result = ThemeManager_ScanDirectory(tempDir, themes);

    ThemeManager_DeleteDirectoryRecursive(tempDir);

    return result;
}

ThemeResult ThemeManager_ImportFromDirectory(const wchar_t* dirPath, Themes* themes) {
    return ThemeManager_ScanDirectory(dirPath, themes);
}

ThemeResult ThemeManager_ScanDirectory(const wchar_t* path, Themes* themes) {
    TCHAR searchPath[MAX_PATH];
    WIN32_FIND_DATAW findData;

    swprintf_s(searchPath, MAX_PATH, L"%s\\*", path);
    HANDLE hFind = FindFirstFileW(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return THEME_RESULT_ERROR_FILE_NOT_FOUND;
    }

    bool foundTheme = false;
    ThemeResult result = THEME_RESULT_SUCCESS;

    do {
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0)
            continue;

        TCHAR fullPath[MAX_PATH];
        swprintf_s(fullPath, MAX_PATH, L"%s\\%s", path, findData.cFileName);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ThemeManager_ScanDirectory(fullPath, themes);
        }
        else if (!foundTheme) {
            wchar_t* lowerName = _wcsdup(findData.cFileName);
            ThemeManager_StringToLower(lowerName);

            ThemeType themeType = THEME_TYPE_UNKNOWN;
            if (wcscmp(lowerName, L"install.inf") == 0) {
                themeType = THEME_TYPE_WINDOWS;
            }
            else if (wcscmp(lowerName, L"index.theme") == 0 ||
                wcscmp(lowerName, L"cursor.theme") == 0) {
                themeType = THEME_TYPE_XCURSOR;
            }

            if (themeType != THEME_TYPE_UNKNOWN) {
                Theme* newTheme = calloc(1, sizeof(Theme));
                if (!newTheme) {
                    free(lowerName);
                    result = THEME_RESULT_ERROR_MEMORY;
                    break;
                }

                wcscpy(newTheme->BasePath, path);
                newTheme->index = themes->length;

                ThemeResult processResult;
                if (themeType == THEME_TYPE_WINDOWS) {
                    processResult = ThemeManager_ProcessWindowsTheme(path, newTheme);
                }
                else {
                    processResult = ThemeManager_ProcessXCursorTheme(path, newTheme);
                }

                if (processResult == THEME_RESULT_SUCCESS) {
                    da_append(themes, *newTheme);
                    foundTheme = true;
                }
                else {
                    ThemeManager_FreeTheme(newTheme);
                    free(newTheme);
                    result = processResult;
                }
            }

            free(lowerName);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return result;
}

bool ThemeManager_ValidateTheme(const Theme* theme) {
    if (!theme) return false;

    int definedCursors = 0;
    for (int i = 0; i < 17; i++) {
        if (theme->Icons[i][0] != '\0') {
            definedCursors++;
        }
    }

    return definedCursors >= 3;
}

wchar_t* ThemeManager_GetThemeDisplayName(const Theme* theme) {
    if (!theme) return NULL;

    if (theme->name && wcslen(theme->name) > 0) {
        return _wcsdup(theme->name);
    }

    wchar_t* baseName = PathFindFileNameW(theme->BasePath);
    if (baseName && wcslen(baseName) > 0) {
        return _wcsdup(baseName);
    }

    return _wcsdup(L"Unnamed Theme");
}

ThemeResult ThemeManager_InstallTheme(const Theme* theme) {
    if (!ThemeManager_ValidateTheme(theme)) {
        return THEME_RESULT_ERROR_INVALID_FORMAT;
    }

    return THEME_RESULT_SUCCESS;
}