#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <Windows.h>
#include <stdbool.h>
#include "da.h"

typedef enum {
    THEME_TYPE_UNKNOWN,
    THEME_TYPE_WINDOWS,
    THEME_TYPE_XCURSOR
} ThemeType;

typedef enum {
    THEME_RESULT_SUCCESS = 0,
    THEME_RESULT_ERROR_FILE_NOT_FOUND,
    THEME_RESULT_ERROR_INVALID_FORMAT,
    THEME_RESULT_ERROR_MEMORY,
    THEME_RESULT_ERROR_EXTRACTION,
    THEME_RESULT_ERROR_CONVERSION,
    THEME_RESULT_ERROR_IO
} ThemeResult;

typedef struct {
    size_t index;
    wchar_t* name;
    wchar_t* description;
    wchar_t BasePath[MAX_PATH];
    wchar_t Icons[17][MAX_PATH];
} Theme;

da_define(Theme, Themes);

void ThemeManager_Initialize(const wchar_t* baseDirectory);
void ThemeManager_Cleanup();

const wchar_t* ThemeManager_IconIndexToName(size_t index);

ThemeResult ThemeManager_ImportFromArchive(const wchar_t* archivePath, Themes* themes);
ThemeResult ThemeManager_ImportFromDirectory(const wchar_t* dirPath, Themes* themes);
ThemeResult ThemeManager_InstallTheme(const Theme* theme);

ThemeResult ThemeManager_ScanDirectory(const wchar_t* path, Themes* themes);
ThemeType ThemeManager_DetectThemeType(const wchar_t* path);
bool ThemeManager_ValidateTheme(const Theme* theme);

ThemeResult ThemeManager_ProcessWindowsTheme(const wchar_t* path, Theme* theme);
ThemeResult ThemeManager_ProcessXCursorTheme(const wchar_t* path, Theme* theme);

ThemeResult ThemeManager_ProcessTheme(const wchar_t* path, Theme* theme);

wchar_t* ThemeManager_CharToWchar(const char* str);
wchar_t* ThemeManager_StringToLower(wchar_t* str);
bool ThemeManager_DeleteDirectoryRecursive(const wchar_t* path);
wchar_t* ThemeManager_GetThemeDisplayName(const Theme* theme);
void ThemeManager_FreeTheme(Theme* theme);

typedef enum {
    ARCHIVE_TYPE_UNKNOWN,
    ARCHIVE_TYPE_XZ,
    ARCHIVE_TYPE_BZ2,
    ARCHIVE_TYPE_ZIP,
    ARCHIVE_TYPE_TAR,
    ARCHIVE_TYPE_GZIP
} ArchiveType;

ArchiveType ThemeManager_GetArchiveType(const wchar_t* path);
ThemeResult ThemeManager_ExtractArchive(const wchar_t* archivePath, wchar_t* outPath);

const wchar_t* ThemeManager_GetErrorMessage(ThemeResult result);

#endif