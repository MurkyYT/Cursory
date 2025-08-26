#ifndef TARUNZIP_H
#define TARUNZIP_H

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>

int extract_tar(const wchar_t* tarPath, const wchar_t* outDir);

#endif