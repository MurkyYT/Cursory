#include "xz_unzip.h"

int decompress_xz(const Byte* inBuf, size_t inSize, Byte** outBuf, size_t* outSize)
{
    ISzAlloc alloc = { SzAlloc, SzFree };

    Crc64GenerateTable();
    CrcGenerateTable();

    CXzUnpacker state;
    XzUnpacker_Construct(&state, &alloc);
    XzUnpacker_Init(&state);

    size_t inPos = 0;
    size_t outCapacity = 1 << 20;
    *outBuf = (Byte*)SzAlloc(NULL, outCapacity);
    *outSize = 0;

    Byte* tempOut = malloc(1 << 14);
    if (!tempOut)
        return SZ_ERROR_FAIL;

    while (inPos < inSize) {
        SizeT destLen = 1 << 14;
        SizeT srcLen = inSize - inPos;

        int lastChunk = (inPos + srcLen == inSize);

        ECoderStatus status;
        SRes res = XzUnpacker_Code(
            &state,
            tempOut, &destLen,
            inBuf + inPos, &srcLen,
            lastChunk,
            CODER_FINISH_ANY,
            &status);

        if (res != SZ_OK) {
            XzUnpacker_Free(&state);
            SzFree(NULL, *outBuf);
            *outBuf = NULL;
            *outSize = 0;
            return res;
        }

        if (*outSize + destLen > outCapacity) {
            outCapacity *= 2;
            Byte* newBuf = (Byte*)SzAlloc(NULL, outCapacity);
            memcpy(newBuf, *outBuf, *outSize);
            SzFree(NULL, *outBuf);
            *outBuf = newBuf;
        }

        memcpy(*outBuf + *outSize, tempOut, destLen);
        *outSize += destLen;

        inPos += srcLen;

        if (status == CODER_STATUS_FINISHED_WITH_MARK)
            break;
    }

    free(tempOut);

    XzUnpacker_Free(&state);
    return SZ_OK;
}

SRes decompress_xz_file(const wchar_t* xzPath, Byte** outBuf, size_t* outSize)
{
    FILE* f = _wfopen(xzPath, L"rb");
    if (!f)
        return SZ_ERROR_FAIL;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    Byte* buf = (Byte*)malloc(size);
    if (!buf) {
        fclose(f);
        return SZ_ERROR_MEM;
    }

    if (fread(buf, 1, size, f) != size) {
        free(buf);
        fclose(f);
        return SZ_ERROR_FAIL;
    }
    fclose(f);

    SRes res = decompress_xz(buf, size, outBuf, outSize);
    free(buf);
    return res;
}