## Process this file with automake to produce Makefile.in

# see /ctk-doc/examples/Makefile.am

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# The name of the module, e.g. 'glib'.
DOC_MODULE=lapiz

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# ctk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
DOC_SOURCE_DIR=../../lapiz

# Extra options to pass to ctkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to ctkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="CTK_DISABLE_DEPRECATED" 
SCAN_OPTIONS=

# Extra options to supply to ctkdoc-mkdb.
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to ctkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

# Extra options to supply to ctkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../cdk-pixbuf/html --extra-dir=../cdk/html
FIXXREF_OPTIONS=

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB=$(top_srcdir)/lapiz/*.h
CFILE_GLOB=$(top_srcdir)/lapiz/*.c

# Header files to ignore when scanning (These are internal to lapiz).
IGNORE_HFILES=		\
	lapiz-commands.h			\
	lapiz-document-loader.h			\
	lapiz-document-saver.h			\
	lapiz-documents-panel.h			\
	lapiz-io-error-message-area.h		\
	lapiz-languages-manager.h		\
	lapiz-plugins-engine.h			\
	lapiz-prefs-manager-private.h		\
	lapiz-session.h				\
	lapiz-ui.h				\
	lapiz-window-private.h			\
	lapiz-output-window.h			\
	lapiz-close-confirmation-dialog.h	\
	lapiz-encodings-dialog.h		\
	lapiz-open-location-dialog.h		\
	lapiz-page-setup-dialog.h		\
	lapiz-preferences-dialog.h		\
	lapiz-search-dialog.h			\
	lapiz-marshal.h				\
	lapiz-enum-types.h

# These files are not part of lapiz but were copied into it for some reason.
# Do not parse them to make the docs.
IGNORE_HFILES +=	\
	bacon-message-connection.h		\
	lapiztextregion.h			\
	sexy-icon-entry.h

# Images to copy into HTML directory.
HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files=

# SGML files where ctk-doc abbrevations (#CtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# CFLAGS and LDFLAGS for compiling ctkdoc-scangobj with your library.
# Only needed if you are using ctkdoc-scangobj to dynamically query widget
# signals and properties.
CTKDOC_CFLAGS=	\
	-I$(top_srcdir)/lapiz	\
	-I$(top_builddir)/lapiz \
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	$(LAPIZ_CFLAGS)

CTKDOC_LIBS=	\
	$(top_builddir)/lapiz/liblapiz.la	\
	$(LAPIZ_LIBS)

# This includes the standard ctk-doc make rules, copied by ctkdocize.
include $(top_srcdir)/ctk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in

regenerate-types:
	grep _get_type $(DOC_SOURCE_DIR)/lapiz-*.h | grep -oR '^.*\.h' | sort | uniq - | sed -r 's/^.*\.h$/#include "\0"/' > lapiz.types
	grep _get_type $(DOC_SOURCE_DIR)/lapiz-*.h | grep -oR '^.*\.h' | sort | uniq - | sed -re 'y/-/_/' -e 's/^(.*)\.h$/\1_get_type/' >> lapiz.types.new

-include $(top_srcdir)/git.mk
