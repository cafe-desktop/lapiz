uidir = $(datadir)/lapiz/ui/

AM_CPPFLAGS = 							\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/lapiz 					\
	-I$(top_builddir)/lapiz					\
	$(PLUMA_CFLAGS) 					\
	$(WARN_CFLAGS)

noinst_LTLIBRARIES = libdialogs.la

libdialogs_la_SOURCES = 			\
	lapiz-preferences-dialog.h		\
	lapiz-preferences-dialog.c		\
	lapiz-close-confirmation-dialog.c 	\
	lapiz-close-confirmation-dialog.h 	\
	lapiz-encodings-dialog.c		\
	lapiz-encodings-dialog.h		\
	lapiz-search-dialog.h			\
	lapiz-search-dialog.c

ui_files =					\
	lapiz-encodings-dialog.ui		\
	lapiz-preferences-dialog.ui		\
	lapiz-search-dialog.ui

include $(top_srcdir)/gla11y.mk

ui_DATA = $(ui_files)

EXTRA_DIST = $(ui_DATA) ui-a11y.suppr

-include $(top_srcdir)/git.mk
