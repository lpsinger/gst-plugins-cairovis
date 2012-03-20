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


#ifndef __CAIROVIS_WATERFALL_H__
#define __CAIROVIS_WATERFALL_H__


#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "colormap.h"
#include "cairovis_base.h"


G_BEGIN_DECLS
#define CAIROVIS_WATERFALL_TYPE \
	(cairovis_waterfall_get_type())
#define CAIROVIS_WATERFALL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), CAIROVIS_WATERFALL_TYPE, CairoVisWaterfall))
#define CAIROVIS_WATERFALL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), CAIROVIS_WATERFALL_TYPE, CairoVisWaterfallClass))
#define GST_IS_CAIROVIS_WATERFALL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), CAIROVIS_WATERFALL_TYPE))
#define GST_IS_CAIROVIS_WATERFALL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), CAIROVIS_WATERFALL_TYPE))
    typedef struct
{
  CairoVisBaseClass parent_class;
} CairoVisWaterfallClass;


typedef struct _CairoVisWaterfall
{
  CairoVisBase element;

  /* Pads */
  GstPad *sinkpad;
  GstAdapter *adapter;

  /* Properties */
  GstClockTime history;
  enum cairovis_scale zscale;
  gchar *zlabel;
  gboolean zautoscale;
  gdouble zmin, zmax;
  gboolean colorbar;

  /* Internal data */
  gint nchannels;
  gint rate;
  GstClockTime t0;
  guint64 offset0;
  guint64 last_offset_end;
  guint64 frame_number;
  enum cairovis_colormap_name map_name;
  colormap *map;
} CairoVisWaterfall;


GType cairovis_waterfall_get_type (void);


G_END_DECLS
#endif /* __CAIROVIS_WATERFALL_H__ */
