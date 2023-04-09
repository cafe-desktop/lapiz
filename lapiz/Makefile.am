## Process this file with automake to produce Makefile.in
SUBDIRS = dialogs smclient

bin_PROGRAMS = lapiz

noinst_LTLIBRARIES = liblapiz.la

AM_CPPFLAGS =								\
	-I$(top_srcdir)							\
	-I$(srcdir)							\
	-I$(srcdir)/smclient						\
	$(PLUMA_CFLAGS)							\
	$(WARN_CFLAGS)							\
	$(INTROSPECTION_CFLAGS)						\
	-DDATADIR=\""$(datadir)"\"					\
	-DPLUMA_DATADIR=\""$(datadir)/lapiz"\"				\
	-DLIBDIR=\""$(libdir)"\"

lapiz_SOURCES = \
	lapiz.c

lapiz_LDADD = liblapiz.la $(PLUMA_LIBS) $(EGG_SMCLIENT_LIBS) $(INTROSPECTION_LIBS)

lapiz_LDFLAGS = -export-dynamic -no-undefined -export-symbols-regex "^[[^_]].*"

liblapiz_la_LDFLAGS = -export-dynamic -no-undefined -export-symbols-regex "^[[^_]].*"

liblapiz_la_LIBADD = \
	dialogs/libdialogs.la		\
	smclient/libeggsmclient.la

# PLUMA_LIBS must be the last to ensure correct order on some platforms
liblapiz_la_LIBADD += $(PLUMA_LIBS) -lICE

BUILT_SOURCES = 			\
	lapiz-enum-types.c		\
	lapiz-enum-types.h		\
	lapiz-marshal.c			\
	lapiz-marshal.h


NOINST_H_FILES =			\
	lapiz-close-button.h		\
	lapiz-dirs.h			\
	lapiz-document-input-stream.h	\
	lapiz-document-loader.h		\
	lapiz-document-output-stream.h	\
	lapiz-document-saver.h		\
	lapiz-documents-panel.h		\
	lapiz-file-chooser-dialog.h	\
	lapiz-gio-document-loader.h	\
	lapiz-gio-document-saver.h	\
	lapiz-history-entry.h		\
	lapiz-io-error-message-area.h	\
	lapiz-language-manager.h	\
	lapiz-plugins-engine.h		\
	lapiz-prefs-manager-private.h	\
	lapiz-print-job.h		\
	lapiz-print-preview.h		\
	lapiz-session.h			\
	lapiz-smart-charset-converter.h	\
	lapiz-style-scheme-manager.h	\
	lapiz-tab-label.h		\
	lapiztextregion.h		\
	lapiz-ui.h			\
	lapiz-window-private.h

INST_H_FILES =				\
	lapiz-app.h			\
	lapiz-commands.h		\
	lapiz-debug.h			\
	lapiz-document.h 		\
	lapiz-encodings.h		\
	lapiz-encodings-combo-box.h	\
	lapiz-help.h 			\
	lapiz-message-bus.h		\
	lapiz-message-type.h		\
	lapiz-message.h			\
	lapiz-notebook.h		\
	lapiz-panel.h			\
	lapiz-prefs-manager-app.h	\
	lapiz-prefs-manager.h		\
	lapiz-progress-message-area.h	\
	lapiz-statusbar.h		\
	lapiz-status-combo-box.h	\
	lapiz-tab.h 			\
	lapiz-utils.h 			\
	lapiz-view.h 			\
	lapiz-window.h

if !ENABLE_GVFS_METADATA
INST_H_FILES += lapiz-metadata-manager.h
endif

headerdir = $(prefix)/include/lapiz

header_DATA = 				\
	$(INST_H_FILES)

liblapiz_c_files = 			\
	lapiz-app.c			\
	lapiz-close-button.c		\
	lapiz-commands-documents.c	\
	lapiz-commands-edit.c		\
	lapiz-commands-file.c		\
	lapiz-commands-file-print.c	\
	lapiz-commands-help.c		\
	lapiz-commands-search.c		\
	lapiz-commands-view.c		\
	lapiz-debug.c			\
	lapiz-dirs.c			\
	lapiz-document.c 		\
	lapiz-document-input-stream.c	\
	lapiz-document-loader.c		\
	lapiz-document-output-stream.c	\
	lapiz-gio-document-loader.c	\
	lapiz-document-saver.c		\
	lapiz-gio-document-saver.c	\
	lapiz-documents-panel.c		\
	lapiz-encodings.c		\
	lapiz-encodings-combo-box.c	\
	lapiz-file-chooser-dialog.c	\
	lapiz-help.c			\
	lapiz-history-entry.c		\
	lapiz-io-error-message-area.c	\
	lapiz-language-manager.c	\
	lapiz-message-bus.c		\
	lapiz-message-type.c		\
	lapiz-message.c			\
	lapiz-notebook.c		\
	lapiz-panel.c			\
	lapiz-plugins-engine.c		\
	lapiz-prefs-manager-app.c	\
	lapiz-prefs-manager.c		\
	lapiz-prefs-manager-private.h	\
	lapiz-print-job.c		\
	lapiz-print-preview.c		\
	lapiz-progress-message-area.c	\
	lapiz-session.c			\
	lapiz-smart-charset-converter.c	\
	lapiz-statusbar.c		\
	lapiz-status-combo-box.c	\
	lapiz-style-scheme-manager.c	\
	lapiz-tab.c 			\
	lapiz-tab-label.c		\
	lapiz-utils.c 			\
	lapiz-view.c 			\
	lapiz-window.c			\
	lapiztextregion.c

liblapiz_la_SOURCES = 			\
	$(liblapiz_c_files)		\
	$(BUILT_SOURCES)		\
	$(BACON_FILES)			\
	$(POSIXIO_FILES)		\
	$(NOINST_H_FILES)		\
	$(INST_H_FILES)

if !ENABLE_GVFS_METADATA
liblapiz_la_SOURCES += lapiz-metadata-manager.c
endif

lapiz-enum-types.h: lapiz-enum-types.h.template $(INST_H_FILES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template lapiz-enum-types.h.template $(INST_H_FILES)) > $@

lapiz-enum-types.c: lapiz-enum-types.c.template $(INST_H_FILES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template lapiz-enum-types.c.template $(INST_H_FILES)) > $@

lapiz-marshal.h: lapiz-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=lapiz_marshal > $@

lapiz-marshal.c: lapiz-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) echo "#include \"lapiz-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=lapiz_marshal >> $@

aboutdir = $(datadir)/lapiz/
about_DATA =				\
	lapiz.about			\
	$(NULL)

uidir = $(datadir)/lapiz/ui/
ui_files = lapiz-print-preferences.ui
include $(top_srcdir)/gla11y.mk
ui_DATA = 				\
	lapiz-ui.xml			\
	$(ui_files)

EXTRA_DIST = 				\
	$(ui_DATA)			\
	$(about_DATA)			\
	lapiz-enum-types.h.template	\
	lapiz-enum-types.c.template	\
	lapiz-marshal.list		\
	lapiz.rc			\
	ui-a11y.suppr

CLEANFILES = $(BUILT_SOURCES)

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = Pluma-1.0.gir
INTROSPECTION_SCANNER_ARGS = -I$(top_srcdir) --warn-all

Pluma-1.0.gir: lapiz
Pluma_1_0_gir_INCLUDES = Gtk-3.0 GtkSource-3.0
Pluma_1_0_gir_FILES = $(INST_H_FILES) $(liblapiz_c_files) $(BUILT_SOURCES)
Pluma_1_0_gir_VERSION = 1.0
Pluma_1_0_gir_PROGRAM = $(builddir)/lapiz
Pluma_1_0_gir_NAMESPACE = Pluma

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

BACON_DIR=$(srcdir)/../../libbacon/src/
BACON_FILES=bacon-message-connection.h bacon-message-connection.c

regenerate-built-sources:
	BACONFILES="$(BACON_FILES)" BACONDIR="$(BACON_DIR)" $(top_srcdir)/lapiz/update-from-bacon.sh

-include $(top_srcdir)/git.mk
