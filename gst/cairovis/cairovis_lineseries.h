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


#ifndef __CAIROVIS_LINESERIES_H__
#define __CAIROVIS_LINESERIES_H__


#include <glib.h>
#include <gst/gst.h>

#include "cairovis_base.h"


G_BEGIN_DECLS
#define CAIROVIS_LINESERIES_TYPE \
	(cairovis_lineseries_get_type())
#define CAIROVIS_LINESERIES(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), CAIROVIS_LINESERIES_TYPE, CairoVisLineSeries))
#define CAIROVIS_LINESERIES_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), CAIROVIS_LINESERIES_TYPE, CairoVisLineSeriesClass))
#define GST_IS_CAIROVIS_LINESERIES(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), CAIROVIS_LINESERIES_TYPE))
#define GST_IS_CAIROVIS_LINESERIES_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), CAIROVIS_LINESERIES_TYPE))
    typedef struct
{
  CairoVisBaseClass parent_class;
} CairoVisLineSeriesClass;


typedef struct _CairoVisLineSeries
{
  CairoVisBase element;

  /* Pads */
  GstPad *sinkpad;

  /* Internal data */
  gint nchannels;
} CairoVisLineSeries;


GType cairovis_lineseries_get_type (void);


G_END_DECLS
#endif /* __CAIROVIS_LINESERIES_H__ */
