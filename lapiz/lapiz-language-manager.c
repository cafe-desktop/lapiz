/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-languages-manager.c
 * This file is part of lapiz
 *
 * Copyright (C) 2003-2006 - Paolo Maggi
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
 * Modified by the lapiz Team, 2003-2006. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#include <string.h>
#include <ctk/ctk.h>
#include "lapiz-language-manager.h"
#include "lapiz-prefs-manager.h"
#include "lapiz-utils.h"
#include "lapiz-debug.h"

static GtkSourceLanguageManager *language_manager = NULL;

GtkSourceLanguageManager *
lapiz_get_language_manager (void)
{
	if (language_manager == NULL)
	{
		language_manager = ctk_source_language_manager_new ();
	}

	return language_manager;
}

static gint
language_compare (gconstpointer a, gconstpointer b)
{
	GtkSourceLanguage *lang_a = (GtkSourceLanguage *)a;
	GtkSourceLanguage *lang_b = (GtkSourceLanguage *)b;
	const gchar *name_a = ctk_source_language_get_name (lang_a);
	const gchar *name_b = ctk_source_language_get_name (lang_b);

	return g_utf8_collate (name_a, name_b);
}

GSList *
lapiz_language_manager_list_languages_sorted (GtkSourceLanguageManager *lm,
					      gboolean                  include_hidden)
{
	GSList *languages = NULL;
	const gchar * const *ids;

	ids = ctk_source_language_manager_get_language_ids (lm);
	if (ids == NULL)
		return NULL;

	while (*ids != NULL)
	{
		GtkSourceLanguage *lang;

		lang = ctk_source_language_manager_get_language (lm, *ids);
		g_return_val_if_fail (CTK_SOURCE_IS_LANGUAGE (lang), NULL);
		++ids;

		if (include_hidden || !ctk_source_language_get_hidden (lang))
		{
			languages = g_slist_prepend (languages, lang);
		}
	}

	return g_slist_sort (languages, (GCompareFunc)language_compare);
}

