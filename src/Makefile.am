# Note: plugindir is set in configure

# compile for debugging ?
if DEBUG
    DEBUG_CFLAGS = -DDEBUG
endif



plugin_LTLIBRARIES = libgstniftyled.la

# sources used to compile this plug-in
libgstniftyled_la_SOURCES = gstniftyled.c gstniftyled.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstniftyled_la_CFLAGS = $(GST_CFLAGS) $(niftyled_CFLAGS) $(DEBUG_CFLAGS)
libgstniftyled_la_LIBADD = $(GST_LIBS) $(niftyled_LIBS)
libgstniftyled_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstniftyled_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstniftyled.h

