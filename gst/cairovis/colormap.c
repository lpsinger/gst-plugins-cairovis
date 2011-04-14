/*
 * Copyright (C) 2010 Leo Singer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <colormap.h>


static colormap_channel channel_for_data(colormap_channel_data *data)
{
	colormap_channel channel;
	channel.accel = gsl_interp_accel_alloc();
	channel.spline = gsl_spline_alloc(gsl_interp_linear, data->len);
	gsl_spline_init(channel.spline, data->x, data->y, data->len);

	/* FIXME: move these into a separate destructor for colormap_channel_data? */
	g_free(data->x);
	g_free(data->y);

	return channel;
}


/* FIXME: it would be nice to have shared instances of colormaps, instead of each element having its own colormaps. */
colormap *colormap_create_by_name(guint name)
{
	colormap *map;
	colormap_data data;

	g_return_val_if_fail(colormap_get_data_by_name(name, &data), NULL);
	map = g_malloc(sizeof(colormap));

	map->red = channel_for_data(&data.red);
	map->green = channel_for_data(&data.green);
	map->blue = channel_for_data(&data.blue);

	return map;
}


static void colormap_destroy_channel(colormap_channel *channel)
{
	gsl_spline_free(channel->spline);
	gsl_interp_accel_free(channel->accel);
}


void colormap_destroy(colormap *map)
{
	if (map == NULL)
		return;

	colormap_destroy_channel(&map->red);
	colormap_destroy_channel(&map->green);
	colormap_destroy_channel(&map->blue);
	g_free(map);
}


guint8 colormap_map_channel(colormap_channel *channel, double x)
{
	return gsl_spline_eval(channel->spline, x, channel->accel) * 0xFF;
}


guint32 colormap_map(colormap *map, double x)
{
	guint32 color = 0;
	color |= (guint32) colormap_map_channel(&map->red, x);
	color <<= 8;
	color |= (guint32) colormap_map_channel(&map->green, x);
	color <<= 8;
	color |= (guint32) colormap_map_channel(&map->blue, x);

	return color;
}
