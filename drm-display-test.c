/*
 * Copyright (C) 2019-2021 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2020 Bootlin
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include <cairo.h>

#include <drm-display.h>

static int draw_text(struct drm_display_buffer *buffer, const char *text)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	cairo_text_extents_t extents;
	int ret = -1;
	double x, y;

	if (!buffer || !buffer->data[0] || !text ||
	    buffer->format != DRM_FORMAT_XRGB8888 ||
	    buffer->width > INT_MAX || buffer->height > INT_MAX ||
	    buffer->strides[0] > INT_MAX)
		return -EINVAL;

	surface = cairo_image_surface_create_for_data(buffer->data[0],
						       CAIRO_FORMAT_RGB24,
						       buffer->width,
						       buffer->height,
						       buffer->strides[0]);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
		goto complete;

	cr = cairo_create(surface);
	if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
		cairo_destroy(cr);
		goto complete;
	}

	/* Paint a dark background, then center the supplied string in white. */
	cairo_set_source_rgb(cr, 0.08, 0.12, 0.20);
	cairo_paint(cr);

	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 48.0);
	cairo_text_extents(cr, text, &extents);
	x = (buffer->width - extents.width) / 2.0 - extents.x_bearing;
	y = (buffer->height - extents.height) / 2.0 - extents.y_bearing;
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_move_to(cr, x, y);
	cairo_show_text(cr, text);

	if (cairo_status(cr) == CAIRO_STATUS_SUCCESS)
		ret = 0;

	cairo_destroy(cr);

complete:
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
	return ret;
}

static int test_color(struct drm_display *display, const char *first_text,
		      const char *second_text)
{
	struct drm_display_buffer *buffer;
	int ret;

	display->primary_setup.buffer_format = DRM_FORMAT_XRGB8888;

	ret = drm_display_probe(display);
	if (ret)
		return 1;

	ret = drm_display_setup(display);
	if (ret)
		return ret;

	buffer = drm_display_primary_buffer_cycle(display);
	if (!buffer)
		return 1;

	ret = draw_text(buffer, first_text);
	if (ret) {
		fprintf(stderr, "Could not draw text in first frame\n");
		return ret;
	}

	ret = drm_display_configure(display, &display->primary_setup, buffer);
	if (ret)
		return ret;

	buffer = drm_display_primary_buffer_cycle(display);
	if (!buffer)
		return 1;

	printf("Press enter to continue ");
	getchar();

	ret = draw_text(buffer, second_text);
	if (ret) {
		fprintf(stderr, "Could not draw text in second frame\n");
		return ret;
	}

	ret = drm_display_page_flip(display, &display->primary_setup, buffer);
	if (ret)
		return ret;

	printf("Press enter to continue ");
	getchar();

	buffer = drm_display_primary_buffer_cycle(display);
	if (!buffer)
		return 1;

	ret = drm_display_page_flip(display, &display->primary_setup, buffer);
	if (ret)
		return ret;

	return 0;
}

int main(int argc, char *argv[])
{
	struct drm_display display = { 0 };
	int ret;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <first text> <second text>\n", argv[0]);
		return 1;
	}

	ret = drm_display_open(&display);
	if (ret)
		return 1;

	ret = test_color(&display, argv[1], argv[2]);
	if (ret) {
		if (display.up) {
			int teardown_ret = drm_display_teardown(&display);
			if (teardown_ret)
				fprintf(stderr, "Display restore failed: %s\n",
					strerror(-teardown_ret));
		}
		drm_display_close(&display);
		return 1;
	}

	printf("Press enter to continue ");
	getchar();

	ret = drm_display_teardown(&display);
	if (ret)
		fprintf(stderr, "Display restore failed: %s\n",
			strerror(-ret));

	drm_display_close(&display);

	return ret ? 1 : 0;
}
