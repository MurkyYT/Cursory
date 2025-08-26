#include "tar_unzip.h"

#include <Windows.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include "da.h"

#pragma warning(disable: 4996)

#define BLOCK_SIZE 512

typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} TarHeader;

typedef struct
{
    char name[100];
    char symlink[100];
} Symlink;

da_define(Symlink, Symlinks);

unsigned long oct2int(const char* str, size_t size) {
    unsigned long val = 0;
    while (size-- && *str >= '0' && *str <= '7') {
        val = (val << 3) + (*str++ - '0');
    }
    return val;
}

void tar_create_dirs_w(const wchar_t* path) {
    wchar_t tmp[MAX_PATH];
    wcsncpy(tmp, path, MAX_PATH);
    for (wchar_t* p = tmp + 1; *p; p++) {
        if (*p == L'\\') {
            *p = 0;
            _wmkdir(tmp);
            *p = L'\\';
        }
        if (*p == L'/') {
            *p = 0;
            _wmkdir(tmp);
            *p = L'/';
        }
    }
}

int extract_tar(const wchar_t* tarPath, const wchar_t* outDir) {
    FILE* f = _wfopen(tarPath, L"rb");
    if (!f) {
        return 0;
    }

    TarHeader header;
    Symlinks symlinks = { 0 };

    while (fread(&header, 1, BLOCK_SIZE, f) == BLOCK_SIZE) 
    {
        if (header.name[0] == '\0')
            break;
        if (header.typeflag == '5') // Skip folders
            continue;

        if (header.typeflag == '2') // Add symlink for later
        {
            Symlink symlink = { 0 };
            strcpy(symlink.name, header.name);
            strcpy(symlink.symlink, header.linkname);
            da_append(&symlinks, symlink);
            continue;
        }

        unsigned long fileSize = oct2int(header.size, sizeof(header.size));
        char* data = (char*)malloc(fileSize);
        if (!data) break;

        fread(data, 1, fileSize, f);
        size_t padding = ((fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE - fileSize;
        if (padding) fseek(f, (long)padding, SEEK_CUR);

        wchar_t fileNameW[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, header.name, -1, fileNameW, MAX_PATH);

        wchar_t fullPath[MAX_PATH];
        swprintf(fullPath, MAX_PATH, L"%s\\%s", outDir, fileNameW);

        tar_create_dirs_w(fullPath);

        FILE* out = _wfopen(fullPath, L"wb");
        if (out) {
            fwrite(data, 1, fileSize, out);
            fclose(out);
        }

        free(data);
    }

    // Copy symlinks
    for (size_t i = 0; i < symlinks.length; i++) 
    {
        Symlink item = symlinks.items[i];

        wchar_t destRel[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, item.name, -1, destRel, MAX_PATH);

        wchar_t srcRel[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, item.symlink, -1, srcRel, MAX_PATH);

        wchar_t destFull[MAX_PATH];
        swprintf(destFull, MAX_PATH, L"%s\\%s", outDir, destRel);

        for (wchar_t* p = destFull; *p; ++p)
            if (*p == L'/') *p = L'\\';

        wchar_t baseDir[MAX_PATH];
        wcscpy(baseDir, destFull);
        PathRemoveFileSpecW(baseDir);

        wchar_t srcFull[MAX_PATH];
        swprintf(srcFull, MAX_PATH, L"%s\\%s", baseDir, srcRel);

        for (wchar_t* p = srcFull; *p; ++p)
            if (*p == L'/') *p = L'\\';

        if (_waccess(srcFull, 0) == 0) {
            tar_create_dirs_w(destFull);
            CopyFileW(srcFull, destFull, FALSE);
        }
    }

    free(symlinks.items);
    fclose(f);
    return 1;
}
