#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <proto/intuition.h>
#include <proto/graphics.h>


static void ARGB_to_RGB(png_byte *ptr, uint32 ARGB)
{
	ptr[0] = (ARGB & 0xFF0000) >> 16;
	ptr[1] = (ARGB & 0x00FF00) >> 8;
	ptr[2] = (ARGB & 0x0000FF);
}

void amigaos_screenshot();
int amigaos_snapshot(char* filename, char* title, int width, int height, char *buffer,int BytesPerRow, int BytesPerPixel );

extern struct Window *Menu_Window;
extern char *SCREENSHOTDIR;

void amigaos_screenshot()
{
	struct Screen *src = Menu_Window -> WScreen;
	uint32	bpr;
	APTR	adr;

	char 	*temp;

	uint32 	width = GetBitMapAttr( src->RastPort.BitMap, BMA_WIDTH);
	uint32	height = GetBitMapAttr( src->RastPort.BitMap, BMA_HEIGHT);
	uint32	bpp = GetBitMapAttr( src->RastPort.BitMap, BMA_BYTESPERPIXEL);

	APTR	lock = LockBitMapTags (  src->RastPort.BitMap,
					LBM_BytesPerRow, &bpr,
					LBM_BaseAddress, &adr,
					TAG_END);

	int		size = bpr * height *2;

	if (temp = malloc( size ))
	{
		if (lock)
		{
			char *path_with_name;

			memcpy(temp,adr, size );
			UnlockBitMap(lock);

			if (path_with_name = to_name_and_path(SCREENSHOTDIR,"mplayer.png"))
			{
				amigaos_snapshot(path_with_name, "MPlayer", width,  height, temp, bpr, bpp );	
			}
			free(temp);
		}

	}
}

int amigaos_snapshot(char* filename, char* title, int width, int height, char *buffer,int BytesPerRow, int BytesPerPixel )
{
	int code = 0;
	FILE *fp = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytep row;

	uint32 ARGB;	


	// Open file for writing (binary mode)
	fp = fopen(filename, "wb");
	if (fp == NULL) {
		fprintf(stderr, "Could not open file %s for writing\n", filename);
		code = 1;
		goto finalise;
	}

	// Initialize write structure

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		fprintf(stderr, "Could not allocate write struct\n");
		code = 1;
		goto finalise;
	}

	// Initialize info structure
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fprintf(stderr, "Could not allocate info struct\n");
		code = 1;
		goto finalise;
	}

	// Setup Exception handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "Error during png creation\n");
		code = 1;
		goto finalise;
	}

	png_init_io(png_ptr, fp);

	// Write header (8 bit colour depth)
	png_set_IHDR(png_ptr, info_ptr, width, height,
			8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	// Set title
	if (title != NULL) {
		png_text title_text;
		title_text.compression = PNG_TEXT_COMPRESSION_NONE;
		title_text.key = "Title";
		title_text.text = title;
		png_set_text(png_ptr, info_ptr, &title_text, 1);
	}

	png_write_info(png_ptr, info_ptr);

	// Allocate memory for one row (3 bytes per pixel - RGB)
	row = (png_bytep) malloc(3 * width * sizeof(png_byte));

	// Write image data
	int x, y;
	for (y=0 ; y<height ; y++) {
		for (x=0 ; x<width ; x++) {
			ARGB =  *( (uint32 *) ( buffer + (y*BytesPerRow) + (x*BytesPerPixel) ) );
			ARGB_to_RGB(&(row[x*3]), ARGB);
		}
		png_write_row(png_ptr, row);
	}

	// End write
	png_write_end(png_ptr, NULL);

	finalise:
	if (fp != NULL) fclose(fp);
	if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
	if (row != NULL) free(row);

	return code;
}
