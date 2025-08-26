#include "zip_unzip.h"

#include <windows.h>
#include <wchar.h>
#include <shlwapi.h>
#include <direct.h>
#include <stdio.h>
#include <stdbool.h>

#pragma comment(lib, "shlwapi.lib")

void wchar_to_utf8(const wchar_t* src, char* dest, int destSize) {
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dest, destSize, NULL, NULL);
}

void create_dirs_w(const wchar_t* path) {
    wchar_t temp[MAX_PATH];
    wcsncpy(temp, path, MAX_PATH);
    temp[MAX_PATH - 1] = 0;

    for (wchar_t* p = temp + 1; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            wchar_t old = *p;
            *p = 0;
            _wmkdir(temp);
            *p = old;
        }
    }
}

int unzip_to_folder(const wchar_t* zipPath, const wchar_t* outDir) {
    char zipPathUTF8[MAX_PATH];
    wchar_to_utf8(zipPath, zipPathUTF8, MAX_PATH);

    mz_zip_archive zip = { 0 };
    if (!mz_zip_reader_init_file(&zip, zipPathUTF8, 0)) {
        wprintf(L"Failed to open ZIP archive: %s\n", zipPath);
        return -1;
    }

    int fileCount = (int)mz_zip_reader_get_num_files(&zip);
    for (int i = 0; i < fileCount; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat))
            continue;

        wchar_t fileNameW[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, stat.m_filename, -1, fileNameW, MAX_PATH);

        wchar_t outPathW[MAX_PATH];
        swprintf(outPathW, MAX_PATH, L"%s\\%s", outDir, fileNameW);

        create_dirs_w(outPathW);

        bool isSymlink = (((stat.m_external_attr >> 16) & 0xF000) == 0xA000);
        bool isDirectory = mz_zip_reader_is_file_a_directory(&zip, i);

        if (isDirectory)
            continue;
        else if (isSymlink) {
            char targetPath[MAX_PATH] = { 0 };
            char symlinkNameUTF8[MAX_PATH] = { 0 };

            strncpy(symlinkNameUTF8, stat.m_filename, MAX_PATH - 1);
            symlinkNameUTF8[MAX_PATH - 1] = 0;

            char* lastSlash = strrchr(symlinkNameUTF8, '/');
            if (!lastSlash)
                lastSlash = strrchr(symlinkNameUTF8, '\\');
            if (lastSlash)
                *(lastSlash + 1) = '\0';
            else
                symlinkNameUTF8[0] = '\0';

            strcpy(targetPath, symlinkNameUTF8);

            char symlinkTarget[MAX_PATH] = { 0 };

            int targetIndex = i;
            bool resolved = false;
            int maxSymlinkDepth = 10;

            while (maxSymlinkDepth-- > 0) {
                if (!mz_zip_reader_extract_to_mem(&zip, targetIndex, symlinkTarget, MAX_PATH - strlen(targetPath), 0))
                    break;

                symlinkTarget[MAX_PATH - 1] = 0;

                char nextPath[MAX_PATH];
                snprintf(nextPath, MAX_PATH, "%s%s", targetPath, symlinkTarget);

                int nextIndex = mz_zip_reader_locate_file(&zip, nextPath, NULL, 0);
                if (nextIndex < 0)
                    break;

                mz_zip_archive_file_stat nextStat;
                if (!mz_zip_reader_file_stat(&zip, nextIndex, &nextStat))
                    break;

                bool nextIsSymlink = (((nextStat.m_external_attr >> 16) & 0xF000) == 0xA000);

                if (!nextIsSymlink) {
                    targetIndex = nextIndex;
                    resolved = true;
                    break;
                }

                strcpy(targetPath, nextPath);
                char* pLastSlash = strrchr(targetPath, '/');
                if (!pLastSlash)
                    pLastSlash = strrchr(targetPath, '\\');
                if (pLastSlash)
                    *(pLastSlash + 1) = '\0';
                else
                    targetPath[0] = '\0';

                targetIndex = nextIndex;
            }

            if (resolved) {
                char outPathUTF8[MAX_PATH];
                wchar_to_utf8(outPathW, outPathUTF8, MAX_PATH);

                mz_zip_reader_extract_to_file(&zip, targetIndex, outPathUTF8, 0);
            }
        }
        else {
            char outPathUTF8[MAX_PATH];
            wchar_to_utf8(outPathW, outPathUTF8, MAX_PATH);
            mz_zip_reader_extract_to_file(&zip, i, outPathUTF8, 0);
        }
    }

    mz_zip_reader_end(&zip);
    return 0;
}