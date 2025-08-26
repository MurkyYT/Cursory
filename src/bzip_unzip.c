#include "bzip_unzip.h"

int decompress_bz2(const unsigned char* inBuf, size_t inSize, unsigned char** outBuf, size_t* outSize) {
    size_t outCap = inSize * 4;
    if (outCap < 65536) outCap = 65536;
    *outBuf = (unsigned char*)malloc(outCap);
    if (!*outBuf) return 1;

    unsigned int destLen = (unsigned int)outCap;
    int res;

    const size_t maxOutCap = 1 << 30;

    while (1) {
        destLen = (unsigned int)outCap;
        res = BZ2_bzBuffToBuffDecompress((char*)*outBuf, &destLen,
            (char*)inBuf, (unsigned int)inSize,
            0, 0);
        if (res == BZ_OK) {
            *outSize = destLen;
            return 0;
        }
        else if (res == BZ_OUTBUFF_FULL) {
            if (outCap >= maxOutCap) {
                free(*outBuf);
                *outBuf = NULL;
                *outSize = 0;
                return 1;
            }
            outCap *= 2;
            unsigned char* newBuf = (unsigned char*)realloc(*outBuf, outCap);
            if (!newBuf) {
                free(*outBuf);
                *outBuf = NULL;
                *outSize = 0;
                return 1;
            }
            *outBuf = newBuf;
        }
        else {
            free(*outBuf);
            *outBuf = NULL;
            *outSize = 0;
            return 1;
        }
    }
}

int decompress_bz2_file(const wchar_t* bzPath, unsigned char** outBuf, size_t* outSize)
{
    FILE* f = _wfopen(bzPath, L"rb");
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

    int res = decompress_bz2(buf, size, outBuf, outSize);
    free(buf);
    return res;
}