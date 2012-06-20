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


#include <cairovis_base.h>

#include <gst/video/video.h>

#include <math.h>


GType cairovis_scale_get_type (void)
{
  static GType tp = 0;
  static const GEnumValue values[] = {
    {CAIROVIS_SCALE_LINEAR, "linear scale", "linear"},
    {CAIROVIS_SCALE_LOG, "logarithmic scale", "log"},
    {0, NULL, NULL},
  };

  if (G_UNLIKELY (tp == 0)) {
    tp = g_enum_register_static ("CairoVisScale", values);
  }
  return tp;
}


static void fixate (GstPad *pad, GstCaps *caps)
{
  GstStructure *structure;

  /* Get the zeroth structure. */
  structure = gst_caps_get_structure (caps, 0);

  /* Throw away all but the zeroth structure. */
  gst_caps_truncate(caps);

  /* Fill in some default fields that we will need. */
  gst_structure_fixate_field_nearest_int (structure, "width", 640);
  gst_structure_fixate_field_nearest_int (structure, "height", 480);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", 30, 1);
}


static void draw_major_tick (cairo_t *cr, double x)
{
  cairo_move_to (cr, x, -8);
  cairo_line_to (cr, x, 8);
}


static void draw_minor_tick (cairo_t *cr, double x)
{
  cairo_move_to (cr, x, -4);
  cairo_line_to (cr, x, 4);
}


void cairovis_draw_axis (cairo_t * cr, const struct cairovis_axis_spec * axis)
{
  cairo_text_extents_t extents;
  int ntick, nmintick, nmaxtick, nsubtick;
  double devicemax = axis->device_max;
  double datamin = axis->data_min;
  double datamax = axis->data_max;
  double scalefactor = devicemax / (datamax - datamin);
  double x;
  gchar *text;

  cairo_save (cr);

  switch (axis->which_side) {
    case CAIROVIS_EAST:
      cairo_rotate (cr, M_PI_2);
      cairo_scale (cr, -1, -1);
      break;
    case CAIROVIS_WEST:
      cairo_rotate (cr, M_PI_2);
      cairo_scale (cr, -1, 1);
      break;
    case CAIROVIS_NORTH:
      cairo_scale (cr, 1, -1);
      break;
    case CAIROVIS_SOUTH:
      break;
  }

  /* Draws a horizontal axis with tick marks, labels, and automagically placed
   * tick marks.  datamin and datamax are the dataspace limits of the plot.
   * If is_log is set to TRUE, then it is expected that datamin and datamax
   * are actually the base-10 logarithms of the dataspace limits.
   *
   * The tick placement algorithm for logarthmic scales draws all major ticks
   * that are at integer powers of 10.  FIXME: this algorithm could be made a
   * lot smarter.
   *
   * The tick placement algorithm for linear scales can pick ticks that are
   * spaced by powers of ten, or doubled or halved powers of ten.
   */
  if (axis->scale == CAIROVIS_SCALE_LOG)
  {
    nmintick = floor (datamin);
    nmaxtick = ceil (datamax);

    /* Place minor ticks. */
    for (ntick = nmintick; ntick <= nmaxtick; ntick++)
    {
      for (nsubtick = 2; nsubtick < 10; nsubtick++)
      {
        x = (log10 (nsubtick) + ntick - datamin) * scalefactor;
        if (x > 0 && x < devicemax)
          draw_minor_tick (cr, x);
      }
    }

    nmintick = ceil (datamin);
    nmaxtick = floor (datamax);

    /* Place major ticks. */
    for (ntick = nmintick; ntick <= nmaxtick; ntick++)
    {
      double x = (ntick - datamin) * scalefactor;
      draw_major_tick (cr, x);
    }

    /* Stroke tick marks. */
    cairo_stroke (cr);

    /* Place major tick labels. */
    for (ntick = nmintick; ntick <= nmaxtick; ntick++)
    {
      text = g_strdup_printf ("10%d", ntick);
      cairo_text_extents (cr, text, &extents);
      g_free (text);
      cairo_move_to (cr, (ntick - datamin) * scalefactor, 16);
      switch (axis->which_side) {
        case CAIROVIS_NORTH:
          cairo_rel_move_to(cr, -0.5 * extents.width, 0.);
          break;
        case CAIROVIS_SOUTH:
          cairo_rel_move_to(cr, -0.5 * extents.width, extents.height);
          break;
        case CAIROVIS_EAST:
          cairo_rel_move_to(cr, -0.5 * extents.height, 0.);
          break;
        case CAIROVIS_WEST:
          cairo_rel_move_to(cr, -0.5 * extents.height, extents.width);
          break;
      }
      cairo_save (cr);
      cairo_identity_matrix (cr);
      cairo_show_text (cr, "10");
      cairo_rel_move_to (cr, 0, -0.5 * extents.height);
      text = g_strdup_printf ("%d", ntick);
      cairo_show_text (cr, text);
      g_free (text);
      cairo_restore (cr);
    }

  } else /* !(is_log) */ {
    /* Set this number to a tick interval (in pixels) that looks pleasing to the eye. */
    double desired_pixel_interval = 100;

    /* Intervals between ticks in data space */
    double interval = desired_pixel_interval / scalefactor;

    /* Propose tick intervals that is rounded to the nearest power of 10,
      * and also intervals that are double and half that. */
    double rounded_interval = pow (10.0, rint (log10 (interval)));
    double doubled_interval = 2.0 * rounded_interval;
    double halved_interval = 0.5 * rounded_interval;

    double rounded_diff = fabs (rounded_interval - interval);
    double doubled_diff = fabs (doubled_interval - interval);
    double halved_diff = fabs (rounded_interval - interval);

    /* Pick the interval that is closest to the desired interval. */
    if (rounded_diff < doubled_diff)
    {
      if (rounded_diff < halved_diff)
        interval = rounded_interval;
      else
        interval = halved_interval;
    } else if (doubled_diff < halved_diff)
      interval = doubled_interval;
    else
      interval = halved_diff;

    datamin /= interval;
    datamax /= interval;
    scalefactor *= interval;

    nmintick = floor (datamin);
    nmaxtick = ceil (datamax);

    /* Place minor ticks. */
    for (ntick = nmintick; ntick <= nmaxtick; ntick++)
    {
      for (nsubtick = 1; nsubtick < 10; nsubtick++)
      {
        x = (0.1 * nsubtick + ntick - datamin) * scalefactor;
        if (x > 0 && x < devicemax)
        {
          draw_minor_tick (cr, x);
        }
      }
    }

    nmintick = ceil (datamin);
    nmaxtick = floor (datamax);

    /* Place major ticks. */
    for (ntick = nmintick; ntick <= nmaxtick; ntick++)
    {
      x = (ntick - datamin) * scalefactor;
      draw_major_tick (cr, x);
    }

    /* Stroke tick marks. */
    cairo_stroke(cr);

    /* Place major tick labels. */
    for (ntick = nmintick; ntick <= nmaxtick; ntick++)
    {
      text = g_strdup_printf ("%g", ntick * interval);
      cairo_text_extents (cr, text, &extents);
      cairo_move_to(cr, (ntick - datamin) * scalefactor, 16);
      switch (axis->which_side) {
        case CAIROVIS_NORTH:
          cairo_rel_move_to (cr, -0.5 * extents.width, 0.);
          break;
        case CAIROVIS_SOUTH:
          cairo_rel_move_to (cr, -0.5 * extents.width, extents.height);
          break;
        case CAIROVIS_EAST:
          cairo_rel_move_to (cr, -0.5 * extents.height, 0.);
          break;
        case CAIROVIS_WEST:
          cairo_rel_move_to (cr, -0.5 * extents.height, extents.width);
          break;
      }
      cairo_save (cr);
      cairo_identity_matrix (cr);
      cairo_show_text (cr, text);
      g_free (text);
      cairo_restore (cr);
    }
  }

  cairo_restore (cr);
}


gboolean cairovis_base_negotiate_srcpad (CairoVisBase *element)
{
  GstPad *srcpad = element->srcpad;
  
  /* Negotiate caps if necessary */
  if (G_UNLIKELY (GST_PAD_CAPS (srcpad) == NULL))
  {
    GstCaps *caps = gst_pad_get_allowed_caps (srcpad);
    if (gst_caps_is_empty (caps))
    {
      gst_caps_unref (caps);
      GST_WARNING_OBJECT (element, "intersected caps is empty");
      return FALSE;
    }
    caps = gst_caps_make_writable (caps);
    gst_pad_fixate_caps (srcpad, caps);
    gst_pad_set_caps (srcpad, caps);
    gst_caps_unref (caps);
  }

  return TRUE;
}


GstFlowReturn cairovis_base_buffer_surface_alloc (CairoVisBase *element, GstBuffer **buf, cairo_surface_t **surf, gint *width, gint *height)
{
  GstFlowReturn result;
  GstPad *srcpad = element->srcpad;
  cairo_status_t stat;
  GstBuffer *out_buf;
  cairo_surface_t *out_surf;
  gint out_width, out_height;

  if (G_UNLIKELY (!cairovis_base_negotiate_srcpad (element)))
    return GST_FLOW_NOT_NEGOTIATED;

  /* Determine width and height of destination */
  if (G_UNLIKELY (!gst_video_get_size (srcpad, &out_width, &out_height)))
    return GST_FLOW_ERROR;

  /* Determine width and height of destination */
  result = gst_pad_alloc_buffer_and_set_caps (srcpad,
    GST_BUFFER_OFFSET_NONE, 4 * out_width * out_height,
    GST_PAD_CAPS (srcpad), &out_buf);

  if (G_UNLIKELY (result != GST_FLOW_OK))
  {
    GST_WARNING_OBJECT (element, "Failed to alloc buffer: %s",
      gst_flow_get_name (result));
    return result;
  }

  /* Attempt to create image surface */
  out_surf = cairo_image_surface_create_for_data(
    GST_BUFFER_DATA (out_buf),
    CAIRO_FORMAT_RGB24,
    out_width, out_height, out_width * 4);

  stat = cairo_surface_status (out_surf);
  if (G_UNLIKELY (stat != CAIRO_STATUS_SUCCESS))
  {
    GST_WARNING_OBJECT (element, "Failed to create image surface: %s",
      cairo_status_to_string (stat));
    gst_buffer_unref (out_buf);
    return GST_FLOW_ERROR;
  }

  *buf = out_buf;
  *surf = out_surf;
  *width = out_width;
  *height = out_height;

  return result;
}


void cairovis_draw_axes (CairoVisBase *element, cairo_t *cr, gint width, gint height)
{
  cairo_font_extents_t font_extents;
  cairo_text_extents_t text_extents;
  double padding, padded_width, padded_height;

  gboolean xlog = (element->xscale == CAIROVIS_SCALE_LOG);
  gboolean ylog = (element->yscale == CAIROVIS_SCALE_LOG);

  gdouble xmin = element->xmin;
  gdouble xmax = element->xmax;
  gdouble ymin = element->ymin;
  gdouble ymax = element->ymax;

  if (xlog) {
    xmin = log10 (xmin);
    xmax = log10 (xmax);
  }

  if (ylog) {
    ymin = log10 (ymin);
    ymax = log10 (ymax);
  }

  /* Paint background black */
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);

  /* Draw everything else white */
  cairo_set_source_rgb (cr, 1, 1, 1);

  /* Determine font metrics, which governs the layout of the plot */
  cairo_set_font_size (cr, 12.0);
  cairo_font_extents (cr, &font_extents);
  padding = 5.0 * font_extents.ascent;
  padded_width = width - 2 * padding;
  padded_height = height - 2 * padding;

  /* Draw centered title */
  if (element->title)
  {
    cairo_text_extents (cr, element->title, &text_extents);
    cairo_move_to (cr, 0.5 * (width - text_extents.width), 2.0 * font_extents.ascent);
    cairo_show_text (cr, element->title);
  }

  /* Draw centered xlabel */
  if (element->xlabel)
  {
    cairo_text_extents (cr, element->xlabel, &text_extents);
    cairo_move_to (cr, 0.5 * (width - text_extents.width), height - 1.0 * font_extents.ascent);
    cairo_show_text (cr, element->xlabel);
  }

  /* Draw centered ylabel */
  if (element->ylabel)
  {
    cairo_text_extents (cr, element->ylabel, &text_extents);
    cairo_move_to (cr, 1.0 * font_extents.ascent, 0.5 * (height + text_extents.width));
    cairo_save (cr);
    cairo_rotate (cr, -M_PI_2);
    cairo_show_text (cr, element->ylabel);
    cairo_restore (cr);
  }

  /* Flip & translate transformation so that bottom left corner is origin */
  cairo_translate (cr, padding, height - padding);

  /* Render x-axis tick marks */
  struct cairovis_axis_spec xspec = {element->xscale, CAIROVIS_SOUTH, padded_width, xmin, xmax};
  struct cairovis_axis_spec yspec = {element->yscale, CAIROVIS_WEST, padded_height, ymin, ymax};
  cairovis_draw_axis(cr, &xspec);
  cairovis_draw_axis(cr, &yspec);

  /* Render y-axis tick marks */

  /* Draw axes frame, and clip all further drawing inside it */
  cairo_scale (cr, 1.0, -1.0);
  cairo_rectangle (cr, 0, 0, padded_width, padded_height);
  cairo_stroke_preserve (cr);
  cairo_clip (cr);

  /* Build transformation for data to user space */
  cairo_scale (cr, padded_width / (xmax - xmin), padded_height / (ymax - ymin));
  cairo_translate (cr, -xmin, -ymin);
}


/*
 * ============================================================================
 *
 *                                Type Support
 *
 * ============================================================================
 */


static GstElementClass *parent_class = NULL;


enum property
{
  PROP_TITLE = 1,
  PROP_XLABEL,
  PROP_YLABEL,
  PROP_XSCALE,
  PROP_YSCALE,
  PROP_XAUTOSCALE,
  PROP_YAUTOSCALE,
  PROP_XMIN,
  PROP_XMAX,
  PROP_YMIN,
  PROP_YMAX,
};


static void set_property (GObject * object, enum property id, const GValue * value, GParamSpec * pspec)
{
  CairoVisBase *element = CAIROVIS_BASE (object);

  GST_OBJECT_LOCK(element);

  switch (id) {
    case PROP_TITLE:
      g_free (element->title);
      element->title = g_value_dup_string (value);
      break;
    case PROP_XLABEL:
      g_free (element->xlabel);
      element->xlabel = g_value_dup_string (value);
      break;
    case PROP_YLABEL:
      g_free (element->ylabel);
      element->ylabel = g_value_dup_string (value);
      break;
    case PROP_XSCALE:
      element->xscale = g_value_get_enum (value);
      break;
    case PROP_YSCALE:
      element->yscale = g_value_get_enum (value);
      break;
    case PROP_XAUTOSCALE:
      element->xautoscale = g_value_get_boolean (value);
      break;
    case PROP_YAUTOSCALE:
      element->yautoscale = g_value_get_boolean (value);
      break;
    case PROP_XMIN:
      element->xmin = g_value_get_double (value);
      break;
    case PROP_XMAX:
      element->xmax = g_value_get_double (value);
      break;
    case PROP_YMIN:
      element->ymin = g_value_get_double (value);
      break;
    case PROP_YMAX:
      element->ymax = g_value_get_double (value);
      break;
  }

  GST_OBJECT_UNLOCK (element);
}


static void get_property (GObject * object, enum property id, GValue * value, GParamSpec * pspec)
{
  CairoVisBase *element = CAIROVIS_BASE (object);

  GST_OBJECT_LOCK (element);

  switch (id) {
    case PROP_TITLE:
      g_value_set_string (value, element->title);
      break;
    case PROP_XLABEL:
      g_value_set_string (value, element->xlabel);
      break;
    case PROP_YLABEL:
      g_value_set_string (value, element->ylabel);
      break;
    case PROP_XSCALE:
      g_value_set_enum (value, element->xscale);
      break;
    case PROP_YSCALE:
      g_value_set_enum (value, element->yscale);
      break;
    case PROP_XAUTOSCALE:
      g_value_set_boolean (value, element->xautoscale);
      break;
    case PROP_YAUTOSCALE:
      g_value_set_boolean (value, element->yautoscale);
      break;
    case PROP_XMIN:
      g_value_set_double (value, element->xmin);
      break;
    case PROP_XMAX:
      g_value_set_double (value, element->xmax);
      break;
    case PROP_YMIN:
      g_value_set_double (value, element->ymin);
      break;
    case PROP_YMAX:
      g_value_set_double (value, element->ymax);
      break;
  }

  GST_OBJECT_UNLOCK (element);
}


static void finalize (GObject *object)
{
  CairoVisBase *element = CAIROVIS_BASE (object);

  g_free (element->title);
  element->title = NULL;
  g_free (element->xlabel);
  element->xlabel = NULL;
  g_free (element->ylabel);
  element->ylabel = NULL;

  gst_object_unref (element->srcpad);
  element->srcpad = NULL;
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void base_init (gpointer class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (class);

  gst_element_class_set_details_simple (
    element_class,
    "Cairo Visualization Base Class",
    "Filter",
    "Base class for elements that use Cairo to render plots",
    "Leo Singer <leo.singer@ligo.org>"
  );

  gst_element_class_add_pad_template (
    element_class,
    gst_pad_template_new (
      "src",
      GST_PAD_SRC,
      GST_PAD_ALWAYS,
      gst_caps_from_string (GST_VIDEO_CAPS_BGRx)
    )
  );
}


static void class_init (gpointer class, gpointer class_data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  gobject_class->get_property = GST_DEBUG_FUNCPTR (get_property);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (set_property);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (finalize);

  g_object_class_install_property (gobject_class,
      PROP_TITLE,
      g_param_spec_string ("title",
          "Title",
          "Title of plot",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (gobject_class,
      PROP_XLABEL,
      g_param_spec_string ("x-label",
          "x-Label",
          "Label for x-axis",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (gobject_class,
      PROP_YLABEL,
      g_param_spec_string ("y-label",
          "y-Label",
          "Label for y-axis",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (gobject_class,
      PROP_XSCALE,
      g_param_spec_enum ("x-scale",
          "x-Scale",
          "Linear or logarithmic scale",
          CAIROVIS_SCALE_TYPE,
          CAIROVIS_SCALE_LINEAR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_YSCALE,
      g_param_spec_enum ("y-scale",
          "y-Scale",
          "Linear or logarithmic scale",
          CAIROVIS_SCALE_TYPE,
          CAIROVIS_SCALE_LINEAR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_XAUTOSCALE,
      g_param_spec_boolean ("x-autoscale",
          "x-Autoscale",
          "Set to true to autoscale the x-axis",
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_YAUTOSCALE,
      g_param_spec_boolean ("y-autoscale",
          "y-Autoscale",
          "Set to true to autoscale the y-axis",
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_XMIN,
      g_param_spec_double ("x-min",
          "x-Minimum",
          "Minimum limit of y-axis (has no effect if x-autoscale is set to true)",
          -G_MAXDOUBLE, G_MAXDOUBLE, -2.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_XMAX,
      g_param_spec_double ("x-max",
          "x-Maximum",
          "Maximum limit of x-axis (has no effect if x-autoscale is set to true)",
          -G_MAXDOUBLE, G_MAXDOUBLE, 2.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_YMIN,
      g_param_spec_double ("y-min",
          "y-Minimum",
          "Minimum limit of y-axis (has no effect if y-autoscale is set to true)",
          -G_MAXDOUBLE, G_MAXDOUBLE, -2.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_YMAX,
      g_param_spec_double ("y-max",
          "y-Maximum",
          "Maximum limit of y-axis (has no effect if y-autoscale is set to true)",
          -G_MAXDOUBLE, G_MAXDOUBLE, 2.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
}


static void instance_init (GTypeInstance *object, gpointer class)
{
  CairoVisBase *element = CAIROVIS_BASE (object);
  GstPadTemplate *tmpl = gst_element_class_get_pad_template (GST_ELEMENT_CLASS (class), "src");
  GstPad *pad = gst_pad_new_from_template (tmpl, "src");

  gst_object_ref (pad);
  gst_element_add_pad (GST_ELEMENT (element), pad);
  gst_pad_use_fixed_caps (pad);
  gst_pad_set_fixatecaps_function (pad, GST_DEBUG_FUNCPTR (fixate));
  element->srcpad = pad;

  element->title = NULL;
  element->xlabel = NULL;
  element->ylabel = NULL;
}


GType cairovis_base_get_type(void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo info = {
      .class_size = sizeof (CairoVisBaseClass),
      .class_init = class_init,
      .base_init = base_init,
      .instance_size = sizeof (CairoVisBase),
      .instance_init = instance_init,
    };
    type = g_type_register_static (GST_TYPE_ELEMENT, "cairovis_base", &info, 0);
  }

  return type;
}
