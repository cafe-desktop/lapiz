# Python snippets plugin
SUBDIRS = snippets data
plugindir = $(LAPIZ_PLUGINS_LIBS_DIR)

plugin_in_files = snippets.plugin.desktop.in
plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --keyword=Name --keyword=Description --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
