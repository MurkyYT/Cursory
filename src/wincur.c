#include "wincur.h"

#include <memory.h>
#include <math.h>
#include <stdlib.h>

#pragma region Window.h redefines
typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef long LONG;

typedef struct tagBITMAPINFOHEADER {
    DWORD      biSize;
    LONG       biWidth;
    LONG       biHeight;
    WORD       biPlanes;
    WORD       biBitCount;
    DWORD      biCompression;
    DWORD      biSizeImage;
    LONG       biXPelsPerMeter;
    LONG       biYPelsPerMeter;
    DWORD      biClrUsed;
    DWORD      biClrImportant;
} BITMAPINFOHEADER;

#define BI_RGB        0L
#pragma endregion

static int __append_data(unsigned char** buffer, size_t* offset, size_t* capacity, const void* data, size_t size) {
    if (!buffer || !*buffer || !offset || !capacity || !data) {
        return -1;
    }

    if (*offset + size > *capacity) {
        size_t new_capacity = (*offset + size) * 2;
        if (new_capacity < *offset + size) {
            return -1;
        }

        unsigned char* newbuf = (unsigned char*)realloc(*buffer, new_capacity);
        if (!newbuf) {
            return -1;
        }
        *buffer = newbuf;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *offset, data, size);
    *offset += size;
    return 0;
}

static void write_u32le(unsigned char* ptr, uint32_t val) {
    ptr[0] = val & 0xFF;
    ptr[1] = (val >> 8) & 0xFF;
    ptr[2] = (val >> 16) & 0xFF;
    ptr[3] = (val >> 24) & 0xFF;
}

static void write_u16le(unsigned char* ptr, uint16_t val) {
    ptr[0] = val & 0xFF;
    ptr[1] = (val >> 8) & 0xFF;
}

unsigned char* wincur_create_ARGB_as_CUR(
    const uint32_t* argbPixels,
    int width, int height,
    int xhot, int yhot,
    size_t* outSize)
{
    if (!argbPixels || !outSize || width <= 0 || height <= 0 ||
        width > 256 || height > 256 || xhot < 0 || yhot < 0) {
        return NULL;
    }

    size_t offset = 0, capacity = 4096;
    unsigned char* buffer = (unsigned char*)malloc(capacity);
    if (!buffer) return NULL;

    int pixelCount = width * height;
    int maskRowBytes = ((width + 31) / 32) * 4;
    int andMaskSize = maskRowBytes * height;
    int dibSize = sizeof(BITMAPINFOHEADER) + pixelCount * 4;
    uint32_t imageSize = dibSize + andMaskSize;
    uint32_t imageOffset = 6 + 16;

    uint8_t iconDir[6];
    write_u16le(iconDir + 0, 0);
    write_u16le(iconDir + 2, 2);
    write_u16le(iconDir + 4, 1);

    if (__append_data(&buffer, &offset, &capacity, iconDir, sizeof(iconDir)) != 0) {
        free(buffer);
        return NULL;
    }

    uint8_t entry[16];
    entry[0] = (width == 256) ? 0 : (uint8_t)width;
    entry[1] = (height == 256) ? 0 : (uint8_t)height;
    entry[2] = 0;
    entry[3] = 0;
    write_u16le(entry + 4, (uint16_t)xhot);
    write_u16le(entry + 6, (uint16_t)yhot);
    write_u32le(entry + 8, imageSize);
    write_u32le(entry + 12, imageOffset);

    if (__append_data(&buffer, &offset, &capacity, entry, sizeof(entry)) != 0) {
        free(buffer);
        return NULL;
    }

    BITMAPINFOHEADER bih = { 0 };
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = height * 2;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = 0;

    if (__append_data(&buffer, &offset, &capacity, &bih, sizeof(bih)) != 0) {
        free(buffer);
        return NULL;
    }

    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            uint32_t px = argbPixels[y * width + x];
            uint8_t bgra[4] = {
                (px >> 0) & 0xFF,
                (px >> 8) & 0xFF,
                (px >> 16) & 0xFF,
                (px >> 24) & 0xFF
            };

            if (__append_data(&buffer, &offset, &capacity, bgra, 4) != 0) {
                free(buffer);
                return NULL;
            }
        }
    }

    unsigned char* andMask = (unsigned char*)calloc(andMaskSize, 1);
    if (!andMask) {
        free(buffer);
        return NULL;
    }

    for (int y = 0; y < height; ++y) {
        int maskRow = (height - 1 - y) * maskRowBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t alpha = (argbPixels[y * width + x] >> 24) & 0xFF;
            if (alpha < 128) {
                andMask[maskRow + (x / 8)] |= (1 << (7 - (x % 8)));
            }
        }
    }

    if (__append_data(&buffer, &offset, &capacity, andMask, andMaskSize) != 0) {
        free(buffer);
        free(andMask);
        return NULL;
    }
    free(andMask);

    *outSize = offset;
    return buffer;
}

unsigned char* wincur_create_ANI_from_frames(
    const uint32_t** frames,
    const int* delaysMs,
    int count, int width, int height, int* xhots, int* yhots,
    size_t* outSize)
{
    if (!frames || !delaysMs || !xhots || !yhots || !outSize ||
        count <= 0 || width <= 0 || height <= 0 || width > 256 || height > 256) {
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        if (!frames[i]) return NULL;
    }

    size_t offset = 0, capacity = 4096;
    unsigned char* buffer = (unsigned char*)malloc(capacity);
    if (!buffer) return NULL;

    unsigned char** cursors = (unsigned char**)malloc(count * sizeof(unsigned char*));
    size_t* sizes = (size_t*)malloc(count * sizeof(size_t));
    if (!cursors || !sizes) {
        free(buffer);
        free(cursors);
        free(sizes);
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        cursors[i] = NULL;
    }

    for (int i = 0; i < count; i++) {
        cursors[i] = wincur_create_ARGB_as_CUR(frames[i], width, height, xhots[i], yhots[i], &sizes[i]);
        if (!cursors[i]) {
            goto cleanup_fail;
        }
    }

    if (__append_data(&buffer, &offset, &capacity, "RIFF", 4) != 0) goto cleanup_fail;

    size_t riffSizeOffset = offset;
    uint32_t placeholder = 0;
    if (__append_data(&buffer, &offset, &capacity, &placeholder, 4) != 0) goto cleanup_fail;

    if (__append_data(&buffer, &offset, &capacity, "ACON", 4) != 0) goto cleanup_fail;

    if (__append_data(&buffer, &offset, &capacity, "anih", 4) != 0) goto cleanup_fail;

    uint32_t anihSize = 36;
    uint8_t anihSizeBytes[4];
    write_u32le(anihSizeBytes, anihSize);
    if (__append_data(&buffer, &offset, &capacity, anihSizeBytes, 4) != 0) goto cleanup_fail;

    uint8_t anih[36];
    write_u32le(anih + 0, 36);
    write_u32le(anih + 4, count);
    write_u32le(anih + 8, count);
    write_u32le(anih + 12, 0);
    write_u32le(anih + 16, 0);
    write_u32le(anih + 20, 0);
    write_u32le(anih + 24, 0);
    write_u32le(anih + 28, 1);
    write_u32le(anih + 32, 1);

    if (__append_data(&buffer, &offset, &capacity, anih, 36) != 0) goto cleanup_fail;

    if (__append_data(&buffer, &offset, &capacity, "rate", 4) != 0) goto cleanup_fail;

    uint32_t rateSize = count * 4;
    uint8_t rateSizeBytes[4];
    write_u32le(rateSizeBytes, rateSize);
    if (__append_data(&buffer, &offset, &capacity, rateSizeBytes, 4) != 0) goto cleanup_fail;

    for (int i = 0; i < count; i++) {

        uint32_t jiffies;
        if (delaysMs[i] == 0)
            jiffies = 0;
        else
        {
            jiffies = (uint32_t)round(delaysMs[i] * 60.0 / 1000.0);
            if (jiffies == 0) jiffies = 1;
        }

        uint8_t jiffyBytes[4];
        write_u32le(jiffyBytes, jiffies);
        if (__append_data(&buffer, &offset, &capacity, jiffyBytes, 4) != 0) goto cleanup_fail;
    }

    if (__append_data(&buffer, &offset, &capacity, "seq ", 4) != 0) goto cleanup_fail;
    if (__append_data(&buffer, &offset, &capacity, rateSizeBytes, 4) != 0) goto cleanup_fail;

    for (int i = 0; i < count; i++) {
        uint8_t indexBytes[4];
        write_u32le(indexBytes, (uint32_t)i);
        if (__append_data(&buffer, &offset, &capacity, indexBytes, 4) != 0) goto cleanup_fail;
    }

    if (__append_data(&buffer, &offset, &capacity, "LIST", 4) != 0) goto cleanup_fail;

    size_t listSizeOffset = offset;
    if (__append_data(&buffer, &offset, &capacity, &placeholder, 4) != 0) goto cleanup_fail;
    if (__append_data(&buffer, &offset, &capacity, "fram", 4) != 0) goto cleanup_fail;

    for (int i = 0; i < count; i++) {
        if (__append_data(&buffer, &offset, &capacity, "icon", 4) != 0) goto cleanup_fail;

        uint8_t frameSizeBytes[4];
        write_u32le(frameSizeBytes, (uint32_t)sizes[i]);
        if (__append_data(&buffer, &offset, &capacity, frameSizeBytes, 4) != 0) goto cleanup_fail;
        if (__append_data(&buffer, &offset, &capacity, cursors[i], sizes[i]) != 0) goto cleanup_fail;

        if (sizes[i] % 2 != 0) {
            uint8_t pad = 0;
            if (__append_data(&buffer, &offset, &capacity, &pad, 1) != 0) goto cleanup_fail;
        }
    }

    uint32_t listChunkSize = (uint32_t)(offset - listSizeOffset - 4);
    write_u32le(buffer + listSizeOffset, listChunkSize);

    uint32_t riffChunkSize = (uint32_t)(offset - 8);
    write_u32le(buffer + riffSizeOffset, riffChunkSize);

    for (int i = 0; i < count; i++) {
        free(cursors[i]);
    }
    free(cursors);
    free(sizes);

    *outSize = offset;
    return buffer;

cleanup_fail:
    if (cursors) {
        for (int i = 0; i < count; i++) {
            free(cursors[i]);
        }
        free(cursors);
    }
    free(sizes);
    free(buffer);
    return NULL;
}