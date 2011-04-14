plugin_LTLIBRARIES = libgstcairovis.la

# FIXME: is it OK to have Matplotlib as a build dependency?
#colormap_data.c colormap_data.h: gencolormap.py
#	python $<

# sources used to compile this plug-in
libgstcairovis_la_SOURCES = \
  cairovis.c \
  colormap_data.h colormap_data.c \
  colormap.h colormap.c \
  cairovis_base.h cairovis_base.c \
  cairovis_histogram.h cairovis_histogram.c \
  cairovis_lineseries.h cairovis_lineseries.c \
  cairovis_waterfall.h cairovis_waterfall.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstcairovis_la_CFLAGS = $(GST_CFLAGS) $(CAIRO_CFLAGS) $(GSL_CFLAGS)
libgstcairovis_la_LIBADD = $(GST_LIBS) $(CAIRO_LIBS) $(GSL_LIBS)
libgstcairovis_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstcairovis_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = \
  colormap_data.h \
  colormap.h \
  cairovis_base.h \
  cairovis_histogram.h \
  cairovis_lineseries.h \
  cairovis_waterfall.h
