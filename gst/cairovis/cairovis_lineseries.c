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


#include <cairovis_lineseries.h>

#include <math.h>


static gboolean
sink_setcaps (GstPad *pad, GstCaps *caps)
{
  CairoVisLineSeries *element = CAIROVIS_LINESERIES (gst_pad_get_parent (pad));
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  gboolean success =
      gst_structure_get_int (structure, "channels", &element->nchannels);

  gst_object_unref (element);

  return success;
}


static GstFlowReturn
chain (GstPad *pad, GstBuffer *inbuf)
{
  CairoVisLineSeries *element = CAIROVIS_LINESERIES (gst_pad_get_parent (pad));
  CairoVisBase *base = CAIROVIS_BASE (element);
  GstFlowReturn result;
  GstBuffer *outbuf;
  gint width, height;
  cairo_surface_t *surf;
  cairo_t *cr;
  cairo_status_t stat;

  gboolean xlog = base->xscale;
  gboolean ylog = base->yscale;

  result =
      cairovis_base_buffer_surface_alloc (base, &outbuf, &surf, &width,
      &height);

  if (G_UNLIKELY (result != GST_FLOW_OK))
    goto done;

  cr = cairo_create (surf);
  stat = cairo_status (cr);
  if (G_UNLIKELY (stat != CAIRO_STATUS_SUCCESS))
  {
    GST_ERROR_OBJECT (element, "cairo_create: %s", cairo_status_to_string (stat));
    cairo_destroy (cr);
    cairo_surface_destroy (surf);
    result = GST_FLOW_ERROR;
    goto done;
  }

  /* Determine number of samples, data pointer */
  const double *data = (const double *) GST_BUFFER_DATA (inbuf);
  gulong nsamples = GST_BUFFER_SIZE (inbuf) / sizeof (double);
  gint nchannels = element->nchannels;
  gulong nsamples_per_channel = nsamples / nchannels;
  gulong i;
  gint channel;

  /* Determine x-axis limits */
  if (base->xautoscale) {
    if (xlog)
      base->xmin = 1;
    else
      base->xmin = 0;
    base->xmax = nsamples_per_channel;
  }

  /* Determine y-axis limits */
  if (base->yautoscale) {
    base->ymin = INFINITY;
    base->ymax = -INFINITY;
    for (i = 0; i < nsamples; i++) {
      if (data[i] < base->ymin)
        base->ymin = data[i];
      if (data[i] > base->ymax)
        base->ymax = data[i];
    }
  }

  /* Draw axes */
  cairovis_draw_axes (base, cr, width, height);

  /* Draw data */
  for (channel = 0; channel < nchannels; channel++) {
    gboolean was_finite = FALSE;

    for (i = 0; i < nsamples; i++) {
      double x = i, y = data[i * nchannels + channel];
      if (xlog)
        x = log10 (x);
      if (ylog)
        y = log10 (y);
      gboolean is_finite = isfinite (x) && isfinite (y);
      if (!was_finite && is_finite)
        cairo_move_to (cr, x, y);
      else if (is_finite)
        cairo_line_to (cr, x, y);
      was_finite = is_finite;
    }

    cairo_save (cr);

    /* Jump back to device space */
    cairo_identity_matrix (cr);

    /* Stroke the line series */
    cairo_stroke (cr);

    cairo_restore (cr);
  }

  /* Discard Cairo context, surface */
  cairo_destroy (cr);
  cairo_surface_destroy (surf);

  /* Copy buffer flags and timestamps */
  gst_buffer_copy_metadata (outbuf, inbuf,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS);

  result = gst_pad_push (base->srcpad, outbuf);

  /* Done! */
done:
  gst_buffer_unref (inbuf);
  gst_object_unref (element);
  return result;
}


/*
 * ============================================================================
 *
 *                                Type Support
 *
 * ============================================================================
 */


static GstElementClass *parent_class = NULL;


static void
finalize (GObject *object)
{
  CairoVisLineSeries *element = CAIROVIS_LINESERIES (object);

  gst_object_unref (element->sinkpad);
  element->sinkpad = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
base_init (gpointer class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (class);

  gst_element_class_set_details_simple (element_class,
      "Lineseries Visualizer",
      "Filter",
      "Render a vector input as a lineseries",
      "Leo Singer <leo.singer@ligo.org>");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink",
          GST_PAD_SINK,
          GST_PAD_ALWAYS,
          gst_caps_from_string ("audio/x-raw-float, "
              "channels   = (int) [1, MAX], " "width      = (int) 64")
      )
      );
}


static void
class_init (gpointer class, gpointer class_data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_ref (CAIROVIS_BASE_TYPE);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (finalize);
}


static void
instance_init (GTypeInstance *object, gpointer class)
{
  CairoVisLineSeries *element = CAIROVIS_LINESERIES (object);
  GstPadTemplate *tmpl =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (class), "sink");
  GstPad *pad = gst_pad_new_from_template (tmpl, "sink");

  gst_object_ref (pad);
  gst_element_add_pad (GST_ELEMENT (element), pad);
  gst_pad_use_fixed_caps (pad);
  gst_pad_set_setcaps_function (pad, GST_DEBUG_FUNCPTR (sink_setcaps));
  gst_pad_set_chain_function (pad, GST_DEBUG_FUNCPTR (chain));
  element->sinkpad = pad;
}


GType
cairovis_lineseries_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo info = {
      .class_size = sizeof (CairoVisLineSeriesClass),
      .class_init = class_init,
      .base_init = base_init,
      .instance_size = sizeof (CairoVisLineSeries),
      .instance_init = instance_init,
    };
    type =
        g_type_register_static (CAIROVIS_BASE_TYPE, "cairovis_lineseries",
        &info, 0);
  }

  return type;
}
