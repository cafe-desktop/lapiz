# time plugin
plugindir = $(LAPIZ_PLUGINS_LIBS_DIR)

AM_CPPFLAGS = \
	-I$(top_srcdir) 				\
	$(LAPIZ_CFLAGS) 				\
	$(WARN_CFLAGS)

plugin_LTLIBRARIES = libtime.la

libtime_la_SOURCES = \
	lapiz-time-plugin.h	\
	lapiz-time-plugin.c

libtime_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libtime_la_LIBADD  = $(LAPIZ_LIBS)

uidir = $(LAPIZ_PLUGINS_DATA_DIR)/time
ui_files = \
	lapiz-time-dialog.ui				\
	lapiz-time-setup-dialog.ui
include $(top_srcdir)/gla11y.mk
ui_DATA = $(ui_files)

plugin_in_files = time.plugin.desktop.in
plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --keyword=Name --keyword=Description --desktop --template $< -d $(top_srcdir)/po -o $@

time_gschema_in = org.cafe.lapiz.plugins.time.gschema.xml.in
gsettings_SCHEMAS = $(time_gschema_in:.xml.in=.xml)
@GSETTINGS_RULES@

EXTRA_DIST = $(ui_DATA) $(plugin_in_files) $(time_gschema_in) ui-a11y.suppr

CLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS_in) $(gsettings_SCHEMASn)
DISTCLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS_in) $(gsettings_SCHEMAS)


-include $(top_srcdir)/git.mk
