/*
 * lapiz-spell-plugin.c
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "lapiz-spell-plugin.h"
#include "lapiz-spell-utils.h"

#include <string.h> /* For strlen */

#include <glib/gi18n.h>
#include <gmodule.h>
#include <libpeas/peas-activatable.h>
#include <libpeas-ctk/peas-ctk-configurable.h>

#include <lapiz/lapiz-window.h>
#include <lapiz/lapiz-debug.h>
#include <lapiz/lapiz-prefs-manager.h>
#include <lapiz/lapiz-statusbar.h>
#include <lapiz/lapiz-utils.h>

#include "lapiz-spell-checker.h"
#include "lapiz-spell-checker-dialog.h"
#include "lapiz-spell-language-dialog.h"
#include "lapiz-automatic-spell-checker.h"

#define LAPIZ_METADATA_ATTRIBUTE_SPELL_LANGUAGE "metadata::lapiz-spell-language"
#define LAPIZ_METADATA_ATTRIBUTE_SPELL_ENABLED  "metadata::lapiz-spell-enabled"

#define MENU_PATH "/MenuBar/ToolsMenu/ToolsOps_1"

/* GSettings keys */
#define SPELL_SCHEMA		"org.cafe.lapiz.plugins.spell"
#define AUTOCHECK_TYPE_KEY	"autocheck-type"

static void peas_activatable_iface_init (PeasActivatableInterface *iface);
static void peas_ctk_configurable_iface_init (PeasGtkConfigurableInterface *iface);

enum {
	PROP_0,
	PROP_OBJECT
};

struct _LapizSpellPluginPrivate
{
	GtkWidget *window;

	GtkActionGroup *action_group;
	guint           ui_id;
	guint           message_cid;
	gulong          tab_added_id;
	gulong          tab_removed_id;

	GSettings *settings;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizSpellPlugin,
                                lapiz_spell_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizSpellPlugin)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_TYPE_ACTIVATABLE,
                                                               peas_activatable_iface_init)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_GTK_TYPE_CONFIGURABLE,
                                                               peas_ctk_configurable_iface_init))

static void	spell_cb	(GtkAction *action, LapizSpellPlugin *plugin);
static void	set_language_cb	(GtkAction *action, LapizSpellPlugin *plugin);
static void	auto_spell_cb	(GtkAction *action, LapizSpellPlugin *plugin);

/* UI actions. */
static const GtkActionEntry action_entries[] =
{
	{ "CheckSpell",
	  "tools-check-spelling",
	  N_("_Check Spelling..."),
	  "<shift>F7",
	  N_("Check the current document for incorrect spelling"),
	  G_CALLBACK (spell_cb)
	},

	{ "ConfigSpell",
	  NULL,
	  N_("Set _Language..."),
	  NULL,
	  N_("Set the language of the current document"),
	  G_CALLBACK (set_language_cb)
	}
};

static const GtkToggleActionEntry toggle_action_entries[] =
{
	{ "AutoSpell",
	  NULL,
	  N_("_Autocheck Spelling"),
	  "<control>F7",
	  N_("Automatically spell-check the current document"),
	  G_CALLBACK (auto_spell_cb),
	  FALSE
	}
};

typedef struct _SpellConfigureDialog SpellConfigureDialog;

struct _SpellConfigureDialog
{
	GtkWidget *content;

	GtkWidget *never;
	GtkWidget *always;
	GtkWidget *document;

	GSettings *settings;
};

typedef enum
{
	AUTOCHECK_NEVER = 0,
	AUTOCHECK_DOCUMENT,
	AUTOCHECK_ALWAYS
} LapizSpellPluginAutocheckType;

typedef struct _CheckRange CheckRange;

struct _CheckRange
{
	GtkTextMark *start_mark;
	GtkTextMark *end_mark;

	gint mw_start; /* misspelled word start */
	gint mw_end;   /* end */

	GtkTextMark *current_mark;
};

static GQuark spell_checker_id = 0;
static GQuark check_range_id = 0;

static void
lapiz_spell_plugin_init (LapizSpellPlugin *plugin)
{
	lapiz_debug_message (DEBUG_PLUGINS, "LapizSpellPlugin initializing");

	plugin->priv = lapiz_spell_plugin_get_instance_private (plugin);

	plugin->priv->settings = g_settings_new (SPELL_SCHEMA);
}

static void
lapiz_spell_plugin_dispose (GObject *object)
{
	LapizSpellPlugin *plugin = LAPIZ_SPELL_PLUGIN (object);

	lapiz_debug_message (DEBUG_PLUGINS, "LapizSpellPlugin disposing");

	if (plugin->priv->window != NULL)
	{
		g_object_unref (plugin->priv->window);
		plugin->priv->window = NULL;
	}

	if (plugin->priv->action_group)
	{
		g_object_unref (plugin->priv->action_group);
		plugin->priv->action_group = NULL;
	}

	g_object_unref (G_OBJECT (plugin->priv->settings));

	G_OBJECT_CLASS (lapiz_spell_plugin_parent_class)->dispose (object);
}

static void
set_spell_language_cb (LapizSpellChecker   *spell,
		       const LapizSpellCheckerLanguage *lang,
		       LapizDocument 	   *doc)
{
	const gchar *key;

	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));
	g_return_if_fail (lang != NULL);

	key = lapiz_spell_checker_language_to_key (lang);
	g_return_if_fail (key != NULL);

	lapiz_document_set_metadata (doc, LAPIZ_METADATA_ATTRIBUTE_SPELL_LANGUAGE,
				     key, NULL);
}

static void
set_language_from_metadata (LapizSpellChecker *spell,
			    LapizDocument     *doc)
{
	const LapizSpellCheckerLanguage *lang = NULL;
	gchar *value = NULL;

	value = lapiz_document_get_metadata (doc, LAPIZ_METADATA_ATTRIBUTE_SPELL_LANGUAGE);

	if (value != NULL)
	{
		lang = lapiz_spell_checker_language_from_key (value);
		g_free (value);
	}

	if (lang != NULL)
	{
		g_signal_handlers_block_by_func (spell, set_spell_language_cb, doc);
		lapiz_spell_checker_set_language (spell, lang);
		g_signal_handlers_unblock_by_func (spell, set_spell_language_cb, doc);
	}
}

static LapizSpellPluginAutocheckType
get_autocheck_type (LapizSpellPlugin *plugin)
{
	LapizSpellPluginAutocheckType autocheck_type;

	autocheck_type = g_settings_get_enum (plugin->priv->settings,
					      AUTOCHECK_TYPE_KEY);

	return autocheck_type;
}

static void
set_autocheck_type (GSettings *settings,
		    LapizSpellPluginAutocheckType autocheck_type)
{
	if (!g_settings_is_writable (settings,
				     AUTOCHECK_TYPE_KEY))
	{
		return;
	}

	g_settings_set_enum (settings,
			     AUTOCHECK_TYPE_KEY,
			     autocheck_type);
}

static LapizSpellChecker *
get_spell_checker_from_document (LapizDocument *doc)
{
	LapizSpellChecker *spell;
	gpointer data;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (doc != NULL, NULL);

	data = g_object_get_qdata (G_OBJECT (doc), spell_checker_id);

	if (data == NULL)
	{
		spell = lapiz_spell_checker_new ();

		set_language_from_metadata (spell, doc);

		g_object_set_qdata_full (G_OBJECT (doc),
					 spell_checker_id,
					 spell,
					 (GDestroyNotify) g_object_unref);

		g_signal_connect (spell,
				  "set_language",
				  G_CALLBACK (set_spell_language_cb),
				  doc);
	}
	else
	{
		g_return_val_if_fail (LAPIZ_IS_SPELL_CHECKER (data), NULL);
		spell = LAPIZ_SPELL_CHECKER (data);
	}

	return spell;
}

static CheckRange *
get_check_range (LapizDocument *doc)
{
	CheckRange *range;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (doc != NULL, NULL);

	range = (CheckRange *) g_object_get_qdata (G_OBJECT (doc), check_range_id);

	return range;
}

static void
update_current (LapizDocument *doc,
		gint           current)
{
	CheckRange *range;
	GtkTextIter iter;
	GtkTextIter end_iter;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_if_fail (doc != NULL);
	g_return_if_fail (current >= 0);

	range = get_check_range (doc);
	g_return_if_fail (range != NULL);

	ctk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc),
					    &iter, current);

	if (!ctk_text_iter_inside_word (&iter))
	{
		/* if we're not inside a word,
		 * we must be in some spaces.
		 * skip forward to the beginning of the next word. */
		if (!ctk_text_iter_is_end (&iter))
		{
			ctk_text_iter_forward_word_end (&iter);
			ctk_text_iter_backward_word_start (&iter);
		}
	}
	else
	{
		if (!ctk_text_iter_starts_word (&iter))
			ctk_text_iter_backward_word_start (&iter);
	}

	ctk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
					  &end_iter,
					  range->end_mark);

	if (ctk_text_iter_compare (&end_iter, &iter) < 0)
	{
		ctk_text_buffer_move_mark (GTK_TEXT_BUFFER (doc),
					   range->current_mark,
					   &end_iter);
	}
	else
	{
		ctk_text_buffer_move_mark (GTK_TEXT_BUFFER (doc),
					   range->current_mark,
					   &iter);
	}
}

static void
set_check_range (LapizDocument *doc,
		 GtkTextIter   *start,
		 GtkTextIter   *end)
{
	CheckRange *range;
	GtkTextIter iter;

	lapiz_debug (DEBUG_PLUGINS);

	range = get_check_range (doc);

	if (range == NULL)
	{
		lapiz_debug_message (DEBUG_PLUGINS, "There was not a previous check range");

		ctk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &iter);

		range = g_new0 (CheckRange, 1);

		range->start_mark = ctk_text_buffer_create_mark (GTK_TEXT_BUFFER (doc),
				"check_range_start_mark", &iter, TRUE);

		range->end_mark = ctk_text_buffer_create_mark (GTK_TEXT_BUFFER (doc),
				"check_range_end_mark", &iter, FALSE);

		range->current_mark = ctk_text_buffer_create_mark (GTK_TEXT_BUFFER (doc),
				"check_range_current_mark", &iter, TRUE);

		g_object_set_qdata_full (G_OBJECT (doc),
				 check_range_id,
				 range,
				 (GDestroyNotify)g_free);
	}

	if (lapiz_spell_utils_skip_no_spell_check (start, end))
	 {
		if (!ctk_text_iter_inside_word (end))
		{
			/* if we're neither inside a word,
			 * we must be in some spaces.
			 * skip backward to the end of the previous word. */
			if (!ctk_text_iter_is_end (end))
			{
				ctk_text_iter_backward_word_start (end);
				ctk_text_iter_forward_word_end (end);
			}
		}
		else
		{
			if (!ctk_text_iter_ends_word (end))
				ctk_text_iter_forward_word_end (end);
		}
	}
	else
	{
		/* no spell checking in the specified range */
		start = end;
	}

	ctk_text_buffer_move_mark (GTK_TEXT_BUFFER (doc),
				   range->start_mark,
				   start);
	ctk_text_buffer_move_mark (GTK_TEXT_BUFFER (doc),
				   range->end_mark,
				   end);

	range->mw_start = -1;
	range->mw_end = -1;

	update_current (doc, ctk_text_iter_get_offset (start));
}

static gchar *
get_current_word (LapizDocument *doc, gint *start, gint *end)
{
	const CheckRange *range;
	GtkTextIter end_iter;
	GtkTextIter current_iter;
	gint range_end;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (doc != NULL, NULL);
	g_return_val_if_fail (start != NULL, NULL);
	g_return_val_if_fail (end != NULL, NULL);

	range = get_check_range (doc);
	g_return_val_if_fail (range != NULL, NULL);

	ctk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
			&end_iter, range->end_mark);

	range_end = ctk_text_iter_get_offset (&end_iter);

	ctk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
			&current_iter, range->current_mark);

	end_iter = current_iter;

	if (!ctk_text_iter_is_end (&end_iter))
	{
		lapiz_debug_message (DEBUG_PLUGINS, "Current is not end");

		ctk_text_iter_forward_word_end (&end_iter);
	}

	*start = ctk_text_iter_get_offset (&current_iter);
	*end = MIN (ctk_text_iter_get_offset (&end_iter), range_end);

	lapiz_debug_message (DEBUG_PLUGINS, "Current word extends [%d, %d]", *start, *end);

	if (!(*start < *end))
		return NULL;

	return ctk_text_buffer_get_slice (GTK_TEXT_BUFFER (doc),
					  &current_iter,
					  &end_iter,
					  TRUE);
}

static gboolean
goto_next_word (LapizDocument *doc)
{
	CheckRange *range;
	GtkTextIter current_iter;
	GtkTextIter old_current_iter;
	GtkTextIter end_iter;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (doc != NULL, FALSE);

	range = get_check_range (doc);
	g_return_val_if_fail (range != NULL, FALSE);

	ctk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
					  &current_iter,
					  range->current_mark);
	ctk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &end_iter);

	old_current_iter = current_iter;

	ctk_text_iter_forward_word_ends (&current_iter, 2);
	ctk_text_iter_backward_word_start (&current_iter);

	if (lapiz_spell_utils_skip_no_spell_check (&current_iter, &end_iter) &&
	    (ctk_text_iter_compare (&old_current_iter, &current_iter) < 0) &&
	    (ctk_text_iter_compare (&current_iter, &end_iter) < 0))
	{
		update_current (doc, ctk_text_iter_get_offset (&current_iter));
		return TRUE;
	}

	return FALSE;
}

static gchar *
get_next_misspelled_word (LapizView *view)
{
	LapizDocument *doc;
	CheckRange *range;
	gint start, end;
	gchar *word;
	LapizSpellChecker *spell;

	g_return_val_if_fail (view != NULL, NULL);

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	g_return_val_if_fail (doc != NULL, NULL);

	range = get_check_range (doc);
	g_return_val_if_fail (range != NULL, NULL);

	spell = get_spell_checker_from_document (doc);
	g_return_val_if_fail (spell != NULL, NULL);

	word = get_current_word (doc, &start, &end);
	if (word == NULL)
		return NULL;

	lapiz_debug_message (DEBUG_PLUGINS, "Word to check: %s", word);

	while (lapiz_spell_checker_check_word (spell, word, -1))
	{
		g_free (word);

		if (!goto_next_word (doc))
			return NULL;

		/* may return null if we reached the end of the selection */
		word = get_current_word (doc, &start, &end);
		if (word == NULL)
			return NULL;

		lapiz_debug_message (DEBUG_PLUGINS, "Word to check: %s", word);
	}

	if (!goto_next_word (doc))
		update_current (doc, ctk_text_buffer_get_char_count (GTK_TEXT_BUFFER (doc)));

	if (word != NULL)
	{
		GtkTextIter s, e;

		range->mw_start = start;
		range->mw_end = end;

		lapiz_debug_message (DEBUG_PLUGINS, "Select [%d, %d]", start, end);

		ctk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &s, start);
		ctk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &e, end);

		ctk_text_buffer_select_range (GTK_TEXT_BUFFER (doc), &s, &e);

		lapiz_view_scroll_to_cursor (view);
	}
	else
	{
		range->mw_start = -1;
		range->mw_end = -1;
	}

	return word;
}

static void
ignore_cb (LapizSpellCheckerDialog *dlg,
	   const gchar             *w,
	   LapizView               *view)
{
	gchar *word = NULL;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_if_fail (w != NULL);
	g_return_if_fail (view != NULL);

	word = get_next_misspelled_word (view);
	if (word == NULL)
	{
		lapiz_spell_checker_dialog_set_completed (dlg);

		return;
	}

	lapiz_spell_checker_dialog_set_misspelled_word (LAPIZ_SPELL_CHECKER_DIALOG (dlg),
							word,
							-1);

	g_free (word);
}

static void
change_cb (LapizSpellCheckerDialog *dlg,
	   const gchar             *word,
	   const gchar             *change,
	   LapizView               *view)
{
	LapizDocument *doc;
	CheckRange *range;
	gchar *w = NULL;
	GtkTextIter start, end;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_if_fail (view != NULL);
	g_return_if_fail (word != NULL);
	g_return_if_fail (change != NULL);

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	g_return_if_fail (doc != NULL);

	range = get_check_range (doc);
	g_return_if_fail (range != NULL);

	ctk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &start, range->mw_start);
	if (range->mw_end < 0)
		ctk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &end);
	else
		ctk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &end, range->mw_end);

	w = ctk_text_buffer_get_slice (GTK_TEXT_BUFFER (doc), &start, &end, TRUE);
	g_return_if_fail (w != NULL);

	if (strcmp (w, word) != 0)
	{
		g_free (w);
		return;
	}

	g_free (w);

	ctk_text_buffer_begin_user_action (GTK_TEXT_BUFFER(doc));

	ctk_text_buffer_delete (GTK_TEXT_BUFFER (doc), &start, &end);
	ctk_text_buffer_insert (GTK_TEXT_BUFFER (doc), &start, change, -1);

	ctk_text_buffer_end_user_action (GTK_TEXT_BUFFER(doc));

	update_current (doc, range->mw_start + g_utf8_strlen (change, -1));

	/* go to next misspelled word */
	ignore_cb (dlg, word, view);
}

static void
change_all_cb (LapizSpellCheckerDialog *dlg,
	       const gchar             *word,
	       const gchar             *change,
	       LapizView               *view)
{
	LapizDocument *doc;
	CheckRange *range;
	gchar *w = NULL;
	GtkTextIter start, end;
	gint flags = 0;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_if_fail (view != NULL);
	g_return_if_fail (word != NULL);
	g_return_if_fail (change != NULL);

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	g_return_if_fail (doc != NULL);

	range = get_check_range (doc);
	g_return_if_fail (range != NULL);

	ctk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &start, range->mw_start);
	if (range->mw_end < 0)
		ctk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &end);
	else
		ctk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &end, range->mw_end);

	w = ctk_text_buffer_get_slice (GTK_TEXT_BUFFER (doc), &start, &end, TRUE);
	g_return_if_fail (w != NULL);

	if (strcmp (w, word) != 0)
	{
		g_free (w);
		return;
	}

	g_free (w);

	LAPIZ_SEARCH_SET_CASE_SENSITIVE (flags, TRUE);
	LAPIZ_SEARCH_SET_ENTIRE_WORD (flags, TRUE);

	/* CHECK: currently this function does escaping etc */
	lapiz_document_replace_all (doc, word, change, flags);

	update_current (doc, range->mw_start + g_utf8_strlen (change, -1));

	/* go to next misspelled word */
	ignore_cb (dlg, word, view);
}

static void
add_word_cb (LapizSpellCheckerDialog *dlg,
	     const gchar             *word,
	     LapizView               *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (word != NULL);

	/* go to next misspelled word */
	ignore_cb (dlg, word, view);
}

static void
language_dialog_response (GtkDialog         *dlg,
			  gint               res_id,
			  LapizSpellChecker *spell)
{
	if (res_id == GTK_RESPONSE_OK)
	{
		const LapizSpellCheckerLanguage *lang;

		lang = lapiz_spell_language_get_selected_language (LAPIZ_SPELL_LANGUAGE_DIALOG (dlg));
		if (lang != NULL)
			lapiz_spell_checker_set_language (spell, lang);
	}

	ctk_widget_destroy (GTK_WIDGET (dlg));
}

static SpellConfigureDialog *
get_configure_dialog (LapizSpellPlugin *plugin)
{
	SpellConfigureDialog *dialog = NULL;
	gchar *data_dir;
	gchar *ui_file;
	LapizSpellPluginAutocheckType autocheck_type;
	GtkWidget *error_widget;
	gboolean ret;
	gchar *root_objects[] = {
		"spell_dialog_content",
		NULL
	};

	lapiz_debug (DEBUG_PLUGINS);

	dialog = g_slice_new (SpellConfigureDialog);
	dialog->settings = g_object_ref (plugin->priv->settings);

	data_dir = peas_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	ui_file = g_build_filename (data_dir, "lapiz-spell-setup-dialog.ui", NULL);
	ret = lapiz_utils_get_ui_objects (ui_file,
					  root_objects,
					  &error_widget,
					  "spell_dialog_content", &dialog->content,
					  "autocheck_never", &dialog->never,
					  "autocheck_document", &dialog->document,
					  "autocheck_always", &dialog->always,
					  NULL);

	g_free (data_dir);
	g_free (ui_file);

	if (!ret)
	{
		return NULL;
	}

	autocheck_type = get_autocheck_type (plugin);

	if (autocheck_type == AUTOCHECK_ALWAYS)
	{
		ctk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->always), TRUE);
	}
	else if (autocheck_type == AUTOCHECK_DOCUMENT)
	{
		ctk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->document), TRUE);
	}
	else
	{
		ctk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->never), TRUE);
	}

	return dialog;
}

static void
configure_dialog_button_toggled (GtkToggleButton      *button,
                                 SpellConfigureDialog *dialog)
{
	lapiz_debug (DEBUG_PLUGINS);

	if (ctk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->always)))
	{
		set_autocheck_type (dialog->settings, AUTOCHECK_ALWAYS);
	}
	else if (ctk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->document)))
	{
		set_autocheck_type (dialog->settings, AUTOCHECK_DOCUMENT);
	}
	else
	{
		set_autocheck_type (dialog->settings, AUTOCHECK_NEVER);
	}
}

static void
configure_dialog_destroyed (GtkWidget *widget,
                            gpointer   data)
{
	SpellConfigureDialog *dialog = (SpellConfigureDialog *) data;

	lapiz_debug (DEBUG_PLUGINS);

	g_object_unref (dialog->settings);
	g_slice_free (SpellConfigureDialog, data);
}

static void
set_language_cb (GtkAction   *action,
		 LapizSpellPlugin *plugin)
{
	LapizWindow *window;
	LapizDocument *doc;
	LapizSpellChecker *spell;
	const LapizSpellCheckerLanguage *lang;
	GtkWidget *dlg;
	GtkWindowGroup *wg;
	gchar *data_dir;

	lapiz_debug (DEBUG_PLUGINS);

	window = LAPIZ_WINDOW (plugin->priv->window);
	doc = lapiz_window_get_active_document (window);
	g_return_if_fail (doc != NULL);

	spell = get_spell_checker_from_document (doc);
	g_return_if_fail (spell != NULL);

	lang = lapiz_spell_checker_get_language (spell);

	data_dir = peas_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	dlg = lapiz_spell_language_dialog_new (GTK_WINDOW (window),
					       lang,
					       data_dir);
	g_free (data_dir);

	wg = lapiz_window_get_group (window);

	ctk_window_group_add_window (wg, GTK_WINDOW (dlg));

	ctk_window_set_modal (GTK_WINDOW (dlg), TRUE);

	g_signal_connect (dlg,
			  "response",
			  G_CALLBACK (language_dialog_response),
			  spell);

	ctk_widget_show (dlg);
}

static void
spell_cb (GtkAction   *action,
	  LapizSpellPlugin *plugin)
{
	LapizSpellPluginPrivate *data;
	LapizWindow *window;
	LapizView *view;
	LapizDocument *doc;
	LapizSpellChecker *spell;
	GtkWidget *dlg;
	GtkTextIter start, end;
	gchar *word;
	gchar *data_dir;

	lapiz_debug (DEBUG_PLUGINS);

	data = plugin->priv;
	window = LAPIZ_WINDOW (data->window);
	view = lapiz_window_get_active_view (window);
	g_return_if_fail (view != NULL);

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	g_return_if_fail (doc != NULL);

	spell = get_spell_checker_from_document (doc);
	g_return_if_fail (spell != NULL);

	if (ctk_text_buffer_get_char_count (GTK_TEXT_BUFFER (doc)) <= 0)
	{
		GtkWidget *statusbar;

		statusbar = lapiz_window_get_statusbar (window);
		lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (statusbar),
					       data->message_cid,
					       _("The document is empty."));

		return;
	}

	if (!ctk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (doc),
						   &start,
						   &end))
	{
		/* no selection, get the whole doc */
		ctk_text_buffer_get_bounds (GTK_TEXT_BUFFER (doc),
					    &start,
					    &end);
	}

	set_check_range (doc, &start, &end);

	word = get_next_misspelled_word (view);
	if (word == NULL)
	{
		GtkWidget *statusbar;

		statusbar = lapiz_window_get_statusbar (window);
		lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (statusbar),
					       data->message_cid,
					       _("No misspelled words"));

		return;
	}

	data_dir = peas_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	dlg = lapiz_spell_checker_dialog_new_from_spell_checker (spell, data_dir);
	g_free (data_dir);

	ctk_window_set_modal (GTK_WINDOW (dlg), TRUE);
	ctk_window_set_transient_for (GTK_WINDOW (dlg),
				      GTK_WINDOW (window));

	g_signal_connect (dlg, "ignore", G_CALLBACK (ignore_cb), view);
	g_signal_connect (dlg, "ignore_all", G_CALLBACK (ignore_cb), view);

	g_signal_connect (dlg, "change", G_CALLBACK (change_cb), view);
	g_signal_connect (dlg, "change_all", G_CALLBACK (change_all_cb), view);

	g_signal_connect (dlg, "add_word_to_personal", G_CALLBACK (add_word_cb), view);

	lapiz_spell_checker_dialog_set_misspelled_word (LAPIZ_SPELL_CHECKER_DIALOG (dlg),
							word,
							-1);

	g_free (word);

	ctk_widget_show (dlg);
}

static void
set_auto_spell (LapizWindow   *window,
		LapizDocument *doc,
		gboolean       active)
{
	LapizAutomaticSpellChecker *autospell;
	LapizSpellChecker *spell;

	spell = get_spell_checker_from_document (doc);
	g_return_if_fail (spell != NULL);

	autospell = lapiz_automatic_spell_checker_get_from_document (doc);

	if (active)
	{
		if (autospell == NULL)
		{
			LapizView *active_view;

			active_view = lapiz_window_get_active_view (window);
			g_return_if_fail (active_view != NULL);

			autospell = lapiz_automatic_spell_checker_new (doc, spell);

			if (doc == lapiz_window_get_active_document (window))
			{
				lapiz_automatic_spell_checker_attach_view (autospell, active_view);
			}

			lapiz_automatic_spell_checker_recheck_all (autospell);
		}
	}
	else
	{
		if (autospell != NULL)
			lapiz_automatic_spell_checker_free (autospell);
	}
}

static void
auto_spell_cb (GtkAction   *action,
	       LapizSpellPlugin *plugin)
{
	LapizWindow *window;
	LapizDocument *doc;
	gboolean active;

	lapiz_debug (DEBUG_PLUGINS);

	window = LAPIZ_WINDOW (plugin->priv->window);

	active = ctk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	lapiz_debug_message (DEBUG_PLUGINS, active ? "Auto Spell activated" : "Auto Spell deactivated");

	doc = lapiz_window_get_active_document (window);
	if (doc == NULL)
		return;

	if (get_autocheck_type (plugin) == AUTOCHECK_DOCUMENT)
	{
		lapiz_document_set_metadata (doc,
				     LAPIZ_METADATA_ATTRIBUTE_SPELL_ENABLED,
				     active ? "1" : NULL, NULL);
	}

	set_auto_spell (window, doc, active);
}

static void
update_ui (LapizSpellPlugin *plugin)
{
	LapizSpellPluginPrivate *data;
	LapizWindow *window;
	LapizDocument *doc;
	LapizView *view;
	gboolean autospell;
	GtkAction *action;

	lapiz_debug (DEBUG_PLUGINS);

	data = plugin->priv;
	window = LAPIZ_WINDOW (data->window);
	doc = lapiz_window_get_active_document (window);
	view = lapiz_window_get_active_view (window);

	autospell = (doc != NULL &&
	             lapiz_automatic_spell_checker_get_from_document (doc) != NULL);

	if (doc != NULL)
	{
		LapizTab *tab;
		LapizTabState state;

		tab = lapiz_window_get_active_tab (window);
		state = lapiz_tab_get_state (tab);

		/* If the document is loading we can't get the metadata so we
		   endup with an useless speller */
		if (state == LAPIZ_TAB_STATE_NORMAL)
		{
			action = ctk_action_group_get_action (data->action_group,
							      "AutoSpell");

			g_signal_handlers_block_by_func (action, auto_spell_cb,
							 plugin);
			set_auto_spell (window, doc, autospell);
			ctk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
						      autospell);
			g_signal_handlers_unblock_by_func (action, auto_spell_cb,
							   plugin);
		}
	}

	ctk_action_group_set_sensitive (data->action_group,
					(view != NULL) &&
					ctk_text_view_get_editable (GTK_TEXT_VIEW (view)));
}

static void
set_auto_spell_from_metadata (LapizSpellPlugin *plugin,
			      LapizDocument  *doc,
			      GtkActionGroup *action_group)
{
	gboolean active = FALSE;
	gchar *active_str = NULL;
	LapizWindow *window;
	LapizDocument *active_doc;
	LapizSpellPluginAutocheckType autocheck_type;

	autocheck_type = get_autocheck_type (plugin);

	switch (autocheck_type)
	{
		case AUTOCHECK_ALWAYS:
		{
			active = TRUE;
			break;
		}
		case AUTOCHECK_DOCUMENT:
		{
			active_str = lapiz_document_get_metadata (doc,
						  LAPIZ_METADATA_ATTRIBUTE_SPELL_ENABLED);
			break;
		}
		case AUTOCHECK_NEVER:
		default:
			active = FALSE;
			break;
	}

	if (active_str)
	{
		active = *active_str == '1';

		g_free (active_str);
	}

	window = LAPIZ_WINDOW (plugin->priv->window);

	set_auto_spell (window, doc, active);

	/* In case that the doc is the active one we mark the spell action */
	active_doc = lapiz_window_get_active_document (window);

	if (active_doc == doc && action_group != NULL)
	{
		GtkAction *action;

		action = ctk_action_group_get_action (action_group,
						      "AutoSpell");

		g_signal_handlers_block_by_func (action, auto_spell_cb,
						 plugin);
		ctk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      active);
		g_signal_handlers_unblock_by_func (action, auto_spell_cb,
						   plugin);
	}
}

static void
on_document_loaded (LapizDocument *doc,
		    const GError  *error,
		    LapizSpellPlugin *plugin)
{
	if (error == NULL)
	{
		LapizSpellChecker *spell;

		spell = LAPIZ_SPELL_CHECKER (g_object_get_qdata (G_OBJECT (doc),
								 spell_checker_id));
		if (spell != NULL)
		{
			set_language_from_metadata (spell, doc);
		}

		set_auto_spell_from_metadata (plugin, doc, plugin->priv->action_group);
	}
}

static void
on_document_saved (LapizDocument *doc,
		   const GError  *error,
		   LapizSpellPlugin *plugin)
{
	LapizAutomaticSpellChecker *autospell;
	LapizSpellChecker *spell;
	const gchar *key;

	if (error != NULL)
	{
		return;
	}

	/* Make sure to save the metadata here too */
	autospell = lapiz_automatic_spell_checker_get_from_document (doc);
	spell = LAPIZ_SPELL_CHECKER (g_object_get_qdata (G_OBJECT (doc), spell_checker_id));

	if (spell != NULL)
	{
		key = lapiz_spell_checker_language_to_key (lapiz_spell_checker_get_language (spell));
	}
	else
	{
		key = NULL;
	}

	if (get_autocheck_type (plugin) == AUTOCHECK_DOCUMENT)
	{

		lapiz_document_set_metadata (doc,
				LAPIZ_METADATA_ATTRIBUTE_SPELL_ENABLED,
				autospell != NULL ? "1" : NULL,
				LAPIZ_METADATA_ATTRIBUTE_SPELL_LANGUAGE,
				key,
				NULL);
	}
	else
	{
		lapiz_document_set_metadata (doc,
				LAPIZ_METADATA_ATTRIBUTE_SPELL_LANGUAGE,
				key,
				NULL);
	}
}

static void
tab_added_cb (LapizWindow *window,
	      LapizTab    *tab,
	      LapizSpellPlugin *plugin)
{
	LapizDocument *doc;
	gchar *uri;

	doc = lapiz_tab_get_document (tab);

	g_object_get(G_OBJECT(doc), "uri", &uri, NULL);

	if (!uri)
	{
		set_auto_spell_from_metadata (plugin, doc, plugin->priv->action_group);

		g_free(uri);
	}

	g_signal_connect (doc, "loaded",
			  G_CALLBACK (on_document_loaded),
			  plugin);

	g_signal_connect (doc, "saved",
			  G_CALLBACK (on_document_saved),
			  plugin);
}

static void
tab_removed_cb (LapizWindow *window,
		LapizTab    *tab,
		LapizSpellPlugin *plugin)
{
	LapizDocument *doc;

	doc = lapiz_tab_get_document (tab);

	g_signal_handlers_disconnect_by_func (doc, on_document_loaded, plugin);
	g_signal_handlers_disconnect_by_func (doc, on_document_saved, plugin);
}

static void
lapiz_spell_plugin_activate (PeasActivatable *activatable)
{
	LapizSpellPlugin *plugin;
	LapizSpellPluginPrivate *data;
	LapizWindow *window;
	GtkUIManager *manager;
	GList *docs, *l;

	lapiz_debug (DEBUG_PLUGINS);

	plugin = LAPIZ_SPELL_PLUGIN (activatable);
	data = plugin->priv;
	window = LAPIZ_WINDOW (data->window);

	manager = lapiz_window_get_ui_manager (window);

	data->action_group = ctk_action_group_new ("LapizSpellPluginActions");
	ctk_action_group_set_translation_domain (data->action_group,
						 GETTEXT_PACKAGE);
	ctk_action_group_add_actions (data->action_group,
					   action_entries,
					   G_N_ELEMENTS (action_entries),
					   plugin);
	ctk_action_group_add_toggle_actions (data->action_group,
					     toggle_action_entries,
					     G_N_ELEMENTS (toggle_action_entries),
					     plugin);

	ctk_ui_manager_insert_action_group (manager, data->action_group, -1);

	data->ui_id = ctk_ui_manager_new_merge_id (manager);

	data->message_cid = ctk_statusbar_get_context_id
			(GTK_STATUSBAR (lapiz_window_get_statusbar (window)),
			 "spell_plugin_message");

	ctk_ui_manager_add_ui (manager,
			       data->ui_id,
			       MENU_PATH,
			       "CheckSpell",
			       "CheckSpell",
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	ctk_ui_manager_add_ui (manager,
			       data->ui_id,
			       MENU_PATH,
			       "AutoSpell",
			       "AutoSpell",
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	ctk_ui_manager_add_ui (manager,
			       data->ui_id,
			       MENU_PATH,
			       "ConfigSpell",
			       "ConfigSpell",
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	update_ui (plugin);

	docs = lapiz_window_get_documents (window);
	for (l = docs; l != NULL; l = g_list_next (l))
	{
		LapizDocument *doc = LAPIZ_DOCUMENT (l->data);

		set_auto_spell_from_metadata (plugin, doc,
					      data->action_group);

		g_signal_handlers_disconnect_by_func (doc,
		                                      on_document_loaded,
		                                      plugin);

		g_signal_handlers_disconnect_by_func (doc,
		                                      on_document_saved,
		                                      plugin);
	}

	data->tab_added_id =
		g_signal_connect (window, "tab-added",
				  G_CALLBACK (tab_added_cb), plugin);
	data->tab_removed_id =
		g_signal_connect (window, "tab-removed",
				  G_CALLBACK (tab_removed_cb), plugin);
}

static void
lapiz_spell_plugin_deactivate (PeasActivatable *activatable)
{
	LapizSpellPluginPrivate *data;
	LapizWindow *window;
	GtkUIManager *manager;

	lapiz_debug (DEBUG_PLUGINS);

	data = LAPIZ_SPELL_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (data->window);

	manager = lapiz_window_get_ui_manager (window);

	ctk_ui_manager_remove_ui (manager, data->ui_id);
	ctk_ui_manager_remove_action_group (manager, data->action_group);

	g_signal_handler_disconnect (window, data->tab_added_id);
	g_signal_handler_disconnect (window, data->tab_removed_id);
}

static void
lapiz_spell_plugin_update_state (PeasActivatable *activatable)
{
	lapiz_debug (DEBUG_PLUGINS);

	update_ui (LAPIZ_SPELL_PLUGIN (activatable));
}

static void
lapiz_spell_plugin_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	LapizSpellPlugin *plugin = LAPIZ_SPELL_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_OBJECT:
			plugin->priv->window = GTK_WIDGET (g_value_dup_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_spell_plugin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
	LapizSpellPlugin *plugin = LAPIZ_SPELL_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_OBJECT:
			g_value_set_object (value, plugin->priv->window);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static GtkWidget *
lapiz_spell_plugin_create_configure_widget (PeasGtkConfigurable *configurable)
{
	SpellConfigureDialog *dialog;

	dialog = get_configure_dialog (LAPIZ_SPELL_PLUGIN (configurable));

	g_signal_connect (dialog->always,
	                  "toggled",
	                  G_CALLBACK (configure_dialog_button_toggled),
	                  dialog);
	g_signal_connect (dialog->document,
	                  "toggled",
	                  G_CALLBACK (configure_dialog_button_toggled),
	                  dialog);
	g_signal_connect (dialog->never,
	                  "toggled",
	                  G_CALLBACK (configure_dialog_button_toggled),
	                  dialog);

	g_signal_connect (dialog->content,
	                  "destroy",
	                  G_CALLBACK (configure_dialog_destroyed),
	                  dialog);

	return dialog->content;
}

static void
lapiz_spell_plugin_class_init (LapizSpellPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = lapiz_spell_plugin_dispose;
	object_class->set_property = lapiz_spell_plugin_set_property;
	object_class->get_property = lapiz_spell_plugin_get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");

	if (spell_checker_id == 0)
		spell_checker_id = g_quark_from_string ("LapizSpellCheckerID");

	if (check_range_id == 0)
		check_range_id = g_quark_from_string ("CheckRangeID");
}

static void
lapiz_spell_plugin_class_finalize (LapizSpellPluginClass *klass)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
peas_activatable_iface_init (PeasActivatableInterface *iface)
{
	iface->activate = lapiz_spell_plugin_activate;
	iface->deactivate = lapiz_spell_plugin_deactivate;
	iface->update_state = lapiz_spell_plugin_update_state;
}

static void
peas_ctk_configurable_iface_init (PeasGtkConfigurableInterface *iface)
{
	iface->create_configure_widget = lapiz_spell_plugin_create_configure_widget;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	lapiz_spell_plugin_register_type (G_TYPE_MODULE (module));

	peas_object_module_register_extension_type (module,
	                                            PEAS_TYPE_ACTIVATABLE,
	                                            LAPIZ_TYPE_SPELL_PLUGIN);

	peas_object_module_register_extension_type (module,
	                                            PEAS_GTK_TYPE_CONFIGURABLE,
	                                            LAPIZ_TYPE_SPELL_PLUGIN);
}
