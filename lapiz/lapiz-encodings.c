/*
 * lapiz-encodings.c
 * This file is part of lapiz
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Modified by the lapiz Team, 2002-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>

#include "lapiz-encodings.h"


struct _LapizEncoding
{
	gint   index;
	const gchar *charset;
	const gchar *name;
};

/*
 * The original versions of the following tables are taken from profterm
 *
 * Copyright (C) 2002 Red Hat, Inc.
 */

typedef enum
{

  LAPIZ_ENCODING_ISO_8859_1,
  LAPIZ_ENCODING_ISO_8859_2,
  LAPIZ_ENCODING_ISO_8859_3,
  LAPIZ_ENCODING_ISO_8859_4,
  LAPIZ_ENCODING_ISO_8859_5,
  LAPIZ_ENCODING_ISO_8859_6,
  LAPIZ_ENCODING_ISO_8859_7,
  LAPIZ_ENCODING_ISO_8859_8,
  LAPIZ_ENCODING_ISO_8859_9,
  LAPIZ_ENCODING_ISO_8859_10,
  LAPIZ_ENCODING_ISO_8859_13,
  LAPIZ_ENCODING_ISO_8859_14,
  LAPIZ_ENCODING_ISO_8859_15,
  LAPIZ_ENCODING_ISO_8859_16,

  LAPIZ_ENCODING_UTF_7,
  LAPIZ_ENCODING_UTF_16,
  LAPIZ_ENCODING_UTF_16_BE,
  LAPIZ_ENCODING_UTF_16_LE,
  LAPIZ_ENCODING_UTF_32,
  LAPIZ_ENCODING_UCS_2,
  LAPIZ_ENCODING_UCS_4,

  LAPIZ_ENCODING_ARMSCII_8,
  LAPIZ_ENCODING_BIG5,
  LAPIZ_ENCODING_BIG5_HKSCS,
  LAPIZ_ENCODING_CP_866,

  LAPIZ_ENCODING_EUC_JP,
  LAPIZ_ENCODING_EUC_JP_MS,
  LAPIZ_ENCODING_CP932,
  LAPIZ_ENCODING_EUC_KR,
  LAPIZ_ENCODING_EUC_TW,

  LAPIZ_ENCODING_GB18030,
  LAPIZ_ENCODING_GB2312,
  LAPIZ_ENCODING_GBK,
  LAPIZ_ENCODING_GEOSTD8,

  LAPIZ_ENCODING_IBM_850,
  LAPIZ_ENCODING_IBM_852,
  LAPIZ_ENCODING_IBM_855,
  LAPIZ_ENCODING_IBM_857,
  LAPIZ_ENCODING_IBM_862,
  LAPIZ_ENCODING_IBM_864,

  LAPIZ_ENCODING_ISO_2022_JP,
  LAPIZ_ENCODING_ISO_2022_KR,
  LAPIZ_ENCODING_ISO_IR_111,
  LAPIZ_ENCODING_JOHAB,
  LAPIZ_ENCODING_KOI8_R,
  LAPIZ_ENCODING_KOI8__R,
  LAPIZ_ENCODING_KOI8_U,

  LAPIZ_ENCODING_SHIFT_JIS,
  LAPIZ_ENCODING_TCVN,
  LAPIZ_ENCODING_TIS_620,
  LAPIZ_ENCODING_UHC,
  LAPIZ_ENCODING_VISCII,

  LAPIZ_ENCODING_WINDOWS_1250,
  LAPIZ_ENCODING_WINDOWS_1251,
  LAPIZ_ENCODING_WINDOWS_1252,
  LAPIZ_ENCODING_WINDOWS_1253,
  LAPIZ_ENCODING_WINDOWS_1254,
  LAPIZ_ENCODING_WINDOWS_1255,
  LAPIZ_ENCODING_WINDOWS_1256,
  LAPIZ_ENCODING_WINDOWS_1257,
  LAPIZ_ENCODING_WINDOWS_1258,

  LAPIZ_ENCODING_LAST,

  LAPIZ_ENCODING_UTF_8,
  LAPIZ_ENCODING_UNKNOWN

} LapizEncodingIndex;

static const LapizEncoding utf8_encoding =  {
	LAPIZ_ENCODING_UTF_8,
	"UTF-8",
	N_("Unicode")
};

/* initialized in lapiz_encoding_lazy_init() */
static LapizEncoding unknown_encoding = {
	LAPIZ_ENCODING_UNKNOWN,
	NULL,
	NULL
};

static const LapizEncoding encodings [] = {

  { LAPIZ_ENCODING_ISO_8859_1,
    "ISO-8859-1", N_("Western") },
  { LAPIZ_ENCODING_ISO_8859_2,
   "ISO-8859-2", N_("Central European") },
  { LAPIZ_ENCODING_ISO_8859_3,
    "ISO-8859-3", N_("South European") },
  { LAPIZ_ENCODING_ISO_8859_4,
    "ISO-8859-4", N_("Baltic") },
  { LAPIZ_ENCODING_ISO_8859_5,
    "ISO-8859-5", N_("Cyrillic") },
  { LAPIZ_ENCODING_ISO_8859_6,
    "ISO-8859-6", N_("Arabic") },
  { LAPIZ_ENCODING_ISO_8859_7,
    "ISO-8859-7", N_("Greek") },
  { LAPIZ_ENCODING_ISO_8859_8,
    "ISO-8859-8", N_("Hebrew Visual") },
  { LAPIZ_ENCODING_ISO_8859_9,
    "ISO-8859-9", N_("Turkish") },
  { LAPIZ_ENCODING_ISO_8859_10,
    "ISO-8859-10", N_("Nordic") },
  { LAPIZ_ENCODING_ISO_8859_13,
    "ISO-8859-13", N_("Baltic") },
  { LAPIZ_ENCODING_ISO_8859_14,
    "ISO-8859-14", N_("Celtic") },
  { LAPIZ_ENCODING_ISO_8859_15,
    "ISO-8859-15", N_("Western") },
  { LAPIZ_ENCODING_ISO_8859_16,
    "ISO-8859-16", N_("Romanian") },

  { LAPIZ_ENCODING_UTF_7,
    "UTF-7", N_("Unicode") },
  { LAPIZ_ENCODING_UTF_16,
    "UTF-16", N_("Unicode") },
  { LAPIZ_ENCODING_UTF_16_BE,
    "UTF-16BE", N_("Unicode") },
  { LAPIZ_ENCODING_UTF_16_LE,
    "UTF-16LE", N_("Unicode") },
  { LAPIZ_ENCODING_UTF_32,
    "UTF-32", N_("Unicode") },
  { LAPIZ_ENCODING_UCS_2,
    "UCS-2", N_("Unicode") },
  { LAPIZ_ENCODING_UCS_4,
    "UCS-4", N_("Unicode") },

  { LAPIZ_ENCODING_ARMSCII_8,
    "ARMSCII-8", N_("Armenian") },
  { LAPIZ_ENCODING_BIG5,
    "BIG5", N_("Chinese Traditional") },
  { LAPIZ_ENCODING_BIG5_HKSCS,
    "BIG5-HKSCS", N_("Chinese Traditional") },
  { LAPIZ_ENCODING_CP_866,
    "CP866", N_("Cyrillic/Russian") },

  { LAPIZ_ENCODING_EUC_JP,
    "EUC-JP", N_("Japanese") },
  { LAPIZ_ENCODING_EUC_JP_MS,
    "EUC-JP-MS", N_("Japanese") },
  { LAPIZ_ENCODING_CP932,
    "CP932", N_("Japanese") },

  { LAPIZ_ENCODING_EUC_KR,
    "EUC-KR", N_("Korean") },
  { LAPIZ_ENCODING_EUC_TW,
    "EUC-TW", N_("Chinese Traditional") },

  { LAPIZ_ENCODING_GB18030,
    "GB18030", N_("Chinese Simplified") },
  { LAPIZ_ENCODING_GB2312,
    "GB2312", N_("Chinese Simplified") },
  { LAPIZ_ENCODING_GBK,
    "GBK", N_("Chinese Simplified") },
  { LAPIZ_ENCODING_GEOSTD8,
    "GEORGIAN-ACADEMY", N_("Georgian") }, /* FIXME GEOSTD8 ? */

  { LAPIZ_ENCODING_IBM_850,
    "IBM850", N_("Western") },
  { LAPIZ_ENCODING_IBM_852,
    "IBM852", N_("Central European") },
  { LAPIZ_ENCODING_IBM_855,
    "IBM855", N_("Cyrillic") },
  { LAPIZ_ENCODING_IBM_857,
    "IBM857", N_("Turkish") },
  { LAPIZ_ENCODING_IBM_862,
    "IBM862", N_("Hebrew") },
  { LAPIZ_ENCODING_IBM_864,
    "IBM864", N_("Arabic") },

  { LAPIZ_ENCODING_ISO_2022_JP,
    "ISO-2022-JP", N_("Japanese") },
  { LAPIZ_ENCODING_ISO_2022_KR,
    "ISO-2022-KR", N_("Korean") },
  { LAPIZ_ENCODING_ISO_IR_111,
    "ISO-IR-111", N_("Cyrillic") },
  { LAPIZ_ENCODING_JOHAB,
    "JOHAB", N_("Korean") },
  { LAPIZ_ENCODING_KOI8_R,
    "KOI8R", N_("Cyrillic") },
  { LAPIZ_ENCODING_KOI8__R,
    "KOI8-R", N_("Cyrillic") },
  { LAPIZ_ENCODING_KOI8_U,
    "KOI8U", N_("Cyrillic/Ukrainian") },

  { LAPIZ_ENCODING_SHIFT_JIS,
    "SHIFT_JIS", N_("Japanese") },
  { LAPIZ_ENCODING_TCVN,
    "TCVN", N_("Vietnamese") },
  { LAPIZ_ENCODING_TIS_620,
    "TIS-620", N_("Thai") },
  { LAPIZ_ENCODING_UHC,
    "UHC", N_("Korean") },
  { LAPIZ_ENCODING_VISCII,
    "VISCII", N_("Vietnamese") },

  { LAPIZ_ENCODING_WINDOWS_1250,
    "WINDOWS-1250", N_("Central European") },
  { LAPIZ_ENCODING_WINDOWS_1251,
    "WINDOWS-1251", N_("Cyrillic") },
  { LAPIZ_ENCODING_WINDOWS_1252,
    "WINDOWS-1252", N_("Western") },
  { LAPIZ_ENCODING_WINDOWS_1253,
    "WINDOWS-1253", N_("Greek") },
  { LAPIZ_ENCODING_WINDOWS_1254,
    "WINDOWS-1254", N_("Turkish") },
  { LAPIZ_ENCODING_WINDOWS_1255,
    "WINDOWS-1255", N_("Hebrew") },
  { LAPIZ_ENCODING_WINDOWS_1256,
    "WINDOWS-1256", N_("Arabic") },
  { LAPIZ_ENCODING_WINDOWS_1257,
    "WINDOWS-1257", N_("Baltic") },
  { LAPIZ_ENCODING_WINDOWS_1258,
    "WINDOWS-1258", N_("Vietnamese") }
};

static void
lapiz_encoding_lazy_init (void)
{
	static gboolean initialized = FALSE;
	const gchar *locale_charset;

	if (initialized)
		return;

	if (g_get_charset (&locale_charset) == FALSE)
	{
		unknown_encoding.charset = g_strdup (locale_charset);
	}

	initialized = TRUE;
}

const LapizEncoding *
lapiz_encoding_get_from_charset (const gchar *charset)
{
	gint i;

	g_return_val_if_fail (charset != NULL, NULL);

	lapiz_encoding_lazy_init ();

	if (charset == NULL)
		return NULL;

	if (g_ascii_strcasecmp (charset, "UTF-8") == 0)
		return lapiz_encoding_get_utf8 ();

	i = 0;
	while (i < LAPIZ_ENCODING_LAST)
	{
		if (g_ascii_strcasecmp (charset, encodings[i].charset) == 0)
			return &encodings[i];

		++i;
	}

	if (unknown_encoding.charset != NULL)
	{
		if (g_ascii_strcasecmp (charset, unknown_encoding.charset) == 0)
			return &unknown_encoding;
	}

	return NULL;
}

const LapizEncoding *
lapiz_encoding_get_from_index (gint idx)
{
	g_return_val_if_fail (idx >= 0, NULL);

	if (idx >= LAPIZ_ENCODING_LAST)
		return NULL;

	lapiz_encoding_lazy_init ();

	return &encodings[idx];
}

const LapizEncoding *
lapiz_encoding_get_utf8 (void)
{
	lapiz_encoding_lazy_init ();

	return &utf8_encoding;
}

const LapizEncoding *
lapiz_encoding_get_current (void)
{
	static gboolean initialized = FALSE;
	static const LapizEncoding *locale_encoding = NULL;

	const gchar *locale_charset;

	lapiz_encoding_lazy_init ();

	if (initialized != FALSE)
		return locale_encoding;

	if (g_get_charset (&locale_charset) == FALSE)
	{
		g_return_val_if_fail (locale_charset != NULL, &utf8_encoding);

		locale_encoding = lapiz_encoding_get_from_charset (locale_charset);
	}
	else
	{
		locale_encoding = &utf8_encoding;
	}

	if (locale_encoding == NULL)
	{
		locale_encoding = &unknown_encoding;
	}

	g_return_val_if_fail (locale_encoding != NULL, NULL);

	initialized = TRUE;

	return locale_encoding;
}

gchar *
lapiz_encoding_to_string (const LapizEncoding* enc)
{
	g_return_val_if_fail (enc != NULL, NULL);

	lapiz_encoding_lazy_init ();

	g_return_val_if_fail (enc->charset != NULL, NULL);

	if (enc->name != NULL)
	{
	    	return g_strdup_printf ("%s (%s)", _(enc->name), enc->charset);
	}
	else
	{
		if (g_ascii_strcasecmp (enc->charset, "ANSI_X3.4-1968") == 0)
			return g_strdup_printf ("US-ASCII (%s)", enc->charset);
		else
			return g_strdup (enc->charset);
	}
}

const gchar *
lapiz_encoding_get_charset (const LapizEncoding* enc)
{
	g_return_val_if_fail (enc != NULL, NULL);

	lapiz_encoding_lazy_init ();

	g_return_val_if_fail (enc->charset != NULL, NULL);

	return enc->charset;
}

const gchar *
lapiz_encoding_get_name (const LapizEncoding* enc)
{
	g_return_val_if_fail (enc != NULL, NULL);

	lapiz_encoding_lazy_init ();

	return (enc->name == NULL) ? _("Unknown") : _(enc->name);
}

/* These are to make language bindings happy. Since Encodings are
 * const, copy() just returns the same pointer and fres() doesn't
 * do nothing */

LapizEncoding *
lapiz_encoding_copy (const LapizEncoding *enc)
{
	g_return_val_if_fail (enc != NULL, NULL);

	return (LapizEncoding *) enc;
}

void
lapiz_encoding_free (LapizEncoding *enc)
{
	g_return_if_fail (enc != NULL);
}

/**
 * lapiz_encoding_get_type:
 *
 * Retrieves the GType object which is associated with the
 * #LapizEncoding class.
 *
 * Return value: the GType associated with #LapizEncoding.
 **/
GType
lapiz_encoding_get_type (void)
{
	static GType our_type = 0;

	if (!our_type)
		our_type = g_boxed_type_register_static (
			"LapizEncoding",
			(GBoxedCopyFunc) lapiz_encoding_copy,
			(GBoxedFreeFunc) lapiz_encoding_free);

	return our_type;
}

