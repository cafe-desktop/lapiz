# sort plugin
plugindir = $(LAPIZ_PLUGINS_LIBS_DIR)

AM_CPPFLAGS = \
	-I$(top_srcdir) 				\
	$(LAPIZ_CFLAGS) 				\
	$(WARN_CFLAGS)

plugin_LTLIBRARIES = libsort.la

libsort_la_SOURCES = \
	lapiz-sort-plugin.h	\
	lapiz-sort-plugin.c

libsort_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libsort_la_LIBADD  = $(LAPIZ_LIBS)

uidir = $(LAPIZ_PLUGINS_DATA_DIR)/sort
ui_files = sort.ui
include $(top_srcdir)/gla11y.mk
ui_DATA = $(ui_files)

plugin_in_files = sort.plugin.desktop.in
plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --keyword=Name --keyword=Description --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(ui_DATA) $(plugin_in_files) ui-a11y.suppr

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)


-include $(top_srcdir)/git.mk
