# ##(PLUGIN_NAME)

plugindir = $(LAPIZ_PLUGINS_LIBS_DIR)

AM_CPPFLAGS = \
	-I$(top_srcdir) 						\
	$(LAPIZ_CFLAGS) 						\
	$(WARN_CFLAGS)							\
	-DLAPIZ_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

plugin_LTLIBRARIES = lib##(PLUGIN_MODULE).la

lib##(PLUGIN_MODULE)_la_SOURCES = \
	##(PLUGIN_MODULE)-plugin.h	\
	##(PLUGIN_MODULE)-plugin.c

lib##(PLUGIN_MODULE)_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
lib##(PLUGIN_MODULE)_la_LIBADD  = $(LAPIZ_LIBS)

# UI files (if you use ctkbuilder for your plugin, list those files here)
uidir = $(LAPIZ_PLUGINS_DATA_DIR)/##(PLUGIN_MODULE)
ui_DATA =

plugin_in_files = ##(PLUGIN_MODULE).lapiz-plugin.desktop.in
plugin_DATA = $(plugin_in_files:.lapiz-plugin.desktop.in=.lapiz-plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --keyword=Description --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA) $(ui_DATA)

DISTCLEANFILES = $(plugin_DATA) $(ui_DATA)

-include $(top_srcdir)/git.mk
