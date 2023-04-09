# filebrowser

plugindir = $(LAPIZ_PLUGINS_LIBS_DIR)

AM_CPPFLAGS = \
	-I$(top_srcdir) 						\
	$(LAPIZ_CFLAGS) 						\
	$(WARN_CFLAGS)

BUILT_SOURCES = \
	lapiz-file-browser-enum-types.h		\
	lapiz-file-browser-enum-types.c		\
	lapiz-file-browser-marshal.h 		\
	lapiz-file-browser-marshal.c

plugin_LTLIBRARIES = libfilebrowser.la

NOINST_H_FILES = \
	lapiz-file-bookmarks-store.h 		\
	lapiz-file-browser-store.h 		\
	lapiz-file-browser-view.h 		\
	lapiz-file-browser-widget.h 		\
	lapiz-file-browser-error.h		\
	lapiz-file-browser-utils.h		\
	lapiz-file-browser-plugin.h		\
	lapiz-file-browser-messages.h

libfilebrowser_la_SOURCES = \
	$(BUILT_SOURCES) 			\
	lapiz-file-bookmarks-store.c 		\
	lapiz-file-browser-store.c 		\
	lapiz-file-browser-view.c 		\
	lapiz-file-browser-widget.c 		\
	lapiz-file-browser-utils.c 		\
	lapiz-file-browser-plugin.c		\
	lapiz-file-browser-messages.c		\
	$(NOINST_H_FILES)

libfilebrowser_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libfilebrowser_la_LIBADD = $(LAPIZ_LIBS)

# UI files (if you use ui for your plugin, list those files here)
uidir = $(LAPIZ_PLUGINS_DATA_DIR)/filebrowser
ui_DATA = lapiz-file-browser-widget-ui.xml

plugin_in_files = filebrowser.plugin.desktop.in
plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --keyword=Name --keyword=Description --desktop --template $< -d $(top_srcdir)/po -o $@

lapiz-file-browser-enum-types.h: lapiz-file-browser-enum-types.h.template $(NOINST_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template lapiz-file-browser-enum-types.h.template $(NOINST_H_FILES)) > $@

lapiz-file-browser-enum-types.c: lapiz-file-browser-enum-types.c.template lapiz-file-browser-enum-register.c.template $(NOINST_H_FILES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && \
	$(GLIB_MKENUMS) --template lapiz-file-browser-enum-types.c.template $(NOINST_H_FILES) && \
	$(GLIB_MKENUMS) --template lapiz-file-browser-enum-register.c.template $(NOINST_H_FILES)) > $@

lapiz-file-browser-marshal.h: lapiz-file-browser-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=lapiz_file_browser_marshal > $@

lapiz-file-browser-marshal.c: lapiz-file-browser-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) echo "#include \"lapiz-file-browser-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=lapiz_file_browser_marshal >> $@

filebrowser_gschema_in = org.cafe.lapiz.plugins.filebrowser.gschema.xml.in
gsettings_SCHEMAS = $(filebrowser_gschema_in:.xml.in=.xml)
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(ui_DATA)					\
	$(plugin_in_files)				\
	$(filebrowser_gschema_in)			\
	lapiz-file-browser-enum-types.h.template	\
	lapiz-file-browser-enum-types.c.template	\
	lapiz-file-browser-enum-register.c.template	\
	lapiz-file-browser-marshal.list

CLEANFILES = \
	$(plugin_DATA)		\
	$(gsettings_SCHEMAS_in)		\
	$(gsettings_SCHEMAS)		\
	$(BUILT_SOURCES)

DISTCLEANFILES = \
	$(plugin_DATA)		\
	$(gsettings_SCHEMAS_in)		\
	$(gsettings_SCHEMAS)		\
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
