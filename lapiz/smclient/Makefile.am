platform_defines = -DEGG_SM_CLIENT_BACKEND_XSMP
platform_libs = libeggdesktopfile.la
platform_ltlibraries = libeggdesktopfile.la
platform_sources = eggsmclient-xsmp.c

AM_CPPFLAGS =                               \
	-DG_LOG_DOMAIN=\""EggSMClient"\" \
	$(LAPIZ_CFLAGS)			 \
	$(platform_defines)              \
	$(EGG_SMCLIENT_CFLAGS)

noinst_LTLIBRARIES =                     \
	libeggsmclient.la                \
	$(platform_ltlibraries)

libeggsmclient_la_LIBADD =               \
	$(EGG_SMCLIENT_LIBS)             \
	$(platform_libs)

libeggsmclient_la_LDFLAGS =              \
	$(platform_ldflags)

libeggsmclient_la_SOURCES =              \
	eggsmclient.c                    \
	eggsmclient.h                    \
	eggsmclient-private.h            \
	$(platform_sources)

libeggdesktopfile_la_LIBADD =            \
	$(EGG_LIBS)

libeggdesktopfile_la_SOURCES =           \
	eggdesktopfile.c                 \
	eggdesktopfile.h

EXTRA_DIST =                             \
	eggsmclient-xsmp.c

-include $(top_srcdir)/git.mk
