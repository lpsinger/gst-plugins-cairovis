rem ########################################################################
set GSTREAMER_DIR=C:\gstreamer
set LIBXML2_DIR=%GSTREAMER_DIR%
set LIBICONV_DIR=%GSTREAMER_DIR%
set GLIB2_DIR=%GSTREAMER_DIR%
set CAIRO_DIR=%GSTREAMER_DIR%
set GSL_DIR=C:\Users\joshua.doe\Documents\Devel\gsl-1.8\dist

rem Plugins will be installed under %CMAKE_PREFIX_PATH%\lib\gstreamer-0.10
set CMAKE_PREFIX_PATH=C:\gstreamer

rem cd mingw32
rem del *ache* && cmake -G "MinGW Makefiles" ..

rem cd vs9
rem del *ache* && cmake -G "Visual Studio 9 2008" ..

rem cmd
rem ########################################################################

cmake-gui