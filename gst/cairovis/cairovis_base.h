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


#ifndef __CAIROVIS_BASE_H__
#define __CAIROVIS_BASE_H__


#include <glib.h>
#include <gst/gst.h>

#include <cairo.h>


G_BEGIN_DECLS
#define CAIROVIS_BASE_TYPE \
	(cairovis_base_get_type())
#define CAIROVIS_BASE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), CAIROVIS_BASE_TYPE, CairoVisBase))
#define CAIROVIS_BASE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), CAIROVIS_BASE_TYPE, CairoVisBaseClass))
#define GST_IS_CAIROVIS_BASE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), CAIROVIS_BASE_TYPE))
#define GST_IS_CAIROVIS_BASE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), CAIROVIS_BASE_TYPE))
#define CAIROVIS_SCALE_TYPE \
	(cairovis_scale_get_type())
    enum cairovis_scale
{
  CAIROVIS_SCALE_LINEAR,
  CAIROVIS_SCALE_LOG,
};


enum cairovis_cardinal_direction
{
  CAIROVIS_NORTH,
  CAIROVIS_SOUTH,
  CAIROVIS_EAST,
  CAIROVIS_WEST
};


struct cairovis_axis_spec
{
  enum cairovis_scale scale;
  enum cairovis_cardinal_direction which_side;
  double device_max;
  double data_min;
  double data_max;
};


typedef struct
{
  GstElementClass parent_class;
} CairoVisBaseClass;


typedef struct _CairoVisBase
{
  GstElement element;

  /* Pads */
  GstPad *srcpad;

  /* Properties */
  enum cairovis_scale xscale, yscale;
  gchar *title, *xlabel, *ylabel;
  gboolean xautoscale, yautoscale;
  gdouble xmin, xmax, ymin, ymax;
} CairoVisBase;


gboolean cairovis_base_negotiate_srcpad (CairoVisBase * element);
GstFlowReturn cairovis_base_buffer_surface_alloc (CairoVisBase * element,
    GstBuffer ** buf, cairo_surface_t ** surf, gint * width, gint * height);
void cairovis_draw_axis (cairo_t * restrict cr,
    const struct cairovis_axis_spec *restrict axis);
void cairovis_draw_axes (CairoVisBase * element, cairo_t * cr, gint width,
    gint height);


GType cairovis_base_get_type (void);
GType cairovis_scale_get_type (void);


G_END_DECLS
#endif /* __CAIROVIS_BASE_H__ */
