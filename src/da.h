#ifndef DA_H
#define DA_H

#include <stdlib.h>
#include <string.h>

#define da_define(type, name)        \
    typedef struct {              \
        type* items;              \
        size_t length;            \
        size_t capacity;          \
    } name

#define da_free(da) do {                   \
    free((da)->items);                     \
    (da)->items = NULL;                    \
    (da)->length = 0;                      \
    (da)->capacity = 0;                    \
} while (0)

#define da_resize(da, new_cap) do {                                  \
    void* _new = realloc((da)->items, (new_cap) * sizeof(*(da)->items)); \
    if (_new) {                                                      \
        (da)->items = _new;                                          \
        (da)->capacity = (new_cap);                                  \
    }                                                                \
} while (0)

#define da_append(da, value) do {                                     \
    if ((da)->length == (da)->capacity) {                             \
        size_t _newcap = (da)->capacity == 0 ? 4 : (da)->capacity * 2; \
        void* _new = realloc((da)->items, _newcap * sizeof(*(da)->items)); \
        if (!_new) break;                                             \
        (da)->items = _new;                                           \
        (da)->capacity = _newcap;                                     \
    }                                                                 \
    (da)->items[(da)->length++] = (value);                            \
} while (0)

#define da_remove(da, index) do {                                      \
    if ((index) < (da)->length) {                                      \
        memmove(&(da)->items[index], &(da)->items[index + 1],          \
            ((da)->length - (index) - 1) * sizeof(*(da)->items));      \
        (da)->length--;                                                \
    }                                                                  \
} while (0)

#endif
