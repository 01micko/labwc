// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Johan Malm 2023
 */
#define _POSIX_C_SOURCE 200809L
#include <cairo.h>
#include <png.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "img/img-png.h"
#include "common/string-helpers.h"
#include "labwc.h"

/*
 * cairo_image_surface_create_from_png() does not gracefully handle non-png
 * files, so we verify the header before trying to read the rest of the file.
 */
#define PNG_BYTES_TO_CHECK (4)
static bool
ispng(const char *filename)
{
	unsigned char header[PNG_BYTES_TO_CHECK];
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		return false;
	}
	if (fread(header, 1, PNG_BYTES_TO_CHECK, fp) != PNG_BYTES_TO_CHECK) {
		fclose(fp);
		return false;
	}
	if (png_sig_cmp(header, (png_size_t)0, PNG_BYTES_TO_CHECK)) {
		wlr_log(WLR_ERROR, "file '%s' is not a recognised png file", filename);
		fclose(fp);
		return false;
	}
	fclose(fp);
	return true;
}

#undef PNG_BYTES_TO_CHECK

void
img_png_load(const char *filename, struct lab_data_buffer **buffer)
{
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}
	if (string_null_or_empty(filename)) {
		return;
	}
	if (!ispng(filename)) {
		return;
	}

	cairo_surface_t *image = cairo_image_surface_create_from_png(filename);
	if (cairo_surface_status(image)) {
		wlr_log(WLR_ERROR, "error reading png button '%s'", filename);
		cairo_surface_destroy(image);
		return;
	}
	cairo_surface_flush(image);

	if (cairo_image_surface_get_format(image) == CAIRO_FORMAT_ARGB32) {
		/* we can wrap ARGB32 image surfaces directly */
		*buffer = buffer_adopt_cairo_surface(image);
		return;
	}

	/* convert to ARGB32 by painting to a new surface */
	uint32_t w = cairo_image_surface_get_width(image);
	uint32_t h = cairo_image_surface_get_height(image);
	*buffer = buffer_create_cairo(w, h, 1);
	cairo_t *cairo = (*buffer)->cairo;
	cairo_set_source_surface(cairo, image, 0, 0);
	cairo_paint(cairo);
	cairo_surface_flush((*buffer)->surface);
	/* destroy original surface */
	cairo_surface_destroy(image);
}
