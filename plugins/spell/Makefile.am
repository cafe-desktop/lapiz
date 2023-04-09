# Spell checker plugin
plugindir = $(PLUMA_PLUGINS_LIBS_DIR)

AM_CPPFLAGS = 							\
	-I$(top_srcdir)						\
	$(PLUMA_CFLAGS) 					\
	$(ENCHANT_CFLAGS)					\
	$(WARN_CFLAGS)

BUILT_SOURCES = 					\
	lapiz-spell-marshal.c				\
	lapiz-spell-marshal.h

plugin_LTLIBRARIES = libspell.la

libspell_la_SOURCES = \
	lapiz-spell-plugin.c 				\
	lapiz-spell-plugin.h 				\
	lapiz-spell-checker.c 				\
	lapiz-spell-checker.h				\
	lapiz-spell-checker-dialog.c			\
	lapiz-spell-checker-dialog.h			\
	lapiz-spell-checker-language.c			\
	lapiz-spell-checker-language.h			\
	lapiz-spell-language-dialog.c			\
	lapiz-spell-language-dialog.h			\
	lapiz-automatic-spell-checker.c			\
	lapiz-automatic-spell-checker.h			\
	lapiz-spell-utils.c				\
	lapiz-spell-utils.h				\
	$(BUILT_SOURCES)			

libspell_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS) 
libspell_la_LIBADD  = $(PLUMA_LIBS) $(ENCHANT_LIBS)

uidir = $(PLUMA_PLUGINS_DATA_DIR)/spell
ui_files = spell-checker.ui languages-dialog.ui lapiz-spell-setup-dialog.ui
include $(top_srcdir)/gla11y.mk
ui_DATA = $(ui_files)

lapiz-spell-marshal.h: lapiz-spell-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=lapiz_marshal > $@

lapiz-spell-marshal.c: lapiz-spell-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) echo "#include \"lapiz-spell-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=lapiz_marshal >> $@

plugin_in_files = spell.plugin.desktop.in
plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --keyword=Name --keyword=Description --desktop --template $< -d $(top_srcdir)/po -o $@

spell_gschema_in = org.mate.lapiz.plugins.spell.gschema.xml.in
gsettings_SCHEMAS = $(spell_gschema_in:.xml.in=.xml)
@GSETTINGS_RULES@


EXTRA_DIST = 					\
	$(ui_DATA)				\
	$(plugin_in_files)			\
	lapiz-spell-marshal.list    	        \
	$(spell_gschema_in)			\
	ui-a11y.suppr

CLEANFILES = $(BUILT_SOURCES) $(plugin_DATA) $(gsettings_SCHEMAS)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
