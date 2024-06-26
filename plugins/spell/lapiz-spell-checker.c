/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-spell-checker.c
 * This file is part of lapiz
 *
 * Copyright (C) 2002-2006 Paolo Maggi
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
 * Modified by the lapiz Team, 2002-2006. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <enchant.h>

#include <glib/gi18n.h>
#include <glib.h>

#include "lapiz-spell-checker.h"
#include "lapiz-spell-utils.h"
#include "lapiz-spell-marshal.h"

struct _LapizSpellChecker
{
	GObject parent_instance;

	EnchantDict                     *dict;
	EnchantBroker                   *broker;
	const LapizSpellCheckerLanguage *active_lang;
};

/* GObject properties */
enum {
	PROP_0 = 0,
	PROP_LANGUAGE,
	LAST_PROP
};

/* Signals */
enum {
	ADD_WORD_TO_PERSONAL = 0,
	ADD_WORD_TO_SESSION,
	SET_LANGUAGE,
	CLEAR_SESSION,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(LapizSpellChecker, lapiz_spell_checker, G_TYPE_OBJECT)

static void
lapiz_spell_checker_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value G_GNUC_UNUSED,
				  GParamSpec   *pspec)
{
	/*
	LapizSpellChecker *spell = LAPIZ_SPELL_CHECKER (object);
	*/

	switch (prop_id)
	{
		case PROP_LANGUAGE:
			/* TODO */
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
lapiz_spell_checker_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value G_GNUC_UNUSED,
				  GParamSpec *pspec)
{
	/*
	LapizSpellChecker *spell = LAPIZ_SPELL_CHECKER (object);
	*/

	switch (prop_id)
	{
		case PROP_LANGUAGE:
			/* TODO */
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
lapiz_spell_checker_finalize (GObject *object)
{
	LapizSpellChecker *spell_checker;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER (object));

	spell_checker = LAPIZ_SPELL_CHECKER (object);

	if (spell_checker->dict != NULL)
		enchant_broker_free_dict (spell_checker->broker, spell_checker->dict);

	if (spell_checker->broker != NULL)
		enchant_broker_free (spell_checker->broker);

	G_OBJECT_CLASS (lapiz_spell_checker_parent_class)->finalize (object);
}

static void
lapiz_spell_checker_class_init (LapizSpellCheckerClass * klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = lapiz_spell_checker_set_property;
	object_class->get_property = lapiz_spell_checker_get_property;

	object_class->finalize = lapiz_spell_checker_finalize;

	g_object_class_install_property (object_class,
					 PROP_LANGUAGE,
					 g_param_spec_pointer ("language",
						 	      "Language",
							      "The language used by the spell checker",
							      G_PARAM_READWRITE));

	signals[ADD_WORD_TO_PERSONAL] =
	    g_signal_new ("add_word_to_personal",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizSpellCheckerClass, add_word_to_personal),
			  NULL, NULL,
			  lapiz_marshal_VOID__STRING_INT,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_STRING,
			  G_TYPE_INT);

	signals[ADD_WORD_TO_SESSION] =
	    g_signal_new ("add_word_to_session",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizSpellCheckerClass, add_word_to_session),
			  NULL, NULL,
			  lapiz_marshal_VOID__STRING_INT,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_STRING,
			  G_TYPE_INT);

	signals[SET_LANGUAGE] =
	    g_signal_new ("set_language",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizSpellCheckerClass, set_language),
			  NULL, NULL,
			  lapiz_marshal_VOID__POINTER,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_POINTER);

	signals[CLEAR_SESSION] =
	    g_signal_new ("clear_session",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizSpellCheckerClass, clear_session),
			  NULL, NULL,
			  lapiz_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
}

static void
lapiz_spell_checker_init (LapizSpellChecker *spell_checker)
{
	spell_checker->broker = enchant_broker_init ();
	spell_checker->dict = NULL;
	spell_checker->active_lang = NULL;
}

LapizSpellChecker *
lapiz_spell_checker_new	(void)
{
	LapizSpellChecker *spell;

	spell = LAPIZ_SPELL_CHECKER (
			g_object_new (LAPIZ_TYPE_SPELL_CHECKER, NULL));

	g_return_val_if_fail (spell != NULL, NULL);

	return spell;
}

static gboolean
lazy_init (LapizSpellChecker               *spell,
	   const LapizSpellCheckerLanguage *language)
{
	if (spell->dict != NULL)
		return TRUE;

	g_return_val_if_fail (spell->broker != NULL, FALSE);

	spell->active_lang = NULL;

	if (language != NULL)
	{
		spell->active_lang = language;
	}
	else
	{
		/* First try to get a default language */
		const LapizSpellCheckerLanguage *l;
		gint i = 0;
		const gchar * const *lang_tags = g_get_language_names ();

		while (lang_tags [i])
		{
			l = lapiz_spell_checker_language_from_key (lang_tags [i]);

			if (l != NULL)
			{
				spell->active_lang = l;
				break;
			}

			i++;
		}
	}

	/* Second try to get a default language */
	if (spell->active_lang == NULL)
		spell->active_lang = lapiz_spell_checker_language_from_key ("en_US");

	/* Last try to get a default language */
	if (spell->active_lang == NULL)
	{
		const GSList *langs;
		langs = lapiz_spell_checker_get_available_languages ();
		if (langs != NULL)
			spell->active_lang = (const LapizSpellCheckerLanguage *)langs->data;
	}

	if (spell->active_lang != NULL)
	{
		const gchar *key;

		key = lapiz_spell_checker_language_to_key (spell->active_lang);

		spell->dict = enchant_broker_request_dict (spell->broker,
							   key);
	}

	if (spell->dict == NULL)
	{
		spell->active_lang = NULL;

		if (language != NULL)
			g_warning ("Spell checker plugin: cannot select a default language.");

		return FALSE;
	}

	return TRUE;
}

gboolean
lapiz_spell_checker_set_language (LapizSpellChecker               *spell,
				  const LapizSpellCheckerLanguage *language)
{
	gboolean ret;

	g_return_val_if_fail (LAPIZ_IS_SPELL_CHECKER (spell), FALSE);

	if (spell->dict != NULL)
	{
		enchant_broker_free_dict (spell->broker, spell->dict);
		spell->dict = NULL;
	}

	ret = lazy_init (spell, language);

	if (ret)
		g_signal_emit (G_OBJECT (spell), signals[SET_LANGUAGE], 0, language);
	else
		g_warning ("Spell checker plugin: cannot use language %s.",
		           lapiz_spell_checker_language_to_string (language));

	return ret;
}

const LapizSpellCheckerLanguage *
lapiz_spell_checker_get_language (LapizSpellChecker *spell)
{
	g_return_val_if_fail (LAPIZ_IS_SPELL_CHECKER (spell), NULL);

	if (!lazy_init (spell, spell->active_lang))
		return NULL;

	return spell->active_lang;
}

gboolean
lapiz_spell_checker_check_word (LapizSpellChecker *spell,
				const gchar       *word,
				gssize             len)
{
	gint enchant_result;
	gboolean res = FALSE;

	g_return_val_if_fail (LAPIZ_IS_SPELL_CHECKER (spell), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	if (!lazy_init (spell, spell->active_lang))
		return FALSE;

	if (len < 0)
		len = strlen (word);

	if (strcmp (word, "lapiz") == 0)
		return TRUE;

	if (lapiz_spell_utils_is_digit (word, len))
		return TRUE;

	g_return_val_if_fail (spell->dict != NULL, FALSE);
	enchant_result = enchant_dict_check (spell->dict, word, len);

	switch (enchant_result)
	{
		case -1:
			/* error */
			res = FALSE;

			g_warning ("Spell checker plugin: error checking word '%s' (%s).",
				   word, enchant_dict_get_error (spell->dict));

			break;
		case 1:
			/* it is not in the directory */
			res = FALSE;
			break;
		case 0:
			/* is is in the directory */
			res = TRUE;
			break;
		default:
			g_return_val_if_reached (FALSE);
	}

	return res;
}


/* return NULL on error or if no suggestions are found */
GSList *
lapiz_spell_checker_get_suggestions (LapizSpellChecker *spell,
				     const gchar       *word,
				     gssize             len)
{
	gchar **suggestions;
	size_t n_suggestions = 0;
	GSList *suggestions_list = NULL;
	gint i;

	g_return_val_if_fail (LAPIZ_IS_SPELL_CHECKER (spell), NULL);
	g_return_val_if_fail (word != NULL, NULL);

	if (!lazy_init (spell, spell->active_lang))
		return NULL;

	g_return_val_if_fail (spell->dict != NULL, NULL);

	if (len < 0)
		len = strlen (word);

	suggestions = enchant_dict_suggest (spell->dict, word, len, &n_suggestions);

	if (n_suggestions == 0)
		return NULL;

	g_return_val_if_fail (suggestions != NULL, NULL);

	for (i = 0; i < (gint)n_suggestions; i++)
	{
		suggestions_list = g_slist_prepend (suggestions_list,
						    suggestions[i]);
	}

	/* The single suggestions will be freed by the caller */
	g_free (suggestions);

	suggestions_list = g_slist_reverse (suggestions_list);

	return suggestions_list;
}

gboolean
lapiz_spell_checker_add_word_to_personal (LapizSpellChecker *spell,
					  const gchar       *word,
					  gssize             len)
{
	g_return_val_if_fail (LAPIZ_IS_SPELL_CHECKER (spell), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	if (!lazy_init (spell, spell->active_lang))
		return FALSE;

	g_return_val_if_fail (spell->dict != NULL, FALSE);

	if (len < 0)
		len = strlen (word);

	enchant_dict_add (spell->dict, word, len);


	g_signal_emit (G_OBJECT (spell), signals[ADD_WORD_TO_PERSONAL], 0, word, len);

	return TRUE;
}

gboolean
lapiz_spell_checker_add_word_to_session (LapizSpellChecker *spell,
					 const gchar       *word,
					 gssize             len)
{
	g_return_val_if_fail (LAPIZ_IS_SPELL_CHECKER (spell), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	if (!lazy_init (spell, spell->active_lang))
		return FALSE;

	g_return_val_if_fail (spell->dict != NULL, FALSE);

	if (len < 0)
		len = strlen (word);

	enchant_dict_add_to_session (spell->dict, word, len);

	g_signal_emit (G_OBJECT (spell), signals[ADD_WORD_TO_SESSION], 0, word, len);

	return TRUE;
}

gboolean
lapiz_spell_checker_clear_session (LapizSpellChecker *spell)
{
	g_return_val_if_fail (LAPIZ_IS_SPELL_CHECKER (spell), FALSE);

	/* free and re-request dictionary */
	if (spell->dict != NULL)
	{
		enchant_broker_free_dict (spell->broker, spell->dict);
		spell->dict = NULL;
	}

	if (!lazy_init (spell, spell->active_lang))
		return FALSE;

	g_signal_emit (G_OBJECT (spell), signals[CLEAR_SESSION], 0);

	return TRUE;
}

/*
 * Informs dictionary, that word 'word' will be replaced/corrected by word
 * 'replacement'
 */
gboolean
lapiz_spell_checker_set_correction (LapizSpellChecker *spell,
				    const gchar       *word,
				    gssize             w_len,
				    const gchar       *replacement,
				    gssize             r_len)
{
	g_return_val_if_fail (LAPIZ_IS_SPELL_CHECKER (spell), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);
	g_return_val_if_fail (replacement != NULL, FALSE);

	if (!lazy_init (spell, spell->active_lang))
		return FALSE;

	g_return_val_if_fail (spell->dict != NULL, FALSE);

	if (w_len < 0)
		w_len = strlen (word);

	if (r_len < 0)
		r_len = strlen (replacement);

	enchant_dict_store_replacement (spell->dict,
					word,
					w_len,
					replacement,
					r_len);

	return TRUE;
}

