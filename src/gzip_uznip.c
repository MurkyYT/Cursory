#include "gzip_unzip.h"

#include <stdio.h>
#include <stdlib.h>
#include "zlib/zlib.h"

int decompress_gzip(const unsigned char* inBuf, size_t inSize,
    unsigned char** outBuf, size_t* outSize) {
    size_t outCap = inSize * 4;
    if (outCap < 65536) outCap = 65536;

    *outBuf = (unsigned char*)malloc(outCap);
    if (!*outBuf) return 1;

    z_stream strm = { 0 };
    strm.next_in = (Bytef*)inBuf;
    strm.avail_in = (uInt)inSize;

    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
        free(*outBuf);
        return 1;
    }

    size_t outPos = 0;
    int ret;

    do {
        if (outPos == outCap) {
            outCap *= 2;
            unsigned char* tmp = realloc(*outBuf, outCap);
            if (!tmp) {
                inflateEnd(&strm);
                free(*outBuf);
                return 1;
            }
            *outBuf = tmp;
        }

        strm.next_out = *outBuf + outPos;
        strm.avail_out = (uInt)(outCap - outPos);

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            free(*outBuf);
            return 1;
        }

        outPos = outCap - strm.avail_out;
    } while (ret != Z_STREAM_END);

    *outSize = outPos;
    inflateEnd(&strm);
    return 0;
}

int decompress_gzip_file(const wchar_t* gzipPath, unsigned char** outBuf, size_t* outSize)
{
    FILE* f = _wfopen(gzipPath, L"rb");
    if (!f)
        return 1;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char* buf = (unsigned char*)malloc(size);
    if (!buf) {
        fclose(f);
        return 1;
    }

    if (fread(buf, 1, size, f) != size) {
        free(buf);
        fclose(f);
        return 1;
    }
    fclose(f);

    int res = decompress_gzip(buf, size, outBuf, outSize);
    free(buf);
    return res;
}