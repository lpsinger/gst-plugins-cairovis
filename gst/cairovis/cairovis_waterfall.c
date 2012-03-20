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


#include <cairovis_waterfall.h>

#include <gst/video/video.h>

#include <math.h>


#define GST_CAT_DEFAULT cairovis_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);


static gboolean
sink_setcaps (GstPad * pad, GstCaps * caps)
{
  CairoVisWaterfall *element = CAIROVIS_WATERFALL (gst_pad_get_parent (pad));
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  gboolean success =
      gst_structure_get_int (structure, "channels", &element->nchannels);
  success &= gst_structure_get_int (structure, "rate", &element->rate);

  gst_adapter_clear (element->adapter);
  gst_object_unref (element);

  return success;
}


static GstFlowReturn
sink_chain (GstPad * pad, GstBuffer * inbuf)
{
  CairoVisWaterfall *element = CAIROVIS_WATERFALL (gst_pad_get_parent (pad));
  CairoVisBase *base = CAIROVIS_BASE (element);
  GstFlowReturn result = GST_FLOW_ERROR;
  GstBuffer *outbuf;
  gint width, height;
  gint axes_width, axes_height;
  cairo_surface_t *surf;
  cairo_t *cr;
  double *data;
  guint i, j;
  gboolean zlog = (element->zscale == CAIROVIS_SCALE_LOG);
  gdouble zmin, zmax;

  if (base->xscale || base->yscale) {
    gst_buffer_unref (inbuf);
    GST_ELEMENT_ERROR (element, CORE, TOO_LAZY,
        ("logarithmic scale not yet implemented"), (NULL));
    goto done;
  }

  if (G_UNLIKELY (!(GST_BUFFER_TIMESTAMP_IS_VALID (inbuf)
              && GST_BUFFER_DURATION_IS_VALID (inbuf)
              && GST_BUFFER_OFFSET_IS_VALID (inbuf)
              && GST_BUFFER_OFFSET_END_IS_VALID (inbuf)))) {
    gst_buffer_unref (inbuf);
    GST_ERROR_OBJECT (element, "Buffer has invalid timestamp and/or offset");
    goto done;
  }

  if (G_UNLIKELY (!(GST_CLOCK_TIME_IS_VALID (element->t0)))) {
    element->t0 = GST_BUFFER_TIMESTAMP (inbuf);
    element->offset0 = GST_BUFFER_OFFSET (inbuf);
    element->last_offset_end = 0;
    element->frame_number = 0;
  }

  gst_adapter_push (element->adapter, inbuf);

  if (G_UNLIKELY (!cairovis_base_negotiate_srcpad (base))) {
    result = GST_FLOW_NOT_NEGOTIATED;
    goto done;
  }

  guint available_bytes = gst_adapter_available (element->adapter);
  guint stride_bytes = sizeof (double) * element->nchannels;
  guint available_samples = available_bytes / stride_bytes;
  gint fpsn, fpsd;
  gst_video_parse_caps_framerate (GST_PAD_CAPS (base->srcpad), &fpsn, &fpsd);
  guint64 history_samples =
      gst_util_uint64_scale_int_round (element->history, element->rate,
      GST_SECOND);

  /* FIXME: This doesn't really have to be an infinite loop. */
  while (TRUE) {
    GST_INFO_OBJECT (element,
        "checking to see if we have enough data to draw frame %"
        G_GUINT64_FORMAT, element->frame_number);
    GST_INFO_OBJECT (element, "rate=%d, framerate=%d/%d", element->rate, fpsn,
        fpsd);

    /* FIXME: check my timestamp math here; it's probably not perfect */
    guint64 desired_offset =
        gst_util_uint64_scale_int_round (element->frame_number,
        fpsd * element->rate, fpsn);
    if (history_samples < desired_offset)
      desired_offset -= history_samples;
    else
      desired_offset = 0;
    guint64 desired_offset_end =
        gst_util_uint64_scale_int_round (element->frame_number,
        fpsd * element->rate, fpsn);
    guint64 desired_samples = desired_offset_end - desired_offset;
    guint64 desired_bytes = desired_samples * stride_bytes;

    GST_INFO_OBJECT (element,
        "we want offsets %" G_GUINT64_FORMAT " through %" G_GUINT64_FORMAT,
        desired_offset, desired_offset_end);

    if (element->last_offset_end < desired_offset) {
      guint flush_samples = desired_offset - element->last_offset_end;
      if (flush_samples > available_samples)
        flush_samples = available_samples;
      guint flush_bytes = flush_samples * stride_bytes;
      gst_adapter_flush (element->adapter, flush_bytes);
      available_samples -= flush_samples;
      available_bytes -= flush_bytes;
      element->last_offset_end += flush_samples;
    } else if (element->last_offset_end > desired_offset) {
      GST_INFO_OBJECT (element,
          "sink pad has not yet advanced far enough to draw frame %"
          G_GUINT64_FORMAT, element->frame_number);
      result = GST_FLOW_OK;
      goto done;
    }

    if (available_samples < desired_samples) {
      GST_INFO_OBJECT (element,
          "not enough data to draw frame %" G_GUINT64_FORMAT,
          element->frame_number);
      result = GST_FLOW_OK;
      goto done;
    }

    GST_INFO_OBJECT (element, "preparing to draw frame %" G_GUINT64_FORMAT,
        element->frame_number);

    result =
        cairovis_base_buffer_surface_alloc (base, &outbuf, &surf, &width,
        &height);

    if (G_UNLIKELY (result != GST_FLOW_OK))
      goto done;

    /* Make space for colorbar if necessary */
    if (element->colorbar)
    {
      axes_width = width - 72;
      axes_height = height;
    } else {
      axes_width = width;
      axes_height = height;
    }

    cr = cairo_create (surf);

    /* Copy buffer flags and timestamps */
    gst_buffer_copy_metadata (outbuf, inbuf, GST_BUFFER_COPY_FLAGS);
    GST_BUFFER_OFFSET (outbuf) = element->frame_number;
    GST_BUFFER_OFFSET_END (outbuf) = element->frame_number + 1;
    GST_BUFFER_TIMESTAMP (outbuf) =
        gst_util_uint64_scale_round (desired_offset, GST_SECOND,
        element->rate) + element->t0;
    GST_BUFFER_DURATION (outbuf) =
        gst_util_uint64_scale_round (desired_offset_end, GST_SECOND,
        element->rate) + element->t0 - GST_BUFFER_TIMESTAMP (outbuf);

    guint npixels = desired_samples * element->nchannels;
    if (desired_samples > 0) {
      double *orig_data =
          (double *) gst_adapter_peek (element->adapter, desired_bytes);
      if (zlog) {
        data = g_malloc (desired_bytes);
        for (i = 0; i < npixels; i++)
          data[i] = log10 (orig_data[i]);
      } else {
        data = orig_data;
      }
    } else
      data = NULL;

    /* Determine x-axis limits */
    if (base->xautoscale) {
      base->xmin = (-1. / GST_SECOND) * element->history;
      base->xmax = 0;
    }

    /* Determine y-axis limits */
    if (base->yautoscale) {
      base->ymin = 0;
      base->ymax = element->nchannels;
    }

    /* Determine z-axis limits */
    if (element->zautoscale && data) {
      zmin = INFINITY;
      zmax = -INFINITY;
      for (i = 0; i < npixels; i++) {
        if (data[i] < zmin)
          zmin = data[i];
        if (data[i] > zmax)
          zmax = data[i];
      }
    } else {
      zmin = element->zmin;
      zmax = element->zmax;
      if (zlog) {
        zmin = log10 (zmin);
        zmax = log10 (zmax);
      }
    }

    cairovis_draw_axes (base, cr, axes_width, axes_height);

    cairo_save (cr);
    cairo_identity_matrix (cr);
    cairo_reset_clip (cr);
    /* Draw timestamp. */
    gchar *nanos_str = NULL;
    gchar *secs_str = NULL;
    GstClockTime ts =
        GST_BUFFER_TIMESTAMP (outbuf) + GST_BUFFER_DURATION (outbuf);
    for (i = 0; i < 3; ts /= 1000, i++) {
      gchar frag[4];
      g_snprintf (frag, sizeof (frag), "%03hu", (unsigned short) (ts % 1000));
      gchar *new_str = g_strjoin (" ", frag, nanos_str, NULL);
      g_free (nanos_str);
      nanos_str = new_str;
    }
    for (; ts >= 1000; ts /= 1000) {
      gchar frag[4];
      g_snprintf (frag, sizeof (frag), "%03hu", (unsigned short) (ts % 1000));
      gchar *new_str = g_strjoin (" ", frag, secs_str, NULL);
      g_free (secs_str);
      secs_str = new_str;
    }
    if (ts > 0) {
      gchar frag[4];
      g_snprintf (frag, sizeof (frag), "%hu", (unsigned short) (ts % 1000));
      gchar *new_str = g_strjoin (" ", frag, secs_str, NULL);
      g_free (secs_str);
      secs_str = new_str;
    }
    gchar *ts_str = g_strdup_printf ("+ %s.%s", secs_str, nanos_str);
    g_free (nanos_str);
    g_free (secs_str);
    cairo_text_extents_t extents;
    cairo_text_extents (cr, ts_str, &extents);
    cairo_move_to (cr, axes_width - extents.width - 36, axes_height - 18);
    cairo_show_text (cr, ts_str);
    g_free (ts_str);
    cairo_restore (cr);

    /* Draw pixels */
    if (data) {
      GST_INFO_OBJECT (element, "painting pixels for frame %" G_GUINT64_FORMAT,
          element->frame_number);
      guint32 *pixdata = g_malloc (npixels * sizeof (guint32));
      double invzspan = 1.0 / (zmax - zmin);
      for (i = 0; i < npixels; i++) {
        double x = data[i];
        if (x < zmin)
          x = 0;
        else if (x > zmax)
          x = 1;
        else
          x = (x - zmin) * invzspan;
        pixdata[i] = colormap_map (element->map, x);
      }
      cairo_surface_t *pixsurf =
          cairo_image_surface_create_for_data ((unsigned char *) pixdata,
          CAIRO_FORMAT_RGB24, element->nchannels, desired_samples,
          element->nchannels * 4);
      cairo_translate (cr, -1e-9 * GST_BUFFER_DURATION (outbuf), 0);
      cairo_rotate (cr, M_PI_2);
      cairo_scale (cr, 1.0, -1.0 / element->rate);
      cairo_set_source_surface (cr, pixsurf, 0, 0);
      cairo_paint (cr);
      cairo_surface_destroy (pixsurf);
      g_free (pixdata);

      /* Draw colorbar if necessary */
      if (element->colorbar)
      {
        GST_INFO_OBJECT (element, "painting colorbar");

        /* Determine device space coordinates of axes corners */
        double axes_left = -1., axes_right = 1., axes_bottom = -1., axes_top = 1.;
        cairo_user_to_device (cr, &axes_left, &axes_bottom);
        cairo_user_to_device (cr, &axes_right, &axes_top);

        int colorbar_width = 16, colorbar_height = lrint(axes_bottom - axes_top);
        pixdata = g_malloc (colorbar_width * colorbar_height * sizeof (guint32));
        for (i = 0; i < colorbar_height; i++) {
          double x = colormap_map (element->map, (double) i / colorbar_height);
          for (j = 0; j < colorbar_width; j ++)
            pixdata[i * colorbar_width + j] = x;
        }
        pixsurf = cairo_image_surface_create_for_data ((unsigned char *) pixdata, CAIRO_FORMAT_RGB24, colorbar_width, colorbar_height, cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, colorbar_width));
        struct cairovis_axis_spec zspec = {element->zscale, CAIROVIS_WEST, colorbar_height, zmin, zmax};
        cairo_identity_matrix (cr);
        cairo_reset_clip (cr);
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_translate (cr, axes_width, 0.5 * (height + axes_bottom - axes_top));
        cairovis_draw_axis (cr, &zspec);
        cairo_scale (cr, 1., -1.);
        cairo_rectangle (cr, 0., 0., colorbar_width, colorbar_height);
        cairo_clip (cr);
        cairo_set_source_surface (cr, pixsurf, 0, 0);
        cairo_paint (cr);
        cairo_surface_destroy (pixsurf);
        cairo_reset_clip (cr);
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_rectangle (cr, 0., 0., colorbar_width, colorbar_height);
        cairo_stroke (cr);
        g_free (pixdata);

        /* Draw colorbar label if necessary */
        if (element->zlabel)
        {
          cairo_font_extents_t font_extents;
          cairo_text_extents_t text_extents;
          cairo_set_font_size (cr, 12.0);
          cairo_font_extents (cr, &font_extents);
          cairo_text_extents (cr, element->zlabel, &text_extents);
          cairo_scale (cr, 1., -1.);
          cairo_move_to (cr, 1.5 * font_extents.ascent + colorbar_width, -0.5 * (colorbar_height - text_extents.width));
          cairo_rotate (cr, -M_PI_2);
          cairo_show_text (cr, element->zlabel);
        }
      }
    }

    if (zlog)
      g_free (data);

    /* Discard Cairo context, surface */
    cairo_destroy (cr);
    cairo_surface_destroy (surf);

    result = gst_pad_push (base->srcpad, outbuf);
    if (result != GST_FLOW_OK)
      goto done;

    element->frame_number++;
  }

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


enum property
{
  PROP_ZLABEL = 1,
  PROP_ZSCALE,
  PROP_ZAUTOSCALE,
  PROP_ZMIN,
  PROP_ZMAX,
  PROP_HISTORY,
  PROP_COLORMAP,
  PROP_COLORBAR,
};


static void
set_property (GObject * object, enum property id, const GValue * value,
    GParamSpec * pspec)
{
  CairoVisWaterfall *element = CAIROVIS_WATERFALL (object);

  GST_OBJECT_LOCK (element);

  switch (id) {
    case PROP_ZLABEL:
      g_free (element->zlabel);
      element->zlabel = g_value_dup_string (value);
      break;
    case PROP_ZSCALE:
      element->zscale = g_value_get_enum (value);
      break;
    case PROP_ZAUTOSCALE:
      element->zautoscale = g_value_get_boolean (value);
      break;
    case PROP_ZMIN:
      element->zmin = g_value_get_double (value);
      break;
    case PROP_ZMAX:
      element->zmax = g_value_get_double (value);
      break;
    case PROP_HISTORY:
      element->history = g_value_get_uint64 (value);
      break;
    case PROP_COLORMAP:{
      enum cairovis_colormap_name new_map_name = g_value_get_enum (value);
      colormap *new_map = colormap_create_by_name (new_map_name);
      if (new_map) {
        colormap_destroy (element->map);
        element->map_name = new_map_name;
        element->map = new_map;
      } else {
        GST_ERROR_OBJECT (element, "no such colormap");
      }
    }
      break;
    case PROP_COLORBAR:
      element->colorbar = g_value_get_boolean (value);
  }

  GST_OBJECT_UNLOCK (element);
}


static void
get_property (GObject * object, enum property id, GValue * value,
    GParamSpec * pspec)
{
  CairoVisWaterfall *element = CAIROVIS_WATERFALL (object);

  GST_OBJECT_LOCK (element);

  switch (id) {
    case PROP_ZLABEL:
      g_value_set_string (value, element->zlabel);
      break;
    case PROP_ZSCALE:
      g_value_set_enum (value, element->zscale);
      break;
    case PROP_ZAUTOSCALE:
      g_value_set_boolean (value, element->zautoscale);
      break;
    case PROP_ZMIN:
      g_value_set_double (value, element->zmin);
      break;
    case PROP_ZMAX:
      g_value_set_double (value, element->zmax);
      break;
    case PROP_HISTORY:
      g_value_set_uint64 (value, element->history);
      break;
    case PROP_COLORMAP:
      g_value_set_enum (value, element->map_name);
      break;
    case PROP_COLORBAR:
      g_value_set_boolean (value, element->colorbar);
      break;
  }

  GST_OBJECT_UNLOCK (element);
}


static GstElementClass *parent_class = NULL;


static void
finalize (GObject * object)
{
  CairoVisWaterfall *element = CAIROVIS_WATERFALL (object);

  gst_object_unref (element->sinkpad);
  element->sinkpad = NULL;
  gst_object_unref (element->adapter);
  element->adapter = NULL;
  colormap_destroy (element->map);
  element->map = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
base_init (gpointer class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (class);

  gst_element_class_set_details_simple (element_class,
      "Waterfall Visualizer",
      "Filter",
      "Render a multi-channel input as a waterfall plot",
      "Leo Singer <leo.singer@ligo.org>");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink",
          GST_PAD_SINK,
          GST_PAD_ALWAYS,
          gst_caps_from_string ("audio/x-raw-float, "
              "channels   = (int) [2, MAX], " "width      = (int) 64")
      )
      );
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
      PROP_ZLABEL,
      g_param_spec_string ("z-label",
          "z-Label",
          "Label for z-axis", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (gobject_class,
      PROP_ZSCALE,
      g_param_spec_enum ("z-scale",
          "z-Scale",
          "Linear or logarithmic scale",
          CAIROVIS_SCALE_TYPE,
          CAIROVIS_SCALE_LINEAR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_ZAUTOSCALE,
      g_param_spec_boolean ("z-autoscale",
          "z-Autoscale",
          "Set to true to autoscale the z-axis",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_ZMIN,
      g_param_spec_double ("z-min",
          "z-Minimum",
          "Minimum limit of z-axis (has no effect if z-autoscale is set to true)",
          -G_MAXDOUBLE, G_MAXDOUBLE, -2.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_ZMAX,
      g_param_spec_double ("z-max",
          "z-Maximum",
          "Maximum limit of z-axis (has no effect if z-autoscale is set to true)",
          -G_MAXDOUBLE, G_MAXDOUBLE, 2.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_HISTORY,
      g_param_spec_uint64 ("history",
          "History",
          "Duration of history to keep, in nanoseconds",
          0, GST_CLOCK_TIME_NONE, 10 * GST_SECOND,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_COLORMAP,
      g_param_spec_enum ("colormap",
          "Colormap",
          "Name of colormap (e.g. 'jet')",
          CAIROVIS_COLORMAP_TYPE,
          CAIROVIS_COLORMAP_jet,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
  g_object_class_install_property (gobject_class,
      PROP_COLORBAR,
      g_param_spec_boolean ("colorbar",
          "Colorbar",
          "Set to true to make colorbar visible",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)
      );
}


static void
instance_init (GTypeInstance * object, gpointer class)
{
  CairoVisWaterfall *element = CAIROVIS_WATERFALL (object);
  GstPadTemplate *tmpl =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (class), "sink");
  GstPad *pad = gst_pad_new_from_template (tmpl, "sink");

  gst_object_ref (pad);
  gst_element_add_pad (GST_ELEMENT (element), pad);
  gst_pad_use_fixed_caps (pad);
  gst_pad_set_setcaps_function (pad, GST_DEBUG_FUNCPTR (sink_setcaps));
  gst_pad_set_chain_function (pad, GST_DEBUG_FUNCPTR (sink_chain));
  element->sinkpad = pad;

  element->adapter = gst_adapter_new ();
  element->t0 = GST_CLOCK_TIME_NONE;
  element->offset0 = GST_BUFFER_OFFSET_NONE;
  element->last_offset_end = GST_BUFFER_OFFSET_NONE;
  element->map = NULL;
  element->colorbar = FALSE;

  element->zlabel = NULL;
}


GType
cairovis_waterfall_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo info = {
      .class_size = sizeof (CairoVisWaterfallClass),
      .class_init = class_init,
      .base_init = base_init,
      .instance_size = sizeof (CairoVisWaterfall),
      .instance_init = instance_init,
    };
    type =
        g_type_register_static (CAIROVIS_BASE_TYPE, "cairovis_waterfall", &info,
        0);
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "cairovis", 0,
        "cairo visualization elements");
  }

  return type;
}
