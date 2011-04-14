# Copyright (C) 2010  Leo Singer
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
"""
Auto-generate colormap_data.c, which packages Matplotlib's color data.
"""
__author__ = "Leo Singer <leo.singer@ligo.org>"


from matplotlib.cm import datad
from inspect import isfunction
import sys


header = """/*
 * Copyright (c) 2010  Leo Singer
 *
 * Colormap data from Matplotlib's matplotlib.cm module, which is
 * Copyright (c) 2002-2009 John D. Hunter; All Rights Reserved
 *
 */

"""
 
# Select all colormaps to print
datad_items = sorted([(key, value) for key, value in datad.items() if hasattr(value, 'iteritems') and not(isfunction(value['red']) or isfunction(value['green']) or isfunction(value['blue']))])

# Write header file
file = open('colormap_data.h', 'w')
try:
	print >>file, header
	print >>file, "#ifndef __CAIROVIS_COLORMAP_DATA_H__"
	print >>file, "#define __CAIROVIS_COLORMAP_DATA_H__"
	print >>file
	print >>file, "enum cairovis_colormap_name"
	print >>file, "{"
	for key, value in datad_items:
		print >>file, "  CAIROVIS_COLORMAP_%s," % key
	print >>file, "};"
	print >>file, ""
	print >>file, "#endif"
finally:
	file.close()

# Write C file

file = open('colormap_data.c', 'w')
try:
	print >>file, header
	print >>file, """#include "colormap.h"
#include <glib.h>
#include <glib-object.h>

gboolean colormap_get_data_by_name(enum cairovis_colormap_name key, colormap_data *data)
{
  switch (key)
  {"""
	for key, value in datad_items:
		print >>file, '    case CAIROVIS_COLORMAP_%s: {' % key
		for color in ('red', 'green', 'blue'):
			print >>file, '    {'
			print >>file, '      const double x[] = {', ','.join([repr(x) for x, y0, y1 in sorted(value[color])]), '};'
			print >>file, '      const double y[] = {', ','.join([repr(y1) for x, y0, y1 in sorted(value[color])]), '};'
			print >>file, '      data->%s.len = sizeof(x) / sizeof(double);' % color
			print >>file, '      data->%s.x = g_memdup(x, sizeof(x));' % color
			print >>file, '      data->%s.y = g_memdup(y, sizeof(y));' % color
			print >>file, '    }'
		print >>file, '    } return TRUE; break;'
	print >>file, '    default: return FALSE;'
	print >>file, "  }"
	print >>file, "}"
	print >>file, """

GType cairovis_colormap_get_type (void)
{
  static GType tp = 0;
  static const GEnumValue values[] = {"""
	for key, value in datad_items:
		print >>file, '    {CAIROVIS_COLORMAP_%s, "%s", "%s"},' % (key, key, key)
	print >>file, """    {0, NULL, NULL},
  };

  if (G_UNLIKELY (tp == 0)) {
    tp = g_enum_register_static ("CairoVisColormap", values);
  }
  return tp;
}"""
finally:
	file.close()
