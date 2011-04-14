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


#ifndef __CAIROVIS_HISTOGRAM_H__
#define __CAIROVIS_HISTOGRAM_H__


#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "cairovis_base.h"


G_BEGIN_DECLS
#define CAIROVIS_HISTOGRAM_TYPE \
	(cairovis_histogram_get_type())
#define CAIROVIS_HISTOGRAM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), CAIROVIS_HISTOGRAM_TYPE, CairoVisHistogram))
#define CAIROVIS_HISTOGRAM_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), CAIROVIS_HISTOGRAM_TYPE, CairoVisHistogramClass))
#define GST_IS_CAIROVIS_HISTOGRAM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), CAIROVIS_HISTOGRAM_TYPE))
#define GST_IS_CAIROVIS_HISTOGRAM_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), CAIROVIS_HISTOGRAM_TYPE))
#define CAIROVIS_HISTOGRAM_BINS_TYPE \
(cairovis_histogram_bins_get_type())
    enum cairovis_histogram_bins
{
  CAIROVIS_BINS_LINEAR,
  CAIROVIS_BINS_LOG
};


typedef struct
{
  CairoVisBaseClass parent_class;
} CairoVisHistogramClass;


typedef struct _CairoVisHistogram
{
  CairoVisBase element;

  /* Pads */
  GstPad *sinkpad;

  /* Properties */
  enum cairovis_histogram_bins bins;
  gdouble min;
  gdouble max;
  guint nbins;
  guint history;
  gboolean normed;

  /* Internal data */
  guint *bin_counts;
  guint total;
  GstAdapter *adapter;
  gdouble scale;
} CairoVisHistogram;


GType cairovis_histogram_get_type (void);


GType cairovis_histogram_bins_get_type (void);


G_END_DECLS
#endif /* __CAIROVIS_HISTOGRAM_H__ */
