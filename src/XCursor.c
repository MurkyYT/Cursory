#include "XCursor.h"

#include <memory.h>
#include <stdlib.h>

bool xcur_correct_file(const wchar_t* path)
{
	XCurFile file = { 0 };
	if (!xcur_read_file(path, &file)) return false;
	xcur_close_file(&file);
	return true;
}

bool xcur_read_file(const wchar_t* path, XCurFile* file)
{
	file->fd = _wfopen(path, L"rb");
	if (!file->fd) return false;

	fread(file->magic, sizeof(char[4]), 1, file->fd);
	fread(&file->header, sizeof(CARD32), 1, file->fd);
	fread(&file->version, sizeof(CARD32), 1, file->fd);
	fread(&file->ntoc, sizeof(CARD32), 1, file->fd);

	if (memcmp(file->magic, "Xcur", 4) != 0) {
		fclose(file->fd);
		return false;
	}

	file->toc = (Xtoc*)calloc(file->ntoc, sizeof(Xtoc));

	if (!file->toc)
	{
		fclose(file->fd);
		return false;
	}

	if (fread(file->toc, sizeof(Xtoc), file->ntoc, file->fd) != file->ntoc) {
		free(file->toc);
		fclose(file->fd);
		return false;
	}

	return true;
}

CARD32 xcur_get_type(XCurFile* file, size_t ntoc)
{
	return file->toc[ntoc].type;
}

bool xcur_read_xchunk(XCurFile* file, size_t ntoc, XChunk* chunk)
{
	fseek(file->fd, file->toc[ntoc].position, SEEK_SET);

	fread(&chunk->header, sizeof(CARD32), 1, file->fd);
	fread(&chunk->type, sizeof(CARD32), 1, file->fd);
	fread(&chunk->subtype, sizeof(CARD32), 1, file->fd);
	fread(&chunk->version, sizeof(CARD32), 1, file->fd);

	if (chunk->type != file->toc[ntoc].type)
		return false;

	if (chunk->subtype != file->toc[ntoc].subtype)
		return false;

	return true;
}

bool xcur_read_ximage(XCurFile* file, size_t ntoc, XImage* image)
{
	fseek(file->fd, file->toc[ntoc].position, SEEK_SET);

	fread(&image->header, sizeof(CARD32), 1, file->fd);
	fread(&image->type, sizeof(CARD32), 1, file->fd);
	fread(&image->subtype, sizeof(CARD32), 1, file->fd);
	fread(&image->version, sizeof(CARD32), 1, file->fd);
	fread(&image->width, sizeof(CARD32), 1, file->fd);
	fread(&image->height, sizeof(CARD32), 1, file->fd);
	fread(&image->xhot, sizeof(CARD32), 1, file->fd);
	fread(&image->yhot, sizeof(CARD32), 1, file->fd);
	fread(&image->delay, sizeof(CARD32), 1, file->fd);

	if (image->width > 0x7fff)
		return false;
	if (image->height > 0x7fff)
		return false;
	if (image->xhot > image->width)
		return false;
	if (image->yhot > image->height)
		return false;

	CARD32 imageSize = image->width * image->height;
	image->pixels = (XPixel*)calloc(imageSize, sizeof(XPixel));
	if (!image->pixels) return false;
	fread(image->pixels, sizeof(XPixel), imageSize, file->fd);

	return true;
}

bool xcur_read_xcurosr(XCurFile* file, XCursor* cursor, CARD32 nominal, CARD32 maxNominal)
{
	if (nominal == 0)
	{
		for (int i = file->ntoc - 1; i >= 0; i--) {
			if (xcur_get_type(file, i) == XCurType_Image) {
				XChunk chunk;
				if (!xcur_read_xchunk(file, i, &chunk))
					return false;

				if (chunk.subtype <= maxNominal || maxNominal == 0)
				{
					nominal = chunk.subtype;
					break;
				}
			}
		}
	}

	size_t count = 0;

	for (size_t i = 0; i < file->ntoc; i++) {
		if (xcur_get_type(file, i) == XCurType_Image) {
			XChunk chunk;
			if (!xcur_read_xchunk(file, i, &chunk))
				return false;

			if (chunk.subtype == nominal)
				count++;
		}
	}

	cursor->nframes = (CARD32)count;

	if (count < 1) return false;

	cursor->animated = count > 1;
	cursor->frames = (XImage*)calloc(count, sizeof(XImage));

	if (!cursor->frames) return false;

	size_t idx = 0;
	for (size_t i = 0; i < file->ntoc && idx < count; i++) {
		if (xcur_get_type(file, i) == XCurType_Image) {
			XChunk chunk;
			if (!xcur_read_xchunk(file, i, &chunk))
				return false;

			if (chunk.subtype == nominal) {
				if (!xcur_read_ximage(file, i, &cursor->frames[idx++]))
				{
					xcur_free_cursor(cursor);
					return false;
				}
			}
		}
	}

	return true;
}

void xcur_free_cursor(XCursor* cursor) {
	if (!cursor || !cursor->frames) return;

	for (CARD32 i = 0; i < cursor->nframes; i++) {
		if (cursor->frames[i].pixels)
			free(cursor->frames[i].pixels);
	}
	free(cursor->frames);
	cursor->frames = NULL;
	cursor->nframes = 0;
}

void xcur_close_file(XCurFile* file) {
	if (!file) return;
	if (file->fd) fclose(file->fd);
	if (file->toc) free(file->toc);
	file->fd = NULL;
	file->toc = NULL;
}