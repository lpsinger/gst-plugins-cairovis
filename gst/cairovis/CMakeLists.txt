add_definitions (-DHAVE_CONFIG_H)

set (SOURCES
	cairovis.c
	cairovis_base.c
	cairovis_histogram.c
	cairovis_lineseries.c
  cairovis_waterfall.c
  colormap.c
  colormap_data.c)
    
set (HEADERS
	cairovis_base.h
	cairovis_histogram.h
	cairovis_lineseries.h
  cairovis_waterfall.h
  colormap.h
  colormap_data.h)
    
include_directories(AFTER
  ${CAIRO_INCLUDE_DIR}
  ${GSL_INCLUDE_DIR}
  . )

add_library (libgstcairovis MODULE
  ${SOURCES}
  ${HEADERS})
  
target_link_libraries (libgstcairovis
  general ${GLIB2_LIBRARIES}
  general ${GSTREAMER_LIBRARIES}
  general ${GSTREAMER_BASE_LIBRARY}
  general ${GSTREAMER_VIDEO_LIBRARY}
  general ${CAIRO_LIBRARIES}
  general ${GSL_INTERPOLATION_LIBRARY}
  general ${GSL_ERR_LIBRARY}
  general ${GSL_SYS_LIBRARY})
  
install (TARGETS libgstcairovis
    LIBRARY DESTINATION lib/gstreamer-0.10)
    