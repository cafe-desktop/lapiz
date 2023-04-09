# Tag list plugin
plugindir = $(LAPIZ_PLUGINS_LIBS_DIR)

taglistdir = $(LAPIZ_PLUGINS_DATA_DIR)/taglist

taglist_in_files = \
	HTML.tags.xml.in	\
	Latex.tags.xml.in	\
	XSLT.tags.xml.in \
	XUL.tags.xml.in

taglist_DATA = $(taglist_in_files:.tags.xml.in=.tags.gz)

AM_CPPFLAGS = 							\
	-I$(top_srcdir) 					\
	$(LAPIZ_CFLAGS) 					\
	$(WARN_CFLAGS)

plugin_LTLIBRARIES = libtaglist.la

libtaglist_la_SOURCES = \
	lapiz-taglist-plugin-parser.c	\
	lapiz-taglist-plugin-parser.h	\
	lapiz-taglist-plugin-panel.c	\
	lapiz-taglist-plugin-panel.h	\
	lapiz-taglist-plugin.c		\
	lapiz-taglist-plugin.h

libtaglist_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libtaglist_la_LIBADD  = $(LAPIZ_LIBS)

plugin_in_files = taglist.plugin.desktop.in
plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --keyword=Name --keyword=Description --desktop --template $< -d $(top_srcdir)/po -o $@

GZIP_ENV = -9

%.tags.gz: %.tags.xml.in
	$(AM_V_GEN) GETTEXTDATADIR=$(top_srcdir) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $(@:.gz=)
	GZIP=$(GZIP_ENV) gzip -n -f $(@:.gz=)

EXTRA_DIST = \
	$(taglist_in_files) $(taglist_DATA)	\
	$(plugin_in_files) $(plugin_DATA)

CLEANFILES = \
	$(taglist_DATA)	\
	$(plugin_DATA)
DISTCLEANFILES = \
	$(taglist_DATA)	\
	$(plugin_DATA)




-include $(top_srcdir)/git.mk
