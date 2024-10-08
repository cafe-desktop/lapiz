/*
 * lapiz-search-commands.c
 * This file is part of lapiz
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
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
 * Modified by the lapiz Team, 1998-2006. See the AUTHORS file for a
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
#include <ctk/ctk.h>
#include <cdk/cdkkeysyms.h>

#include "lapiz-commands.h"
#include "lapiz-debug.h"
#include "lapiz-statusbar.h"
#include "lapiz-window.h"
#include "lapiz-window-private.h"
#include "lapiz-utils.h"
#include "dialogs/lapiz-search-dialog.h"

#define LAPIZ_SEARCH_DIALOG_KEY		"lapiz-search-dialog-key"
#define LAPIZ_LAST_SEARCH_DATA_KEY	"lapiz-last-search-data-key"

typedef struct _LastSearchData LastSearchData;
struct _LastSearchData
{
	gint x;
	gint y;
};

static void
last_search_data_free (LastSearchData *data)
{
	g_slice_free (LastSearchData, data);
}

static void
last_search_data_restore_position (LapizSearchDialog *dlg)
{
	LastSearchData *data;

	data = g_object_get_data (G_OBJECT (dlg), LAPIZ_LAST_SEARCH_DATA_KEY);

	if ((data != NULL) && (ctk_widget_get_visible (CTK_WIDGET (dlg)) == FALSE))
	{
		ctk_window_move (CTK_WINDOW (dlg),
				 data->x,
				 data->y);
	}
}

static void
last_search_data_store_position (LapizSearchDialog *dlg)
{
	LastSearchData *data;

	data = g_object_get_data (G_OBJECT (dlg), LAPIZ_LAST_SEARCH_DATA_KEY);

	if (data == NULL)
	{
		data = g_slice_new (LastSearchData);

		g_object_set_data_full (G_OBJECT (dlg),
					LAPIZ_LAST_SEARCH_DATA_KEY,
					data,
					(GDestroyNotify) last_search_data_free);
	}

	ctk_window_get_position (CTK_WINDOW (dlg),
				 &data->x,
				 &data->y);
}

/* Use occurrences only for Replace All */
static void
text_found (LapizWindow *window,
	    gint         occurrences)
{
	if (occurrences > 1)
	{
		lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (window->priv->statusbar),
					       window->priv->generic_message_cid,
					       ngettext("Found and replaced %d occurrence",
					     	        "Found and replaced %d occurrences",
					     	        occurrences),
					       occurrences);
	}
	else
	{
		if (occurrences == 1)
			lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (window->priv->statusbar),
						       window->priv->generic_message_cid,
						       _("Found and replaced one occurrence"));
		else
			lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (window->priv->statusbar),
						       window->priv->generic_message_cid,
						       " ");
	}
}

#define MAX_MSG_LENGTH 40
static void
text_not_found (LapizWindow *window,
		const gchar *text)
{
	gchar *searched;

	searched = lapiz_utils_str_end_truncate (text, MAX_MSG_LENGTH);

	lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (window->priv->statusbar),
				       window->priv->generic_message_cid,
				       /* Translators: %s is replaced by the text
				          entered by the user in the search box */
				       _("\"%s\" not found"), searched);
	g_free (searched);
}

static gboolean
run_search (LapizView   *view,
	    gboolean     wrap_around,
	    gboolean     search_backwards)
{
	LapizDocument *doc;
	CtkTextIter start_iter;
	CtkTextIter match_start;
	CtkTextIter match_end;
	gboolean found = FALSE;

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (view)));

	if (!search_backwards)
	{
		ctk_text_buffer_get_selection_bounds (CTK_TEXT_BUFFER (doc),
						      NULL,
						      &start_iter);

		found = lapiz_document_search_forward (doc,
						       &start_iter,
						       NULL,
						       &match_start,
						       &match_end);
	}
	else
	{
		ctk_text_buffer_get_selection_bounds (CTK_TEXT_BUFFER (doc),
						      &start_iter,
						      NULL);

		found = lapiz_document_search_backward (doc,
							NULL,
							&start_iter,
							&match_start,
							&match_end);
	}

	if (!found && wrap_around)
	{
		if (!search_backwards)
			found = lapiz_document_search_forward (doc,
							       NULL,
							       NULL, /* FIXME: set the end_inter */
							       &match_start,
							       &match_end);
		else
			found = lapiz_document_search_backward (doc,
								NULL, /* FIXME: set the start_inter */
								NULL,
								&match_start,
								&match_end);
	}

	if (found)
	{
		ctk_text_buffer_place_cursor (CTK_TEXT_BUFFER (doc),
					      &match_start);

		ctk_text_buffer_move_mark_by_name (CTK_TEXT_BUFFER (doc),
						   "selection_bound",
						   &match_end);

		lapiz_view_scroll_to_cursor (view);
	}
	else
	{
		ctk_text_buffer_place_cursor (CTK_TEXT_BUFFER (doc),
					      &start_iter);
	}

	return found;
}

static void
do_find (LapizSearchDialog *dialog,
	 LapizWindow       *window)
{
	LapizView *active_view;
	LapizDocument *doc;
	gchar *search_text;
	const gchar *entry_text;
	gboolean match_case;
        gboolean match_regex;
	gboolean entire_word;
	gboolean wrap_around;
	gboolean search_backwards;
	gboolean parse_escapes;
	guint flags = 0;
	guint old_flags = 0;
	gboolean found;

	/* TODO: make the dialog insensitive when all the tabs are closed
	 * and assert here that the view is not NULL */
	active_view = lapiz_window_get_active_view (window);
	if (active_view == NULL)
		return;

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (active_view)));

	match_case = lapiz_search_dialog_get_match_case (dialog);
        match_regex = lapiz_search_dialog_get_match_regex(dialog);
	entire_word = lapiz_search_dialog_get_entire_word (dialog);
	search_backwards = lapiz_search_dialog_get_backwards (dialog);
	wrap_around = lapiz_search_dialog_get_wrap_around (dialog);
	parse_escapes = lapiz_search_dialog_get_parse_escapes (dialog);

	if (!parse_escapes) {
		entry_text = lapiz_utils_escape_search_text (lapiz_search_dialog_get_search_text (dialog));
	} else {
		entry_text = lapiz_search_dialog_get_search_text (dialog);
	}

	LAPIZ_SEARCH_SET_CASE_SENSITIVE (flags, match_case);
	LAPIZ_SEARCH_SET_ENTIRE_WORD (flags, entire_word);
        LAPIZ_SEARCH_SET_MATCH_REGEX (flags, match_regex);

	search_text = lapiz_document_get_search_text (doc, &old_flags);

	if ((search_text == NULL) ||
	    (strcmp (search_text, entry_text) != 0) ||
	    (flags != old_flags))
	{
		lapiz_document_set_search_text (doc, entry_text, flags);
	}

	g_free (search_text);
	if (match_regex)
	{
		lapiz_document_set_last_replace_text (doc, lapiz_search_dialog_get_replace_text (dialog));
	}

	found = run_search (active_view,
			    wrap_around,
			    search_backwards);

	if (found)
		text_found (window, 0);
	else {
		if (!parse_escapes) {
			text_not_found (window, lapiz_utils_unescape_search_text (entry_text));
		} else {
			text_not_found (window, entry_text);
		}
	}

	ctk_dialog_set_response_sensitive (CTK_DIALOG (dialog),
					   LAPIZ_SEARCH_DIALOG_REPLACE_RESPONSE,
					   found);
}

/* FIXME: move in lapiz-document.c and share it with lapiz-view */
static gboolean
get_selected_text (CtkTextBuffer  *doc,
		   gchar         **selected_text,
		   gint           *len)
{
	CtkTextIter start, end;

	g_return_val_if_fail (selected_text != NULL, FALSE);
	g_return_val_if_fail (*selected_text == NULL, FALSE);

	if (!ctk_text_buffer_get_selection_bounds (doc, &start, &end))
	{
		if (len != NULL)
			len = 0;

		return FALSE;
	}

	*selected_text = ctk_text_buffer_get_slice (doc, &start, &end, TRUE);

	if (len != NULL)
		*len = g_utf8_strlen (*selected_text, -1);

	return TRUE;
}

static void
replace_selected_text (CtkTextBuffer *buffer,
		       const gchar   *replace)
{
	g_return_if_fail (ctk_text_buffer_get_selection_bounds (buffer, NULL, NULL));
	g_return_if_fail (replace != NULL);

	ctk_text_buffer_begin_user_action (buffer);

	ctk_text_buffer_delete_selection (buffer, FALSE, TRUE);

	ctk_text_buffer_insert_at_cursor (buffer, replace, strlen (replace));

	ctk_text_buffer_end_user_action (buffer);
}

static void
do_replace (LapizSearchDialog *dialog,
	    LapizWindow       *window)
{
	LapizDocument *doc;
	const gchar *search_entry_text;
	const gchar *replace_entry_text;
	gchar *unescaped_search_text;
	gchar *unescaped_replace_text;
	gchar *selected_text = NULL;
	gboolean match_case;
        gboolean match_regex;
	gboolean parse_escapes;
        gboolean need_refind;

	doc = lapiz_window_get_active_document (window);
	if (doc == NULL)
		return;

	parse_escapes = lapiz_search_dialog_get_parse_escapes (dialog);
	if (!parse_escapes) {
		search_entry_text = lapiz_utils_escape_search_text (lapiz_search_dialog_get_search_text (dialog));
	} else {
		search_entry_text = lapiz_search_dialog_get_search_text (dialog);
	}
	g_return_if_fail ((search_entry_text) != NULL);
	g_return_if_fail ((*search_entry_text) != '\0');

	/* replace text may be "", we just delete */
	if (!parse_escapes) {
		replace_entry_text = lapiz_utils_escape_search_text (lapiz_search_dialog_get_replace_text (dialog));
	} else {
		replace_entry_text = lapiz_search_dialog_get_replace_text (dialog);
	}
	g_return_if_fail ((replace_entry_text) != NULL);

	unescaped_search_text = lapiz_utils_unescape_search_text (search_entry_text);

	get_selected_text (CTK_TEXT_BUFFER (doc),
			   &selected_text,
			   NULL);

	match_case = lapiz_search_dialog_get_match_case (dialog);
        match_regex = lapiz_search_dialog_get_match_regex(dialog);

        if (selected_text != NULL)
        {
		if(!match_regex)
		{
		need_refind = (match_case && (strcmp (selected_text,unescaped_search_text) != 0))
			       || (!match_case && !g_utf8_caselessnmatch (selected_text,
                                                                          unescaped_search_text,
                                                                          strlen (selected_text),
                                                                          strlen (unescaped_search_text)) != 0);
		}
		else
		{
		need_refind = !g_regex_match_simple(unescaped_search_text,
						    selected_text,
						    match_case ? 0 : CTK_TEXT_SEARCH_CASE_INSENSITIVE ,
						    0);
		}
	}
	else
	{
		need_refind = TRUE;
	}

	if (need_refind)
	{
		lapiz_document_set_last_replace_text (doc, replace_entry_text);
		do_find (dialog, window);
		g_free (unescaped_search_text);
		g_free (selected_text);

		return;
	}

	if(!match_regex)
		unescaped_replace_text = lapiz_utils_unescape_search_text (replace_entry_text);
	else
		unescaped_replace_text = g_strdup (lapiz_document_get_last_replace_text (doc));

	replace_selected_text (CTK_TEXT_BUFFER (doc), unescaped_replace_text);

	g_free (unescaped_search_text);
	g_free (selected_text);
	g_free (unescaped_replace_text);

	do_find (dialog, window);
}

static void
do_replace_all (LapizSearchDialog *dialog,
		LapizWindow       *window)
{
	LapizView *active_view;
	LapizDocument *doc;
	const gchar *search_entry_text;
	const gchar *replace_entry_text;
	gboolean match_case;
	gboolean match_regex;
	gboolean entire_word;
	gboolean parse_escapes;
	guint flags = 0;
	gint count;

	active_view = lapiz_window_get_active_view (window);
	if (active_view == NULL)
		return;

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (active_view)));

	parse_escapes = lapiz_search_dialog_get_parse_escapes (dialog);
	if (!parse_escapes) {
		search_entry_text = lapiz_utils_escape_search_text(lapiz_search_dialog_get_search_text (dialog));
	} else {
		search_entry_text = lapiz_search_dialog_get_search_text (dialog);
	}
	g_return_if_fail ((search_entry_text) != NULL);
	g_return_if_fail ((*search_entry_text) != '\0');

	/* replace text may be "", we just delete all occurrencies */
	if (!parse_escapes) {
		replace_entry_text = lapiz_utils_escape_search_text (lapiz_search_dialog_get_replace_text (dialog));
	} else {
		replace_entry_text = lapiz_search_dialog_get_replace_text (dialog);
	}
	g_return_if_fail ((replace_entry_text) != NULL);

	match_case = lapiz_search_dialog_get_match_case (dialog);
    match_regex = lapiz_search_dialog_get_match_regex(dialog);
	entire_word = lapiz_search_dialog_get_entire_word (dialog);

	LAPIZ_SEARCH_SET_CASE_SENSITIVE (flags, match_case);
	LAPIZ_SEARCH_SET_MATCH_REGEX (flags, match_regex);
	LAPIZ_SEARCH_SET_ENTIRE_WORD (flags, entire_word);

	count = lapiz_document_replace_all (doc,
					    search_entry_text,
					    replace_entry_text,
					    flags);

	if (count > 0)
	{
		text_found (window, count);
	}
	else
	{
		if (!parse_escapes) {
			text_not_found (window, lapiz_utils_unescape_search_text (search_entry_text));
		} else {
			text_not_found (window, search_entry_text);
		}
	}

	ctk_dialog_set_response_sensitive (CTK_DIALOG (dialog),
					   LAPIZ_SEARCH_DIALOG_REPLACE_RESPONSE,
					   FALSE);
}

static void
search_dialog_response_cb (LapizSearchDialog *dialog,
			   gint               response_id,
			   LapizWindow       *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	switch (response_id)
	{
		case LAPIZ_SEARCH_DIALOG_FIND_RESPONSE:
			do_find (dialog, window);
			break;
		case LAPIZ_SEARCH_DIALOG_REPLACE_RESPONSE:
			do_replace (dialog, window);
			break;
		case LAPIZ_SEARCH_DIALOG_REPLACE_ALL_RESPONSE:
			do_replace_all (dialog, window);
			break;
		default:
			last_search_data_store_position (dialog);
			ctk_widget_hide (CTK_WIDGET (dialog));
	}
}

static gboolean
search_dialog_delete_event_cb (CtkWidget   *widget G_GNUC_UNUSED,
			       CdkEventAny *event G_GNUC_UNUSED,
			       gpointer     user_data G_GNUC_UNUSED)
{
	lapiz_debug (DEBUG_COMMANDS);

	/* prevent destruction */
	return TRUE;
}

static void
search_dialog_destroyed (LapizWindow       *window,
			 LapizSearchDialog *dialog)
{
	lapiz_debug (DEBUG_COMMANDS);

	g_object_set_data (G_OBJECT (window),
			   LAPIZ_SEARCH_DIALOG_KEY,
			   NULL);
	g_object_set_data (G_OBJECT (dialog),
			   LAPIZ_LAST_SEARCH_DATA_KEY,
			   NULL);
}

static CtkWidget *
create_dialog (LapizWindow *window, gboolean show_replace)
{
	CtkWidget *dialog;

	dialog = lapiz_search_dialog_new (CTK_WINDOW (window), show_replace);

	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (search_dialog_response_cb),
			  window);
	g_signal_connect (dialog,
			 "delete-event",
			 G_CALLBACK (search_dialog_delete_event_cb),
			 NULL);

	g_object_set_data (G_OBJECT (window),
			   LAPIZ_SEARCH_DIALOG_KEY,
			   dialog);

	g_object_weak_ref (G_OBJECT (dialog),
			   (GWeakNotify) search_dialog_destroyed,
			   window);

	return dialog;
}

void
_lapiz_cmd_search_find (CtkAction   *action G_GNUC_UNUSED,
			LapizWindow *window)
{
	gpointer data;
	CtkWidget *search_dialog;
	LapizDocument *doc;
	gboolean selection_exists;
	gboolean parse_escapes;
	gchar *find_text = NULL;
	const gchar *search_text = NULL;
	gint sel_len;

	lapiz_debug (DEBUG_COMMANDS);

	data = g_object_get_data (G_OBJECT (window), LAPIZ_SEARCH_DIALOG_KEY);

	if (data == NULL)
	{
		search_dialog = create_dialog (window, FALSE);
	}
	else
	{
		g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (data));

		search_dialog = CTK_WIDGET (data);

		/* turn the dialog into a find dialog if needed */
		if (lapiz_search_dialog_get_show_replace (LAPIZ_SEARCH_DIALOG (search_dialog)))
			lapiz_search_dialog_set_show_replace (LAPIZ_SEARCH_DIALOG (search_dialog),
							      FALSE);
	}

	doc = lapiz_window_get_active_document (window);
	g_return_if_fail (doc != NULL);

	selection_exists = get_selected_text (CTK_TEXT_BUFFER (doc),
					      &find_text,
					      &sel_len);

	if (selection_exists && find_text != NULL && sel_len < 80)
	{
		/*
		 * Special case: if the currently selected text
		 * is the same as the unescaped search text and
		 * escape sequence parsing is activated, use the
		 * same old search text. (Without this, if you e.g.
		 * search for '\n' in escaped mode and then open
		 * the search dialog again, you'll get an unprintable
		 * single-character literal '\n' in the "search for"
		 * box).
		 */
		parse_escapes = lapiz_search_dialog_get_parse_escapes (LAPIZ_SEARCH_DIALOG (search_dialog));
		search_text = lapiz_search_dialog_get_search_text (LAPIZ_SEARCH_DIALOG (search_dialog));
		if (!(search_text != NULL
		      && !strcmp(lapiz_utils_unescape_search_text(search_text), find_text)
		      && parse_escapes)) {
			/* General case */
			lapiz_search_dialog_set_search_text (LAPIZ_SEARCH_DIALOG (search_dialog),
							     find_text);
		}
		g_free (find_text);
	}
	else
	{
		g_free (find_text);
	}

	ctk_widget_show (search_dialog);
	last_search_data_restore_position (LAPIZ_SEARCH_DIALOG (search_dialog));
	lapiz_search_dialog_present_with_time (LAPIZ_SEARCH_DIALOG (search_dialog),
					       CDK_CURRENT_TIME);
}

void
_lapiz_cmd_search_replace (CtkAction   *action G_GNUC_UNUSED,
			   LapizWindow *window)
{
	gpointer data;
	CtkWidget *replace_dialog;
	LapizDocument *doc;
	gboolean selection_exists;
	gboolean parse_escapes;
	gchar *find_text = NULL;
	const gchar *search_text = NULL;
	gint sel_len;

	lapiz_debug (DEBUG_COMMANDS);

	data = g_object_get_data (G_OBJECT (window), LAPIZ_SEARCH_DIALOG_KEY);

	if (data == NULL)
	{
		replace_dialog = create_dialog (window, TRUE);
	}
	else
	{
		g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (data));

		replace_dialog = CTK_WIDGET (data);

		/* turn the dialog into a find dialog if needed */
		if (!lapiz_search_dialog_get_show_replace (LAPIZ_SEARCH_DIALOG (replace_dialog)))
			lapiz_search_dialog_set_show_replace (LAPIZ_SEARCH_DIALOG (replace_dialog),
							      TRUE);
	}

	doc = lapiz_window_get_active_document (window);
	g_return_if_fail (doc != NULL);

	selection_exists = get_selected_text (CTK_TEXT_BUFFER (doc),
					      &find_text,
					      &sel_len);

	if (selection_exists && find_text != NULL && sel_len < 80)
	{
		/*
		 * Special case: if the currently selected text
		 * is the same as the unescaped search text and
		 * escape sequence parsing is activated, use the
		 * same old search text. (Without this, if you e.g.
		 * search for '\n' in escaped mode and then open
		 * the search dialog again, you'll get an unprintable
		 * single-character literal '\n' in the "search for"
		 * box).
		 */
		parse_escapes = lapiz_search_dialog_get_parse_escapes (LAPIZ_SEARCH_DIALOG (replace_dialog));
		search_text = lapiz_search_dialog_get_search_text (LAPIZ_SEARCH_DIALOG (replace_dialog));
		if (!(search_text != NULL
		      && !strcmp(lapiz_utils_unescape_search_text(search_text), find_text)
		      && parse_escapes)) {
			/* General case */
			lapiz_search_dialog_set_search_text (LAPIZ_SEARCH_DIALOG (replace_dialog),
							     find_text);
		}
		g_free (find_text);
	}
	else
	{
		g_free (find_text);
	}

	ctk_widget_show (replace_dialog);
	last_search_data_restore_position (LAPIZ_SEARCH_DIALOG (replace_dialog));
	lapiz_search_dialog_present_with_time (LAPIZ_SEARCH_DIALOG (replace_dialog),
					       CDK_CURRENT_TIME);
}

static void
do_find_again (LapizWindow *window,
	       gboolean     backward)
{
	LapizView *active_view;
	gboolean wrap_around = TRUE;
	gpointer data;

	active_view = lapiz_window_get_active_view (window);
	g_return_if_fail (active_view != NULL);

	data = g_object_get_data (G_OBJECT (window), LAPIZ_SEARCH_DIALOG_KEY);

	if (data != NULL)
		wrap_around = lapiz_search_dialog_get_wrap_around (LAPIZ_SEARCH_DIALOG (data));

	run_search (active_view,
		    wrap_around,
		    backward);
}

void
_lapiz_cmd_search_find_next (CtkAction   *action G_GNUC_UNUSED,
			     LapizWindow *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	do_find_again (window, FALSE);
}

void
_lapiz_cmd_search_find_prev (CtkAction   *action G_GNUC_UNUSED,
			     LapizWindow *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	do_find_again (window, TRUE);
}

void
_lapiz_cmd_search_clear_highlight (CtkAction   *action G_GNUC_UNUSED,
				   LapizWindow *window)
{
	LapizDocument *doc;

	lapiz_debug (DEBUG_COMMANDS);

	doc = lapiz_window_get_active_document (window);
	lapiz_document_set_search_text (LAPIZ_DOCUMENT (doc),
					"",
					LAPIZ_SEARCH_DONT_SET_FLAGS);
}

void
_lapiz_cmd_search_goto_line (CtkAction   *action G_GNUC_UNUSED,
			     LapizWindow *window)
{
	LapizView *active_view;

	lapiz_debug (DEBUG_COMMANDS);

	active_view = lapiz_window_get_active_view (window);
	if (active_view == NULL)
		return;

	/* Focus the view if needed: we need to focus the view otherwise
	   activating the binding for goto line has no effect */
	ctk_widget_grab_focus (CTK_WIDGET (active_view));


	/* goto line is builtin in LapizView, just activate
	 * the corresponding binding.
	 */
	ctk_bindings_activate (G_OBJECT (active_view),
			       CDK_KEY_i,
			       CDK_CONTROL_MASK);
}

void
_lapiz_cmd_search_incremental_search (CtkAction   *action G_GNUC_UNUSED,
				      LapizWindow *window)
{
	LapizView *active_view;

	lapiz_debug (DEBUG_COMMANDS);

	active_view = lapiz_window_get_active_view (window);
	if (active_view == NULL)
		return;

	/* Focus the view if needed: we need to focus the view otherwise
	   activating the binding for incremental search has no effect */
	ctk_widget_grab_focus (CTK_WIDGET (active_view));

	/* incremental search is builtin in LapizView, just activate
	 * the corresponding binding.
	 */
	ctk_bindings_activate (G_OBJECT (active_view),
			       CDK_KEY_k,
			       CDK_CONTROL_MASK);
}
