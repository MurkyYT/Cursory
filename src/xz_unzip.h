#ifndef XZUNZIP_H
#define XZUNZIP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzma/7zAlloc.h"
#include "lzma/XzCrc64.h"
#include "lzma/7zCrc.h"
#include "lzma/Xz.h"

int decompress_xz(const Byte* inBuf, size_t inSize, Byte** outBuf, size_t* outSize);
SRes decompress_xz_file(const wchar_t* xzPath, Byte** outBuf, size_t* outSize);

#endif