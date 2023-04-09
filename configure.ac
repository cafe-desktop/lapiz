dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.63.2)

m4_define(lapiz_major_version, 1)
m4_define(lapiz_minor_version, 24)
m4_define(lapiz_micro_version, 1)
m4_define(lapiz_version, lapiz_major_version.lapiz_minor_version.lapiz_micro_version)

AC_INIT([lapiz], [lapiz_version],
    [https://github.com/cafe-desktop/lapiz/issues],
    [lapiz], [http://cafe-desktop.org])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR(lapiz/lapiz.c)
AC_CONFIG_MACRO_DIR([m4])

AC_DEFINE(LAPIZ_MAJOR_VERSION, lapiz_major_version, [Lapiz major version])
AC_SUBST(LAPIZ_MAJOR_VERSION, lapiz_major_version)
AC_DEFINE(LAPIZ_MINOR_VERSION, lapiz_minor_version, [Lapiz minor version])
AC_SUBST(LAPIZ_MINOR_VERSION, lapiz_minor_version)
AC_DEFINE(LAPIZ_MICRO_VERSION, lapiz_micro_version, [Lapiz micro version])
AC_SUBST(LAPIZ_MICRO_VERSION, lapiz_micro_version)

AM_INIT_AUTOMAKE([1.10 dist-xz no-dist-gzip -Wno-portability check-news])
AM_MAINTAINER_MODE
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

CAFE_COMPILE_WARNINGS

# Checks for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_SYS_LARGEFILE
PKG_PROG_PKG_CONFIG
AC_PATH_PROG([GLA11Y], [gla11y], [true])

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT(disable-static)

CTK_DOC_CHECK([1.0],[--flavour=no-tmpl])

AC_CHECK_LIB(m, floor)

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "$ACLOCAL_FLAGS -I m4")

dnl toolbar style for GSettings schemas
TOOLBAR_STYLE="LAPIZ_TOOLBAR_SYSTEM"
AC_SUBST(TOOLBAR_STYLE)

dnl ===============================================================
dnl Expanded dirs
dnl ===============================================================

# This macro expands DIR and assigns it to RET.
# If DIR is NONE, then it's replaced by DEFAULT.
# Based on AC_DEFINE_DIR
AC_DEFUN([LAPIZ_FULLPATH], [
  test "x$prefix" = xNONE && prefix="$ac_default_prefix"
  test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
  ac_expand=[$]$1
  test "x$ac_expand" = xNONE && ac_expand="[$]$2"
  ac_expand=`eval echo [$]ac_expand`
  $3=`eval echo [$]ac_expand`
])

dnl FULL_LIBEXECDIR is used for X-CAFE-Bugzilla-ExtraInfoScript expansion
dnl in data/lapiz.desktop.in.in
LAPIZ_FULLPATH(libexecdir, NONE, FULL_LIBEXECDIR)
AC_SUBST(FULL_LIBEXECDIR)

dnl ================================================================
dnl Gettext stuff.
dnl ================================================================
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_GNU_GETTEXT([external])


GETTEXT_PACKAGE=lapiz
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define to the Gettext package name])

dnl ================================================================
dnl Use gvfs metadata or the old xml file store.
dnl ================================================================

AC_ARG_ENABLE([gvfs-metadata],
	AS_HELP_STRING([--enable-gvfs-metadata], [Enable using gvfs to store metadata]),
	[enable_gvfs_metadata=$enableval],
	[enable_gvfs_metadata=yes])

AM_CONDITIONAL(ENABLE_GVFS_METADATA, test "x$enable_gvfs_metadata" != "xno")

if test "x$enable_gvfs_metadata" != "xno"; then
	AC_DEFINE([ENABLE_GVFS_METADATA], [1], [Define to enable using gvfs metadata])
fi

dnl ================================================================
dnl spell plugins checks: enchant and iso-codes
dnl ================================================================

ENCHANT_REQUIRED=1.6.0
ISO_CODES_REQUIRED=0.35

AC_ARG_ENABLE([spell],
	AS_HELP_STRING([--disable-spell],[Disable spell plugin (default: enabled)]),
	[enable_enchant=$enableval],
	[enable_enchant=yes])

if test "x$enable_enchant" = "xyes" ; then
        ENCHANT_MODNAME=enchant
        PKG_CHECK_EXISTS([enchant-2 >= $ENCHANT_REQUIRED], [have_enchant_2=yes], [have_enchant_2=no])
        if test "x$have_enchant_2" = "xyes"; then
                ENCHANT_MODNAME=enchant-2
        fi

        AC_MSG_CHECKING([the version of enchant])
        ENCHANT_VERSION=`pkg-config --modversion $ENCHANT_MODNAME`
        AC_MSG_RESULT([$ENCHANT_VERSION])

        PKG_CHECK_MODULES(ENCHANT, [$ENCHANT_MODNAME >= $ENCHANT_REQUIRED], \
                          have_enchant=yes, have_enchant=no)
	if test "x$have_enchant" = "xyes"; then

		PKG_CHECK_EXISTS([iso-codes >= $ISO_CODES_REQUIRED],
				 [have_iso_codes=yes],[have_iso_codes=no])

		if test "x$have_iso_codes" = "xyes"; then
			AC_MSG_CHECKING([whether iso-codes has iso-639 and iso-3166 domains])
			if $PKG_CONFIG --variable=domains iso-codes | grep 639 > /dev/null && \
			   $PKG_CONFIG --variable=domains iso-codes | grep 3166 > /dev/null ; then
				result=yes
			else
				result=no
				have_iso_codes=no
			fi
			AC_MSG_RESULT([$result])
		fi

		if test "x$have_iso_codes" = "xyes"; then
			AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
			AC_DEFINE([HAVE_ISO_CODES],[1],[Define if you have the iso-codes package])
		else
			AC_MSG_ERROR([iso-codes is required to build the spell plugin. Use --disable-spell to build without spell plugin.])
		fi
	else
		enable_enchant=no
		AC_MSG_ERROR([Enchant library not found or too old. Use --disable-spell to build without spell plugin.])
	fi
fi

AM_CONDITIONAL(ENABLE_ENCHANT, test x"$enable_enchant" = "xyes")

CTK_REQUIRED=3.22.0
CTKSOURCEVIEW_REQUIRED=3.0.0

PKG_CHECK_MODULES(GMODULE,gmodule-2.0,[GMODULE_ADD="gmodule-2.0"],[GMODULE_ADD=""])
PKG_CHECK_MODULES(LAPIZ, [
	libxml-2.0 >= 2.5.0
	glib-2.0 >= 2.50.0
	$GMODULE_ADD
	gthread-2.0 >= 2.13.0
	gio-2.0 >= 2.50.0
	ctk+-3.0 >= $CTK_REQUIRED
	ctksourceview-3.0 >= $CTKSOURCEVIEW_REQUIRED
	libpeas-1.0 >= 1.2.0
	libpeas-ctk-1.0 >= 1.2.0
])

PKG_CHECK_MODULES(X11, [x11])

LAPIZ_CFLAGS="$LAPIZ_CFLAGS $X11_CFLAGS"
LAPIZ_LIBS="$LAPIZ_LIBS $X11_LIBS"

AC_SUBST(LAPIZ_CFLAGS)
AC_SUBST(LAPIZ_LIBS)

PKG_CHECK_MODULES(EGG_SMCLIENT, [sm >= 1.0.0])

AC_SUBST(EGG_SMCLIENT_CFLAGS)
AC_SUBST(EGG_SMCLIENT_LIBS)

# Introspection
GOBJECT_INTROSPECTION_CHECK([0.9.3])

if test "$found_introspection" = "yes"; then
	have_introspection=yes
	AC_DEFINE([HAVE_INTROSPECTION], [1], [Define to enable GObject Introspection])
else
	have_introspection=no
fi

AM_PATH_PYTHON([3.0])

dnl ================================================================
dnl GSettings related settings
dnl ================================================================

GLIB_GSETTINGS

dnl ================================================================
dnl Misc
dnl ================================================================
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

AC_ARG_ENABLE([tests],
	AS_HELP_STRING([--enable-tests], [Enable the tests]),
	[enable_tests=$enableval],
	[enable_tests=yes])

AM_CONDITIONAL(ENABLE_TESTS, test x$enable_tests = xyes)

PLUGIN_LIBTOOL_FLAGS="-module -avoid-version"

AC_SUBST(PLUGIN_LIBTOOL_FLAGS)

LAPIZ_PLUGINS_DATA_DIR="$datadir/lapiz/plugins"
AC_SUBST(LAPIZ_PLUGINS_DATA_DIR)

LAPIZ_PLUGINS_LIBS_DIR="$libdir/lapiz/plugins"
AC_SUBST(LAPIZ_PLUGINS_LIBS_DIR)

YELP_HELP_INIT

AC_CONFIG_FILES([
Makefile
data/lapiz.desktop.in
data/lapiz-bugreport.sh
data/lapiz.pc
data/org.cafe.lapiz.gschema.xml
data/Makefile
docs/Makefile
docs/reference/Makefile
lapiz/dialogs/Makefile
lapiz/smclient/Makefile
lapiz/Makefile
help/Makefile
pixmaps/Makefile
plugins/Makefile
plugins/changecase/Makefile
plugins/docinfo/Makefile
plugins/externaltools/data/Makefile
plugins/externaltools/Makefile
plugins/externaltools/scripts/Makefile
plugins/externaltools/tools/Makefile
plugins/filebrowser/Makefile
plugins/filebrowser/org.cafe.lapiz.plugins.filebrowser.gschema.xml
plugins/modelines/Makefile
plugins/pythonconsole/Makefile
plugins/pythonconsole/pythonconsole/Makefile
plugins/pythonconsole/org.cafe.lapiz.plugins.pythonconsole.gschema.xml
plugins/quickopen/Makefile
plugins/quickopen/quickopen/Makefile
plugins/snippets/data/lang/Makefile
plugins/snippets/data/Makefile
plugins/snippets/Makefile
plugins/snippets/snippets/Makefile
plugins/sort/Makefile
plugins/spell/Makefile
plugins/spell/org.cafe.lapiz.plugins.spell.gschema.xml
plugins/taglist/Makefile
plugins/time/Makefile
plugins/time/org.cafe.lapiz.plugins.time.gschema.xml
plugins/trailsave/Makefile
po/Makefile.in
tests/Makefile
])

AC_OUTPUT

echo "

Configuration:

	Source code location:	${srcdir}
	Compiler:		${CC}
	Compiler flags:		${CFLAGS}
	Warning flags:		${WARN_CFLAGS}
	Spell Plugin enabled:	$enable_enchant
	Gvfs metadata enabled:	$enable_gvfs_metadata
	GObject Introspection:	${have_introspection}
	Tests enabled:		$enable_tests
"

if expr ${LAPIZ_MINOR_VERSION} % 2 > /dev/null; then
	stable_version=`expr ${LAPIZ_MINOR_VERSION} - 1`

cat << EOF
 *** IMPORTANT ***

This is an unstable version of lapiz.
It is for test purposes only.

Please, DO NOT use it in a production environment.
It will probably crash and you will lose your data.

If you are looking for a stable release of lapiz, either download
it from:

  http://pub.cafe-desktop.org/release/1.${stable_version}/

Or checkout the 1.${stable_version} branch of the lapiz module from:

  git://github.com/cafe-desktop/lapiz.git

Thanks,
The lapiz team

 *** END OF WARNING ***
EOF

fi
