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


#ifndef __CAIROVIS_COLORMAP_H__
#define __CAIROVIS_COLORMAP_H__


#include <glib.h>
#include <glib-object.h>
#include <gsl/gsl_spline.h>


G_BEGIN_DECLS
#include "colormap_data.h"
#define CAIROVIS_COLORMAP_TYPE \
(cairovis_colormap_get_type())
    typedef struct
{
  size_t len;
  double *x;
  double *y;
} colormap_channel_data;

typedef struct
{
  colormap_channel_data red, green, blue;
} colormap_data;


typedef struct
{
  gsl_spline *spline;
  gsl_interp_accel *accel;
} colormap_channel;


typedef struct
{
  colormap_channel red, green, blue;
} colormap;


gboolean colormap_get_data_by_name (enum cairovis_colormap_name key,
    colormap_data * data);
colormap *colormap_create_by_name (enum cairovis_colormap_name key);
void colormap_destroy (colormap * map);
guint32 colormap_map (colormap * map, double x);


GType cairovis_colormap_get_type (void);


G_END_DECLS
#endif /* __CAIROVIS_COLORMAP_H__ */
