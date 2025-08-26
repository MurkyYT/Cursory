#ifndef GZIPUZNIP_H
#define GZIPUNZIP_H

#include <stdint.h>

int decompress_gzip(const unsigned char* inBuf, size_t inSize, unsigned char** outBuf, size_t* outSize);
int decompress_gzip_file(const wchar_t* gzipPath, unsigned char** outBuf, size_t* outSize);

#endif