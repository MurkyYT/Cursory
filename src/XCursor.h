#ifndef XCURSOR_H
#define XCURSOR_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define XCurType_Comment 0xfffe0001U
#define XCurType_Image 0xfffd0002U

typedef uint32_t CARD32;

typedef struct {
	CARD32 header;
	CARD32 type;
	CARD32 subtype;
	CARD32 version;
} XChunk;

typedef struct {
	unsigned char a, r, g, b;
} XPixel;

typedef struct {
	CARD32 header;
	CARD32 type;
	CARD32 subtype;
	CARD32 version;
	CARD32 width;
	CARD32 height;
	CARD32 xhot;
	CARD32 yhot;
	CARD32 delay;

	XPixel* pixels;
} XImage;

typedef struct {
	CARD32 nframes;
	bool animated;

	XImage* frames;
} XCursor;

typedef struct {
	CARD32 type;
	CARD32 subtype;
	CARD32 position;
} Xtoc;

typedef struct {
	char magic[4];
	CARD32 header;
	CARD32 version;
	CARD32 ntoc;

	Xtoc* toc;
	FILE* fd;
} XCurFile;

bool xcur_correct_file(const wchar_t* path);
bool xcur_read_file(const wchar_t* path, XCurFile* file);
CARD32 xcur_get_type(XCurFile* file, size_t ntoc);
bool xcur_read_xchunk(XCurFile* file, size_t ntoc, XChunk* chunk);
bool xcur_read_ximage(XCurFile* file, size_t ntoc, XImage* image);
bool xcur_read_xcurosr(XCurFile* file, XCursor* cursor, CARD32 nominal, CARD32 maxNominal);
void xcur_free_cursor(XCursor* cursor);
void xcur_close_file(XCurFile* file);

#endif