#ifndef ZIPUNZIP_H
#define ZIPUNZIP_H

#include "miniz/miniz.h"

int unzip_to_folder(const wchar_t* zipPath, const wchar_t* outDir);

#endif