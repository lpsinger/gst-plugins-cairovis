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


#include <cairovis_histogram.h>

#include <math.h>


GType
cairovis_histogram_bins_get_type (void)
{
  static GType tp = 0;
  static const GEnumValue values[] = {
    {CAIROVIS_BINS_LINEAR, "linear bins", "linear"},
    {CAIROVIS_BINS_LOG, "logarithmic bins", "log"},
    {0, NULL, NULL},
  };

  if (G_UNLIKELY (tp == 0)) {
    tp = g_enum_register_static ("CairoVisHistogramBins", values);
  }
  return tp;
}


static void
increment_bin (CairoVisHistogram * element, double x, gchar value)
{
  switch (element->bins) {
    case CAIROVIS_BINS_LINEAR:
      x = element->nbins / (element->max - element->min) * (x - element->min);
      break;
    case CAIROVIS_BINS_LOG:
      x = element->nbins * (log2 (x) -
          log2 (element->min)) / (log2 (element->max) - log2 (element->min));
      break;
  }

  element->total += value;

  if (isfinite (x) && x >= 0 && x < element->nbins)
    element->bin_counts[(uint) floor (x)] += value;
}


static void
increment_bin_from_ptr (CairoVisHistogram * element, const double *ptr,
    const double *end, gchar value)
{
  for (; ptr < end; ptr++)
    increment_bin (element, *ptr, value);
}


static void
increment_bin_from_buf (CairoVisHistogram * element, GstBuffer * buf,
    gchar value)
{
  const double *ptr = (const double *) GST_BUFFER_DATA (buf);
  const double *end =
      (const double *) (GST_BUFFER_DATA (buf) + GST_BUFFER_SIZE (buf));

  increment_bin_from_ptr (element, ptr, end, value);
}


static GstFlowReturn
chain (GstPad * pad, GstBuffer * inbuf)
{
  CairoVisHistogram *element = CAIROVIS_HISTOGRAM (gst_pad_get_parent (pad));
  CairoVisBase *base = CAIROVIS_BASE (element);
  GstFlowReturn result;
  GstBuffer *outbuf;
  gint width, height;
  cairo_surface_t *surf;
  cairo_t *cr;
  double *bin_heights, *bin_edges;
  guint i;
  double last_x;

  if (!element->bin_counts) {
    guint available_bytes = gst_adapter_available (element->adapter);
    element->bin_counts = (guint *) g_new0 (guint, element->nbins);

    if (available_bytes > 0) {
      const guint8 *data = gst_adapter_peek (element->adapter, available_bytes);
      const double *ptr = (const double *) data;
      const double *end = (const double *) (data + available_bytes);
      increment_bin_from_ptr (element, ptr, end, 1);
    }
  }

  /* Add newly arrived data to histogram */
  increment_bin_from_buf (element, inbuf, 1);

  /* Store newly arrived data in adapter */
  gst_adapter_push (element->adapter, inbuf);

  /* Remove oldest data from histogram */
  guint available_bytes = gst_adapter_available (element->adapter);
  guint history_bytes = element->history * sizeof (double);
  if (available_bytes > history_bytes) {
    GstBuffer *buf =
        gst_adapter_take_buffer (element->adapter,
        available_bytes - history_bytes);
    increment_bin_from_buf (element, buf, -1);
    gst_buffer_unref (buf);
  }

  gboolean xlog = base->xscale;
  gboolean ylog = base->yscale;

  result =
      cairovis_base_buffer_surface_alloc (base, &outbuf, &surf, &width,
      &height);

  if (G_UNLIKELY (result != GST_FLOW_OK))
    goto done;

  cr = cairo_create (surf);

  bin_heights = g_malloc (sizeof (double) * element->nbins);
  bin_edges = g_malloc (sizeof (double) * (element->nbins + 1));

  for (i = 0; i < element->nbins + 1; i++) {
    switch (element->bins) {
      case CAIROVIS_BINS_LINEAR:
        bin_edges[i] =
            i * (element->max - element->min) / element->nbins + element->min;
        break;
      case CAIROVIS_BINS_LOG:
        bin_edges[i] =
            pow (element->max / element->min,
            (double) i / element->nbins) * element->min;
        break;
      default:
        g_assert_not_reached ();
    }
  }

  if (element->normed)
    for (i = 0; i < element->nbins; i++)
      bin_heights[i] =
          element->bin_counts[i] / (bin_edges[i + 1] -
          bin_edges[i]) / element->total;
  else
    for (i = 0; i < element->nbins; i++)
      bin_heights[i] = element->bin_counts[i];

  /* Determine x-axis limits */
  if (base->xautoscale) {
    base->xmin = element->min;
    base->xmax = element->max;
  }

  /* Determine y-axis limits */
  if (base->yautoscale) {
    base->ymin = INFINITY;
    base->ymax = -INFINITY;
    for (i = 0; i < element->nbins; i++) {
      double x = bin_heights[i];
      if (!ylog || isfinite (log10 (x))) {
        if (x < base->ymin)
          base->ymin = x;
        if (x > base->ymax)
          base->ymax = x;
      }
    }
  }

  /* Draw axes */
  cairovis_draw_axes (base, cr, width, height);

  last_x = bin_edges[0];
  if (xlog)
    last_x = log10 (last_x);

  /* Draw data */
  for (i = 0; i < element->nbins; i++) {
    double x = bin_edges[i + 1];
    double y = bin_heights[i];

    if (xlog)
      x = log10 (x);
    if (ylog)
      y = log10 (y);

    if (ylog) {
      double vorigin = (y < log10 (base->ymin) ? y : log10 (base->ymin));
      cairo_rectangle (cr, last_x, vorigin, x - last_x, y - vorigin);
    } else
      cairo_rectangle (cr, last_x, 0, x - last_x, y);

    last_x = x;
  }

  g_free (bin_heights);
  g_free (bin_edges);

  /* Jump back to device space */
  cairo_identity_matrix (cr);

  /* Stroke the line series */
  cairo_stroke (cr);

  /* Discard Cairo context, surface */
  cairo_destroy (cr);
  cairo_surface_destroy (surf);

  /* Copy buffer flags and timestamps */
  gst_buffer_copy_metadata (outbuf, inbuf,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS);

  result = gst_pad_push (base->srcpad, outbuf);

  /* Done! */
done:
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
finalize (GObject * object)
{
  CairoVisHistogram *element = CAIROVIS_HISTOGRAM (object);

  gst_object_unref (element->sinkpad);
  element->sinkpad = NULL;
  if (element->adapter) {
    g_object_unref (element->adapter);
    element->adapter = NULL;
  }
  g_free (element->bin_counts);
  element->bin_counts = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
base_init (gpointer class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (class);

  gst_element_class_set_details_simple (element_class,
      "Histogram Visualizer",
      "Filter",
      "Render a vector input as a histogram",
      "Leo Singer <leo.singer@ligo.org>");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink",
          GST_PAD_SINK,
          GST_PAD_ALWAYS,
          gst_caps_from_string ("audio/x-raw-float, "
              "channels   = (int) 1, " "width      = (int) 64")
      )
      );
}


enum property
{
  PROP_BINS = 1,
  PROP_BIN_MIN,
  PROP_BIN_MAX,
  PROP_NUM_BINS,
  PROP_HISTORY_SAMPLES,
  PROP_NORMED
};


static void
set_property (GObject * object, enum property id, const GValue * value,
    GParamSpec * pspec)
{
  CairoVisHistogram *element = CAIROVIS_HISTOGRAM (object);

  GST_OBJECT_LOCK (element);

  switch (id) {
    case PROP_BINS:
      element->bins = g_value_get_enum (value);
      break;
    case PROP_BIN_MIN:
      element->min = g_value_get_double (value);
      break;
    case PROP_BIN_MAX:
      element->max = g_value_get_double (value);
      break;
    case PROP_NUM_BINS:
      element->nbins = g_value_get_uint (value);
      break;
    case PROP_HISTORY_SAMPLES:
      element->history = g_value_get_uint (value);
      break;
    case PROP_NORMED:
      element->normed = g_value_get_boolean (value);
      break;
  }

  /* Setting any of the following properties forces rebinning. */
  if (id == PROP_BINS || id == PROP_BIN_MIN || id == PROP_BIN_MAX
      || id == PROP_NUM_BINS) {
    g_free (element->bin_counts);
    element->bin_counts = NULL;
    element->scale = element->nbins / (element->max - element->min);
  }

  GST_OBJECT_UNLOCK (element);
}


static void
get_property (GObject * object, enum property id, GValue * value,
    GParamSpec * pspec)
{
  CairoVisHistogram *element = CAIROVIS_HISTOGRAM (object);

  GST_OBJECT_LOCK (element);

  switch (id) {
    case PROP_BINS:
      g_value_set_enum (value, element->bins);
      break;
    case PROP_BIN_MIN:
      g_value_set_double (value, element->min);
      break;
    case PROP_BIN_MAX:
      g_value_set_double (value, element->max);
      break;
    case PROP_NUM_BINS:
      g_value_set_uint (value, element->nbins);
      break;
    case PROP_HISTORY_SAMPLES:
      g_value_set_uint (value, element->history);
      break;
    case PROP_NORMED:
      g_value_set_boolean (value, element->normed);
      break;
  }

  GST_OBJECT_UNLOCK (element);
}


static void
class_init (gpointer class, gpointer class_data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_ref (CAIROVIS_BASE_TYPE);

  gobject_class->get_property = GST_DEBUG_FUNCPTR (get_property);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (set_property);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (finalize);

  g_object_class_install_property (gobject_class,
      PROP_BINS,
      g_param_spec_enum ("bins",
          "bins style",
          "Style for bin spacing",
          CAIROVIS_HISTOGRAM_BINS_TYPE,
          CAIROVIS_BINS_LINEAR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_BIN_MIN,
      g_param_spec_double ("bin-min",
          "bin minimum value",
          "bin minimum value",
          -G_MAXDOUBLE, G_MAXDOUBLE, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_BIN_MAX,
      g_param_spec_double ("bin-max",
          "bin maximum value",
          "bin maximum value",
          -G_MAXDOUBLE, G_MAXDOUBLE, 1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_NUM_BINS,
      g_param_spec_uint ("num-bins",
          "number of bins",
          "number of bins to place between bin-min and bin-max",
          1, G_MAXUINT, 50,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_HISTORY_SAMPLES,
      g_param_spec_uint ("history-samples",
          "number of history samples",
          "maximum number of history samples to include in histogram",
          1, G_MAXUINT, 32768,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_NORMED,
      g_param_spec_boolean ("normed",
          "normalization of histogram",
          "set to TRUE to normalize bin heights",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
}


static void
instance_init (GTypeInstance * object, gpointer class)
{
  CairoVisHistogram *element = CAIROVIS_HISTOGRAM (object);
  GstPadTemplate *tmpl =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (class), "sink");
  GstPad *pad = gst_pad_new_from_template (tmpl, "sink");

  gst_object_ref (pad);
  gst_element_add_pad (GST_ELEMENT (element), pad);
  gst_pad_use_fixed_caps (pad);
  gst_pad_set_chain_function (pad, GST_DEBUG_FUNCPTR (chain));
  element->sinkpad = pad;

  element->bin_counts = NULL;
  element->adapter = gst_adapter_new ();
}


GType
cairovis_histogram_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo info = {
      .class_size = sizeof (CairoVisHistogramClass),
      .class_init = class_init,
      .base_init = base_init,
      .instance_size = sizeof (CairoVisHistogram),
      .instance_init = instance_init,
    };
    type =
        g_type_register_static (CAIROVIS_BASE_TYPE, "cairovis_histogram", &info,
        0);
  }

  return type;
}
