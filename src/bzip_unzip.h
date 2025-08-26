#ifndef BZIPUNZIP_H
#define BZIPUNZIP_H

#include "bzip/bzlib.h"

int decompress_bz2(const unsigned char* inBuf, size_t inSize, unsigned char** outBuf, size_t* outSize);
int decompress_bz2_file(const wchar_t* bzPath, unsigned char** outBuf, size_t* outSize);

#endif
