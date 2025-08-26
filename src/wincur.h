#ifndef WINCUR_H
#define WINCUR_h

#include <stdint.h>

unsigned char* wincur_create_ARGB_as_CUR(
    const uint32_t* argbPixels,
    int width, int height,
    int xhot, int yhot,
    size_t* outSize);

unsigned char* wincur_create_ANI_from_frames(
    const uint32_t** frames,
    const int* delaysMs,
    int count, int width, int height, int* xhots, int* yhots,
    size_t* outSize);

#endif
