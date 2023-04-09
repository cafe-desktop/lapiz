/*
 * lapiz-commands-file.c
 * This file is part of lapiz
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
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
 * Modified by the lapiz Team, 1998-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h> /* For strlen and strcmp */

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <ctk/ctk.h>

#include "lapiz-commands.h"
#include "lapiz-window.h"
#include "lapiz-window-private.h"
#include "lapiz-statusbar.h"
#include "lapiz-debug.h"
#include "lapiz-utils.h"
#include "lapiz-file-chooser-dialog.h"
#include "dialogs/lapiz-close-confirmation-dialog.h"


/* Defined constants */
#define LAPIZ_OPEN_DIALOG_KEY 		"lapiz-open-dialog-key"
#define LAPIZ_TAB_TO_SAVE_AS  		"lapiz-tab-to-save-as"
#define LAPIZ_LIST_OF_TABS_TO_SAVE_AS   "lapiz-list-of-tabs-to-save-as"
#define LAPIZ_IS_CLOSING_ALL            "lapiz-is-closing-all"
#define LAPIZ_IS_QUITTING 	        "lapiz-is-quitting"
#define LAPIZ_IS_CLOSING_TAB		"lapiz-is-closing-tab"
#define LAPIZ_IS_QUITTING_ALL		"lapiz-is-quitting-all"

static void tab_state_changed_while_saving (LapizTab    *tab,
					    GParamSpec  *pspec,
					    LapizWindow *window);

void
_lapiz_cmd_file_new (CtkAction   *action,
		     LapizWindow *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	lapiz_window_create_tab (window, TRUE);
}

static LapizTab *
get_tab_from_file (GList *docs, GFile *file)
{
	LapizTab *tab = NULL;

	while (docs != NULL)
	{
		LapizDocument *d;
		GFile *l;

		d = LAPIZ_DOCUMENT (docs->data);

		l = lapiz_document_get_location (d);
		if (l != NULL)
		{
			if (g_file_equal (l, file))
			{
				tab = lapiz_tab_get_from_document (d);
				g_object_unref (l);
				break;
			}

			g_object_unref (l);
		}

		docs = g_list_next (docs);
	}

	return tab;
}

static gboolean
is_duplicated_file (GSList *files, GFile *file)
{
	while (files != NULL)
	{
		if (g_file_equal (files->data, file))
			return TRUE;

		files = g_slist_next (files);
	}

	return FALSE;
}

/* File loading */
static gint
load_file_list (LapizWindow         *window,
		GSList              *files,
		const LapizEncoding *encoding,
		gint                 line_pos,
		gboolean             create)
{
	LapizTab      *tab;
	gint           loaded_files = 0; /* Number of files to load */
	gboolean       jump_to = TRUE; /* Whether to jump to the new tab */
	GList         *win_docs;
	GSList        *files_to_load = NULL;
	GSList        *l;

	lapiz_debug (DEBUG_COMMANDS);

	win_docs = lapiz_window_get_documents (window);

	/* Remove the uris corresponding to documents already open
	 * in "window" and remove duplicates from "uris" list */
	for (l = files; l != NULL; l = l->next)
	{
		if (!is_duplicated_file (files_to_load, l->data))
		{
			tab = get_tab_from_file (win_docs, l->data);
			if (tab != NULL)
			{
				if (l == files)
				{
					lapiz_window_set_active_tab (window, tab);
					jump_to = FALSE;

					if (line_pos > 0)
					{
						LapizDocument *doc;
						LapizView *view;

						doc = lapiz_tab_get_document (tab);
						view = lapiz_tab_get_view (tab);

						/* document counts lines starting from 0 */
						lapiz_document_goto_line (doc, line_pos - 1);
						lapiz_view_scroll_to_cursor (view);
					}
				}

				++loaded_files;
			}
			else
			{
				files_to_load = g_slist_prepend (files_to_load,
								 l->data);
			}
		}
	}

	g_list_free (win_docs);

	if (files_to_load == NULL)
		return loaded_files;

	files_to_load = g_slist_reverse (files_to_load);
	l = files_to_load;

	tab = lapiz_window_get_active_tab (window);
	if (tab != NULL)
	{
		LapizDocument *doc;

		doc = lapiz_tab_get_document (tab);

		if (lapiz_document_is_untouched (doc) &&
		    (lapiz_tab_get_state (tab) == LAPIZ_TAB_STATE_NORMAL))
		{
			gchar *uri;

			// FIXME: pass the GFile to tab when api is there
			uri = g_file_get_uri (l->data);
			_lapiz_tab_load (tab,
					 uri,
					 encoding,
					 line_pos,
					 create);
			g_free (uri);

			l = g_slist_next (l);
			jump_to = FALSE;

			++loaded_files;
		}
	}

	while (l != NULL)
	{
		gchar *uri;

		g_return_val_if_fail (l->data != NULL, 0);

		// FIXME: pass the GFile to tab when api is there
		uri = g_file_get_uri (l->data);
		tab = lapiz_window_create_tab_from_uri (window,
							uri,
							encoding,
							line_pos,
							create,
							jump_to);
		g_free (uri);

		if (tab != NULL)
		{
			jump_to = FALSE;
			++loaded_files;
		}

		l = g_slist_next (l);
	}

	if (loaded_files == 1)
	{
		LapizDocument *doc;
		gchar *uri_for_display;

		g_return_val_if_fail (tab != NULL, loaded_files);

		doc = lapiz_tab_get_document (tab);
		uri_for_display = lapiz_document_get_uri_for_display (doc);

		lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (window->priv->statusbar),
					       window->priv->generic_message_cid,
					       _("Loading file '%s'\342\200\246"),
					       uri_for_display);

		g_free (uri_for_display);
	}
	else
	{
		lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (window->priv->statusbar),
					       window->priv->generic_message_cid,
					       ngettext("Loading %d file\342\200\246",
							"Loading %d files\342\200\246",
							loaded_files),
					       loaded_files);
	}

	/* Free uris_to_load. Note that l points to the first element of uris_to_load */
	g_slist_free (files_to_load);

	return loaded_files;
}


// FIXME: we should expose API with GFile and just make the uri
// variants backward compat wrappers

static gint
load_uri_list (LapizWindow         *window,
	       const GSList        *uris,
	       const LapizEncoding *encoding,
	       gint                 line_pos,
	       gboolean             create)
{
	GSList *files = NULL;
	const GSList *u;
	gint ret;

	for (u = uris; u != NULL; u = u->next)
	{
		gchar *uri = u->data;

		if (lapiz_utils_is_valid_uri (uri))
			files = g_slist_prepend (files, g_file_new_for_uri (uri));
		else
			g_warning ("invalid uri: %s", uri);
	}
	files = g_slist_reverse (files);

	ret = load_file_list (window, files, encoding, line_pos, create);

	g_slist_foreach (files, (GFunc) g_object_unref, NULL);
	g_slist_free (files);

	return ret;
}

/**
 * lapiz_commands_load_uri:
 * @window:
 * @uri:
 * @encoding: (allow-none):
 * @line_pos:
 *
 * Do nothing if uri does not exist
 */
void
lapiz_commands_load_uri (LapizWindow         *window,
			 const gchar         *uri,
			 const LapizEncoding *encoding,
			 gint                 line_pos)
{
	GSList *uris = NULL;

	g_return_if_fail (LAPIZ_IS_WINDOW (window));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (lapiz_utils_is_valid_uri (uri));

	lapiz_debug_message (DEBUG_COMMANDS, "Loading URI '%s'", uri);

	uris = g_slist_prepend (uris, (gchar *)uri);

	load_uri_list (window, uris, encoding, line_pos, FALSE);

	g_slist_free (uris);
}

/**
 * lapiz_commands_load_uris:
 * @window:
 * @uris:
 * @encoding:
 * @line_pos:
 *
 * Ignore non-existing URIs
 *
 * Returns:
 */
gint
lapiz_commands_load_uris (LapizWindow         *window,
			  const GSList        *uris,
			  const LapizEncoding *encoding,
			  gint                 line_pos)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), 0);
	g_return_val_if_fail ((uris != NULL) && (uris->data != NULL), 0);

	lapiz_debug (DEBUG_COMMANDS);

	return load_uri_list (window, uris, encoding, line_pos, FALSE);
}

/*
 * This should become public once we convert all api to GFile:
 */
static gint
lapiz_commands_load_files (LapizWindow         *window,
			   GSList              *files,
			   const LapizEncoding *encoding,
			   gint                 line_pos)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), 0);
	g_return_val_if_fail ((files != NULL) && (files->data != NULL), 0);

	lapiz_debug (DEBUG_COMMANDS);

	return load_file_list (window, files, encoding, line_pos, FALSE);
}

/*
 * From the command line we can specify a line position for the
 * first doc. Beside specifying a not existing uri creates a
 * titled document.
 */
gint
_lapiz_cmd_load_files_from_prompt (LapizWindow         *window,
				   GSList              *files,
				   const LapizEncoding *encoding,
				   gint                 line_pos)
{
	lapiz_debug (DEBUG_COMMANDS);

	return load_file_list (window, files, encoding, line_pos, TRUE);
}

static void
open_dialog_destroyed (LapizWindow            *window,
		       LapizFileChooserDialog *dialog)
{
	lapiz_debug (DEBUG_COMMANDS);

	g_object_set_data (G_OBJECT (window),
			   LAPIZ_OPEN_DIALOG_KEY,
			   NULL);
}

static void
open_dialog_response_cb (LapizFileChooserDialog *dialog,
                         gint                    response_id,
                         LapizWindow            *window)
{
	GSList *files;
	const LapizEncoding *encoding;

	lapiz_debug (DEBUG_COMMANDS);

	if (response_id != CTK_RESPONSE_OK)
	{
		ctk_widget_destroy (CTK_WIDGET (dialog));

		return;
	}

	files = ctk_file_chooser_get_files (CTK_FILE_CHOOSER (dialog));
	g_return_if_fail (files != NULL);

	encoding = lapiz_file_chooser_dialog_get_encoding (dialog);

	ctk_widget_destroy (CTK_WIDGET (dialog));

	/* Remember the folder we navigated to */
	 _lapiz_window_set_default_location (window, files->data);

	lapiz_commands_load_files (window,
				   files,
				   encoding,
				   0);

	g_slist_foreach (files, (GFunc) g_object_unref, NULL);
	g_slist_free (files);
}

void
_lapiz_cmd_file_open (CtkAction   *action,
		      LapizWindow *window)
{
	CtkWidget *open_dialog;
	gpointer data;
	LapizDocument *doc;
	GFile *default_path = NULL;

	lapiz_debug (DEBUG_COMMANDS);

	data = g_object_get_data (G_OBJECT (window), LAPIZ_OPEN_DIALOG_KEY);

	if (data != NULL)
	{
		g_return_if_fail (LAPIZ_IS_FILE_CHOOSER_DIALOG (data));

		ctk_window_present (CTK_WINDOW (data));

		return;
	}

	/* Translators: "Open Files" is the title of the file chooser window */
	open_dialog = lapiz_file_chooser_dialog_new (_("Open Files"),
						     CTK_WINDOW (window),
						     CTK_FILE_CHOOSER_ACTION_OPEN,
						     NULL,
						     "process-stop", CTK_RESPONSE_CANCEL,
						     "document-open", CTK_RESPONSE_OK,
						     NULL);

	g_object_set_data (G_OBJECT (window),
			   LAPIZ_OPEN_DIALOG_KEY,
			   open_dialog);

	g_object_weak_ref (G_OBJECT (open_dialog),
			   (GWeakNotify) open_dialog_destroyed,
			   window);

	/* Set the curret folder uri */
	doc = lapiz_window_get_active_document (window);
	if (doc != NULL)
	{
		GFile *file;

		file = lapiz_document_get_location (doc);

		if (file != NULL)
		{
			default_path = g_file_get_parent (file);
			g_object_unref (file);
		}
	}

	if (default_path == NULL)
		default_path = _lapiz_window_get_default_location (window);

	if (default_path != NULL)
	{
		gchar *uri;

		uri = g_file_get_uri (default_path);
		ctk_file_chooser_set_current_folder_uri (CTK_FILE_CHOOSER (open_dialog),
							 uri);

		g_free (uri);
		g_object_unref (default_path);
	}

	g_signal_connect (open_dialog,
			  "response",
			  G_CALLBACK (open_dialog_response_cb),
			  window);

	ctk_widget_show (open_dialog);
}

/* File saving */
static void file_save_as (LapizTab *tab, LapizWindow *window);

static gboolean
is_read_only (GFile *location)
{
	gboolean ret = TRUE; /* default to read only */
	GFileInfo *info;

	lapiz_debug (DEBUG_COMMANDS);

	info = g_file_query_info (location,
				  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  NULL);

	if (info != NULL)
	{
		if (g_file_info_has_attribute (info,
					       G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
		{
			ret = !g_file_info_get_attribute_boolean (info,
								  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
		}

		g_object_unref (info);
	}

	return ret;
}

/* FIXME: modify this dialog to be similar to the one provided by ctk+ for
 * already existing files - Paolo (Oct. 11, 2005) */
static gboolean
replace_read_only_file (CtkWindow *parent, GFile *file)
{
	CtkWidget *dialog;
	gint ret;
	gchar *parse_name;
	gchar *name_for_display;

	lapiz_debug (DEBUG_COMMANDS);

	parse_name = g_file_get_parse_name (file);

	/* Truncate the name so it doesn't get insanely wide. Note that even
	 * though the dialog uses wrapped text, if the name doesn't contain
	 * white space then the text-wrapping code is too stupid to wrap it.
	 */
	name_for_display = lapiz_utils_str_middle_truncate (parse_name, 50);
	g_free (parse_name);

	dialog = ctk_message_dialog_new (parent,
					 CTK_DIALOG_DESTROY_WITH_PARENT,
					 CTK_MESSAGE_QUESTION,
					 CTK_BUTTONS_NONE,
					 _("The file \"%s\" is read-only."),
				         name_for_display);
	g_free (name_for_display);

	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
						  _("Do you want to try to replace it "
						    "with the one you are saving?"));

	lapiz_dialog_add_button (CTK_DIALOG (dialog),
				 _("_Cancel"),
				 "process-stop",
				 CTK_RESPONSE_CANCEL);

	lapiz_dialog_add_button (CTK_DIALOG (dialog),
				 _("_Replace"),
			  	 "document-save-as",
			  	 CTK_RESPONSE_YES);

	ctk_dialog_set_default_response	(CTK_DIALOG (dialog),
					 CTK_RESPONSE_CANCEL);

	ctk_window_set_resizable (CTK_WINDOW (dialog), FALSE);

	ret = ctk_dialog_run (CTK_DIALOG (dialog));

	ctk_widget_destroy (dialog);

	return (ret == CTK_RESPONSE_YES);
}

static void
save_dialog_response_cb (LapizFileChooserDialog *dialog,
                         gint                    response_id,
                         LapizWindow            *window)
{
	GFile *file;
	const LapizEncoding *encoding;
	LapizTab *tab;
	gpointer data;
	GSList *tabs_to_save_as;
	LapizDocumentNewlineType newline_type;

	lapiz_debug (DEBUG_COMMANDS);

	tab = LAPIZ_TAB (g_object_get_data (G_OBJECT (dialog),
					    LAPIZ_TAB_TO_SAVE_AS));

	if (response_id != CTK_RESPONSE_OK)
	{
		ctk_widget_destroy (CTK_WIDGET (dialog));

		goto save_next_tab;
	}

	file = ctk_file_chooser_get_file (CTK_FILE_CHOOSER (dialog));
	g_return_if_fail (file != NULL);

	encoding = lapiz_file_chooser_dialog_get_encoding (dialog);
	newline_type = lapiz_file_chooser_dialog_get_newline_type (dialog);

	ctk_widget_destroy (CTK_WIDGET (dialog));

	if (tab != NULL)
	{
		LapizDocument *doc;
		gchar *parse_name;
		gchar *uri;

		doc = lapiz_tab_get_document (tab);
		g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

		parse_name = g_file_get_parse_name (file);

		lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (window->priv->statusbar),
					        window->priv->generic_message_cid,
					       _("Saving file '%s'\342\200\246"),
					       parse_name);

		g_free (parse_name);

		/* let's remember the dir we navigated too,
		 * even if the saving fails... */
		 _lapiz_window_set_default_location (window, file);

		// FIXME: pass the GFile to tab when api is there
		uri = g_file_get_uri (file);
		_lapiz_tab_save_as (tab, uri, encoding, newline_type);
		g_free (uri);
	}

	g_object_unref (file);

save_next_tab:

	data = g_object_get_data (G_OBJECT (window),
				  LAPIZ_LIST_OF_TABS_TO_SAVE_AS);
	if (data == NULL)
		return;

	/* Save As the next tab of the list (we are Saving All files) */
	tabs_to_save_as = (GSList *)data;
	g_return_if_fail (tab == LAPIZ_TAB (tabs_to_save_as->data));

	/* Remove the first item of the list */
	tabs_to_save_as = g_slist_delete_link (tabs_to_save_as,
					       tabs_to_save_as);

	g_object_set_data (G_OBJECT (window),
			   LAPIZ_LIST_OF_TABS_TO_SAVE_AS,
			   tabs_to_save_as);

	if (tabs_to_save_as != NULL)
	{
		tab = LAPIZ_TAB (tabs_to_save_as->data);

		if (GPOINTER_TO_BOOLEAN (g_object_get_data (G_OBJECT (tab),
							    LAPIZ_IS_CLOSING_TAB)) == TRUE)
		{
			g_object_set_data (G_OBJECT (tab),
					   LAPIZ_IS_CLOSING_TAB,
					   NULL);

			/* Trace tab state changes */
			g_signal_connect (tab,
					  "notify::state",
					  G_CALLBACK (tab_state_changed_while_saving),
					  window);
		}

		lapiz_window_set_active_tab (window, tab);
		file_save_as (tab, window);
	}
}

static CtkFileChooserConfirmation
confirm_overwrite_callback (CtkFileChooser *dialog,
			    gpointer        data)
{
	gchar *uri;
	GFile *file;
	CtkFileChooserConfirmation res;

	lapiz_debug (DEBUG_COMMANDS);

	uri = ctk_file_chooser_get_uri (dialog);
	file = g_file_new_for_uri (uri);
	g_free (uri);

	if (is_read_only (file))
	{
		if (replace_read_only_file (CTK_WINDOW (dialog), file))
			res = CTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME;
		else
			res = CTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN;
	}
	else
	{
		/* fall back to the default confirmation dialog */
		res = CTK_FILE_CHOOSER_CONFIRMATION_CONFIRM;
	}

	g_object_unref (file);

	return res;
}

static void
file_save_as (LapizTab    *tab,
	      LapizWindow *window)
{
	CtkWidget *save_dialog;
	CtkWindowGroup *wg;
	LapizDocument *doc;
	GFile *file;
	gboolean uri_set = FALSE;
	const LapizEncoding *encoding;
	LapizDocumentNewlineType newline_type;

	g_return_if_fail (LAPIZ_IS_TAB (tab));
	g_return_if_fail (LAPIZ_IS_WINDOW (window));

	lapiz_debug (DEBUG_COMMANDS);

	save_dialog = lapiz_file_chooser_dialog_new (_("Save As\342\200\246"),
						     CTK_WINDOW (window),
						     CTK_FILE_CHOOSER_ACTION_SAVE,
						     NULL,
						     "process-stop", CTK_RESPONSE_CANCEL,
						     "document-save", CTK_RESPONSE_OK,
						     NULL);

	ctk_file_chooser_set_do_overwrite_confirmation (CTK_FILE_CHOOSER (save_dialog),
							TRUE);
	g_signal_connect (save_dialog,
			  "confirm-overwrite",
			  G_CALLBACK (confirm_overwrite_callback),
			  NULL);

	wg = lapiz_window_get_group (window);

	ctk_window_group_add_window (wg,
				     CTK_WINDOW (save_dialog));

	/* Save As dialog is modal to its main window */
	ctk_window_set_modal (CTK_WINDOW (save_dialog), TRUE);

	/* Set the suggested file name */
	doc = lapiz_tab_get_document (tab);
	file = lapiz_document_get_location (doc);

	if (file != NULL)
	{
		uri_set = ctk_file_chooser_set_file (CTK_FILE_CHOOSER (save_dialog),
						     file,
						     NULL);

		g_object_unref (file);
	}


	if (!uri_set)
	{
		GFile *default_path;
		gchar *docname;

		default_path = _lapiz_window_get_default_location (window);
		docname = lapiz_document_get_short_name_for_display (doc);

		if (default_path != NULL)
		{
			gchar *uri;

			uri = g_file_get_uri (default_path);
			ctk_file_chooser_set_current_folder_uri (CTK_FILE_CHOOSER (save_dialog),
								 uri);

			g_free (uri);
			g_object_unref (default_path);
		}

		ctk_file_chooser_set_current_name (CTK_FILE_CHOOSER (save_dialog),
						   docname);

		g_free (docname);
	}

	/* Set suggested encoding */
	encoding = lapiz_document_get_encoding (doc);
	g_return_if_fail (encoding != NULL);

	newline_type = lapiz_document_get_newline_type (doc);

	lapiz_file_chooser_dialog_set_encoding (LAPIZ_FILE_CHOOSER_DIALOG (save_dialog),
						encoding);

	lapiz_file_chooser_dialog_set_newline_type (LAPIZ_FILE_CHOOSER_DIALOG (save_dialog),
	                                            newline_type);

	g_object_set_data (G_OBJECT (save_dialog),
			   LAPIZ_TAB_TO_SAVE_AS,
			   tab);

	g_signal_connect (save_dialog,
			  "response",
			  G_CALLBACK (save_dialog_response_cb),
			  window);

	ctk_widget_show (save_dialog);
}

static void
file_save (LapizTab    *tab,
	   LapizWindow *window)
{
	LapizDocument *doc;
	gchar *uri_for_display;

	lapiz_debug (DEBUG_COMMANDS);

	g_return_if_fail (LAPIZ_IS_TAB (tab));
	g_return_if_fail (LAPIZ_IS_WINDOW (window));

	doc = lapiz_tab_get_document (tab);
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

	if (lapiz_document_is_untitled (doc) ||
	    lapiz_document_get_readonly (doc))
	{
		lapiz_debug_message (DEBUG_COMMANDS, "Untitled or Readonly");

		file_save_as (tab, window);

		return;
	}

	uri_for_display = lapiz_document_get_uri_for_display (doc);
	lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (window->priv->statusbar),
				        window->priv->generic_message_cid,
				       _("Saving file '%s'\342\200\246"),
				       uri_for_display);

	g_free (uri_for_display);

	_lapiz_tab_save (tab);
}

void
_lapiz_cmd_file_save (CtkAction   *action,
		     LapizWindow *window)
{
	LapizTab *tab;

	lapiz_debug (DEBUG_COMMANDS);

	tab = lapiz_window_get_active_tab (window);
	if (tab == NULL)
		return;

	file_save (tab, window);
}

void
_lapiz_cmd_file_save_as (CtkAction   *action,
			LapizWindow *window)
{
	LapizTab *tab;

	lapiz_debug (DEBUG_COMMANDS);

	tab = lapiz_window_get_active_tab (window);
	if (tab == NULL)
		return;

	file_save_as (tab, window);
}

static gboolean
document_needs_saving (LapizDocument *doc)
{
	if (ctk_text_buffer_get_modified (CTK_TEXT_BUFFER (doc)))
		return TRUE;

	/* we check if it was deleted only for local files
	 * since for remote files it may hang */
	if (lapiz_document_is_local (doc) && lapiz_document_get_deleted (doc))
		return TRUE;

	return FALSE;
}

/*
 * The docs in the list must belong to the same LapizWindow.
 */
void
_lapiz_cmd_file_save_documents_list (LapizWindow *window,
				     GList       *docs)
{
	GList *l;
	GSList *tabs_to_save_as = NULL;

	lapiz_debug (DEBUG_COMMANDS);

	g_return_if_fail (!(lapiz_window_get_state (window) &
			    (LAPIZ_WINDOW_STATE_PRINTING |
			     LAPIZ_WINDOW_STATE_SAVING_SESSION)));

	l = docs;
	while (l != NULL)
	{
		LapizDocument *doc;
		LapizTab *t;
		LapizTabState state;

		g_return_if_fail (LAPIZ_IS_DOCUMENT (l->data));

		doc = LAPIZ_DOCUMENT (l->data);
		t = lapiz_tab_get_from_document (doc);
		state = lapiz_tab_get_state (t);

		g_return_if_fail (state != LAPIZ_TAB_STATE_PRINTING);
		g_return_if_fail (state != LAPIZ_TAB_STATE_PRINT_PREVIEWING);
		g_return_if_fail (state != LAPIZ_TAB_STATE_CLOSING);

		if ((state == LAPIZ_TAB_STATE_NORMAL) ||
		    (state == LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW) ||
		    (state == LAPIZ_TAB_STATE_GENERIC_NOT_EDITABLE))
		{
			/* FIXME: manage the case of local readonly files owned by the
			   user is running lapiz - Paolo (Dec. 8, 2005) */
			if (lapiz_document_is_untitled (doc) ||
			    lapiz_document_get_readonly (doc))
			{
				if (document_needs_saving (doc))
			     	{
				     	tabs_to_save_as = g_slist_prepend (tabs_to_save_as,
									   t);
			     	}
			}
			else
			{
				file_save (t, window);
			}
		}
		else
		{
			/* If the state is:
			   - LAPIZ_TAB_STATE_LOADING: we do not save since we are sure the file is unmodified
			   - LAPIZ_TAB_STATE_REVERTING: we do not save since the user wants
			     to return back to the version of the file she previously saved
			   - LAPIZ_TAB_STATE_SAVING: well, we are already saving (no need to save again)
			   - LAPIZ_TAB_STATE_PRINTING, LAPIZ_TAB_STATE_PRINT_PREVIEWING: there is not a
			     real reason for not saving in this case, we do not save to avoid to run
			     two operations using the message area at the same time (may be we can remove
			     this limitation in the future). Note that SaveAll, ClosAll
			     and Quit are unsensitive if the window state is PRINTING.
			   - LAPIZ_TAB_STATE_GENERIC_ERROR: we do not save since the document contains
			     errors (I don't think this is a very frequent case, we should probably remove
			     this state)
			   - LAPIZ_TAB_STATE_LOADING_ERROR: there is nothing to save
			   - LAPIZ_TAB_STATE_REVERTING_ERROR: there is nothing to save and saving the current
			     document will overwrite the copy of the file the user wants to go back to
			   - LAPIZ_TAB_STATE_SAVING_ERROR: we do not save since we just failed to save, so there is
			     no reason to automatically retry... we wait for user intervention
			   - LAPIZ_TAB_STATE_CLOSING: this state is invalid in this case
			*/

			gchar *uri_for_display;

			uri_for_display = lapiz_document_get_uri_for_display (doc);
			lapiz_debug_message (DEBUG_COMMANDS,
					     "File '%s' not saved. State: %d",
					     uri_for_display,
					     state);
			g_free (uri_for_display);
		}

		l = g_list_next (l);
	}

	if (tabs_to_save_as != NULL)
	{
		LapizTab *tab;

		tabs_to_save_as = g_slist_reverse (tabs_to_save_as );

		g_return_if_fail (g_object_get_data (G_OBJECT (window),
						     LAPIZ_LIST_OF_TABS_TO_SAVE_AS) == NULL);

		g_object_set_data (G_OBJECT (window),
				   LAPIZ_LIST_OF_TABS_TO_SAVE_AS,
				   tabs_to_save_as);

		tab = LAPIZ_TAB (tabs_to_save_as->data);

		lapiz_window_set_active_tab (window, tab);
		file_save_as (tab, window);
	}
}

void
lapiz_commands_save_all_documents (LapizWindow *window)
{
	GList *docs;

	g_return_if_fail (LAPIZ_IS_WINDOW (window));

	lapiz_debug (DEBUG_COMMANDS);

	docs = lapiz_window_get_documents (window);

	_lapiz_cmd_file_save_documents_list (window, docs);

	g_list_free (docs);
}

void
_lapiz_cmd_file_save_all (CtkAction   *action,
			 LapizWindow *window)
{
	lapiz_commands_save_all_documents (window);
}

void
lapiz_commands_save_document (LapizWindow   *window,
                              LapizDocument *document)
{
	LapizTab *tab;

	g_return_if_fail (LAPIZ_IS_WINDOW (window));
	g_return_if_fail (LAPIZ_IS_DOCUMENT (document));

	lapiz_debug (DEBUG_COMMANDS);

	tab = lapiz_tab_get_from_document (document);
	file_save (tab, window);
}

/* File revert */
static void
do_revert (LapizWindow *window,
	   LapizTab    *tab)
{
	LapizDocument *doc;
	gchar *docname;

	lapiz_debug (DEBUG_COMMANDS);

	doc = lapiz_tab_get_document (tab);
	docname = lapiz_document_get_short_name_for_display (doc);

	lapiz_statusbar_flash_message (LAPIZ_STATUSBAR (window->priv->statusbar),
				        window->priv->generic_message_cid,
				       _("Reverting the document '%s'\342\200\246"),
				       docname);

	g_free (docname);

	_lapiz_tab_revert (tab);
}

static void
revert_dialog_response_cb (CtkDialog   *dialog,
			   gint         response_id,
			   LapizWindow *window)
{
	LapizTab *tab;

	lapiz_debug (DEBUG_COMMANDS);

	/* FIXME: we are relying on the fact that the dialog is
	   modal so the active tab can't be changed...
	   not very nice - Paolo (Oct 11, 2005) */
	tab = lapiz_window_get_active_tab (window);
	if (tab == NULL)
		return;

	ctk_widget_destroy (CTK_WIDGET (dialog));

	if (response_id == CTK_RESPONSE_OK)
	{
		do_revert (window, tab);
	}
}

static CtkWidget *
revert_dialog (LapizWindow   *window,
	       LapizDocument *doc)
{
	CtkWidget *dialog;
	gchar *docname;
	gchar *primary_msg;
	gchar *secondary_msg;
	glong seconds;

	lapiz_debug (DEBUG_COMMANDS);

	docname = lapiz_document_get_short_name_for_display (doc);
	primary_msg = g_strdup_printf (_("Revert unsaved changes to document '%s'?"),
	                               docname);
	g_free (docname);

	seconds = MAX (1, _lapiz_document_get_seconds_since_last_save_or_load (doc));

	if (seconds < 55)
	{
		secondary_msg = g_strdup_printf (
					ngettext ("Changes made to the document in the last %ld second "
					    	  "will be permanently lost.",
						  "Changes made to the document in the last %ld seconds "
					    	  "will be permanently lost.",
						  seconds),
					seconds);
	}
	else if (seconds < 75) /* 55 <= seconds < 75 */
	{
		secondary_msg = g_strdup (_("Changes made to the document in the last minute "
					    "will be permanently lost."));
	}
	else if (seconds < 110) /* 75 <= seconds < 110 */
	{
		secondary_msg = g_strdup_printf (
					ngettext ("Changes made to the document in the last minute and "
						  "%ld second will be permanently lost.",
						  "Changes made to the document in the last minute and "
						  "%ld seconds will be permanently lost.",
						  seconds - 60 ),
					seconds - 60);
	}
	else if (seconds < 3600)
	{
		secondary_msg = g_strdup_printf (
					ngettext ("Changes made to the document in the last %ld minute "
					    	  "will be permanently lost.",
						  "Changes made to the document in the last %ld minutes "
					    	  "will be permanently lost.",
						  seconds / 60),
					seconds / 60);
	}
	else if (seconds < 7200)
	{
		gint minutes;
		seconds -= 3600;

		minutes = seconds / 60;
		if (minutes < 5)
		{
			secondary_msg = g_strdup (_("Changes made to the document in the last hour "
						    "will be permanently lost."));
		}
		else
		{
			secondary_msg = g_strdup_printf (
					ngettext ("Changes made to the document in the last hour and "
						  "%d minute will be permanently lost.",
						  "Changes made to the document in the last hour and "
						  "%d minutes will be permanently lost.",
						  minutes),
					minutes);
		}
	}
	else
	{
		gint hours;

		hours = seconds / 3600;

		secondary_msg = g_strdup_printf (
					ngettext ("Changes made to the document in the last %d hour "
					    	  "will be permanently lost.",
						  "Changes made to the document in the last %d hours "
					    	  "will be permanently lost.",
						  hours),
					hours);
	}

	dialog = ctk_message_dialog_new (CTK_WINDOW (window),
					 CTK_DIALOG_DESTROY_WITH_PARENT,
					 CTK_MESSAGE_QUESTION,
					 CTK_BUTTONS_NONE,
					 "%s", primary_msg);

	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
						  "%s", secondary_msg);
	g_free (primary_msg);
	g_free (secondary_msg);

	ctk_window_set_resizable (CTK_WINDOW (dialog), FALSE);

	lapiz_dialog_add_button (CTK_DIALOG (dialog),
				 _("_Cancel"),
				 "process-stop",
				 CTK_RESPONSE_CANCEL);

	lapiz_dialog_add_button (CTK_DIALOG (dialog),
				 _("_Revert"),
				 "document-revert",
				 CTK_RESPONSE_OK);

	ctk_dialog_set_default_response	(CTK_DIALOG (dialog),
					 CTK_RESPONSE_CANCEL);

	return dialog;
}

void
_lapiz_cmd_file_revert (CtkAction   *action,
		       LapizWindow *window)
{
	LapizTab       *tab;
	LapizDocument  *doc;
	CtkWidget      *dialog;
	CtkWindowGroup *wg;

	lapiz_debug (DEBUG_COMMANDS);

	tab = lapiz_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	/* If we are already displaying a notification
	 * reverting will drop local modifications, do
	 * not bug the user further */
	if (lapiz_tab_get_state (tab) == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)
	{
		do_revert (window, tab);
		return;
	}

	doc = lapiz_tab_get_document (tab);
	g_return_if_fail (doc != NULL);
	g_return_if_fail (!lapiz_document_is_untitled (doc));

	dialog = revert_dialog (window, doc);

	wg = lapiz_window_get_group (window);

	ctk_window_group_add_window (wg, CTK_WINDOW (dialog));

	ctk_window_set_modal (CTK_WINDOW (dialog), TRUE);

	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (revert_dialog_response_cb),
			  window);

	ctk_widget_show (dialog);
}

/* Close tab */
static gboolean
really_close_tab (LapizTab *tab)
{
	CtkWidget *toplevel;
	LapizWindow *window;

	lapiz_debug (DEBUG_COMMANDS);

	g_return_val_if_fail (lapiz_tab_get_state (tab) == LAPIZ_TAB_STATE_CLOSING,
			      FALSE);

	toplevel = ctk_widget_get_toplevel (CTK_WIDGET (tab));
	g_return_val_if_fail (LAPIZ_IS_WINDOW (toplevel), FALSE);

	window = LAPIZ_WINDOW (toplevel);

	lapiz_window_close_tab (window, tab);

	if (lapiz_window_get_active_tab (window) == NULL)
	{
		gboolean is_quitting;

		is_quitting = GPOINTER_TO_BOOLEAN (g_object_get_data (G_OBJECT (window),
								      LAPIZ_IS_QUITTING));

		if (is_quitting)
			ctk_widget_destroy (CTK_WIDGET (window));
	}

	return FALSE;
}

static void
tab_state_changed_while_saving (LapizTab    *tab,
				GParamSpec  *pspec,
				LapizWindow *window)
{
	LapizTabState ts;

	ts = lapiz_tab_get_state (tab);

	lapiz_debug_message (DEBUG_COMMANDS, "State while saving: %d\n", ts);

	/* When the state become NORMAL, it means the saving operation is
	   finished */
	if (ts == LAPIZ_TAB_STATE_NORMAL)
	{
		LapizDocument *doc;

		g_signal_handlers_disconnect_by_func (tab,
						      G_CALLBACK (tab_state_changed_while_saving),
					      	      window);

		doc = lapiz_tab_get_document (tab);
		g_return_if_fail (doc != NULL);

		/* If the saving operation failed or was interrupted, then the
		   document is still "modified" -> do not close the tab */
		if (document_needs_saving (doc))
			return;

		/* Close the document only if it has been succesfully saved.
		   Tab state is set to CLOSING (it is a state without exiting
		   transitions) and the tab is closed in a idle handler */
		_lapiz_tab_mark_for_closing (tab);

		g_idle_add_full (G_PRIORITY_HIGH_IDLE,
				 (GSourceFunc)really_close_tab,
				 tab,
				 NULL);
	}
}

static void
save_and_close (LapizTab    *tab,
		LapizWindow *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	/* Trace tab state changes */
	g_signal_connect (tab,
			  "notify::state",
			  G_CALLBACK (tab_state_changed_while_saving),
			  window);

	file_save (tab, window);
}

static void
save_as_and_close (LapizTab    *tab,
		   LapizWindow *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	g_object_set_data (G_OBJECT (tab),
			   LAPIZ_IS_CLOSING_TAB,
			   NULL);

	/* Trace tab state changes */
	g_signal_connect (tab,
			  "notify::state",
			  G_CALLBACK (tab_state_changed_while_saving),
			  window);

	lapiz_window_set_active_tab (window, tab);
	file_save_as (tab, window);
}

static void
save_and_close_all_documents (const GList  *docs,
			      LapizWindow  *window)
{
	GList  *tabs;
	GList  *l;
	GSList *sl;
	GSList *tabs_to_save_as;
	GSList *tabs_to_save_and_close;
	GList  *tabs_to_close;

	lapiz_debug (DEBUG_COMMANDS);

	g_return_if_fail (!(lapiz_window_get_state (window) & LAPIZ_WINDOW_STATE_PRINTING));

	tabs = ctk_container_get_children (
			CTK_CONTAINER (_lapiz_window_get_notebook (window)));

	tabs_to_save_as = NULL;
	tabs_to_save_and_close = NULL;
	tabs_to_close = NULL;

	l = tabs;
	while (l != NULL)
	{
		LapizTab *t;
		LapizTabState state;
		LapizDocument *doc;

		t = LAPIZ_TAB (l->data);

		state = lapiz_tab_get_state (t);
		doc = lapiz_tab_get_document (t);

		/* If the state is: ([*] invalid states)
		   - LAPIZ_TAB_STATE_NORMAL: close (and if needed save)
		   - LAPIZ_TAB_STATE_LOADING: close, we are sure the file is unmodified
		   - LAPIZ_TAB_STATE_REVERTING: since the user wants
		     to return back to the version of the file she previously saved, we can close
		     without saving (CHECK: are we sure this is the right behavior, suppose the case
		     the original file has been deleted)
		   - [*] LAPIZ_TAB_STATE_SAVING: invalid, ClosAll
		     and Quit are unsensitive if the window state is SAVING.
		   - [*] LAPIZ_TAB_STATE_PRINTING, LAPIZ_TAB_STATE_PRINT_PREVIEWING: there is not a
		     real reason for not closing in this case, we do not save to avoid to run
		     two operations using the message area at the same time (may be we can remove
		     this limitation in the future). Note that ClosAll
		     and Quit are unsensitive if the window state is PRINTING.
		   - LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW: close (and if needed save)
		   - LAPIZ_TAB_STATE_LOADING_ERROR: close without saving (if the state is LOADING_ERROR then the
		     document is not modified)
		   - LAPIZ_TAB_STATE_REVERTING_ERROR: we do not close since the document contains errors
		   - LAPIZ_TAB_STATE_SAVING_ERROR: we do not close since the document contains errors
		   - LAPIZ_TAB_STATE_GENERIC_ERROR: we do not close since the document contains
		     errors (CHECK: we should problably remove this state)
		   - [*] LAPIZ_TAB_STATE_CLOSING: this state is invalid in this case
		*/

		g_return_if_fail (state != LAPIZ_TAB_STATE_PRINTING);
		g_return_if_fail (state != LAPIZ_TAB_STATE_PRINT_PREVIEWING);
		g_return_if_fail (state != LAPIZ_TAB_STATE_CLOSING);
		g_return_if_fail (state != LAPIZ_TAB_STATE_SAVING);

		if ((state != LAPIZ_TAB_STATE_SAVING_ERROR) &&
		    (state != LAPIZ_TAB_STATE_GENERIC_ERROR) &&
		    (state != LAPIZ_TAB_STATE_REVERTING_ERROR))
		{
			if ((g_list_index ((GList *)docs, doc) >= 0) &&
			    (state != LAPIZ_TAB_STATE_LOADING) &&
			    (state != LAPIZ_TAB_STATE_LOADING_ERROR) &&
			    (state != LAPIZ_TAB_STATE_REVERTING)) /* CHECK: is this the right behavior with REVERTING ?*/
			{
				/* The document must be saved before closing */
				g_return_if_fail (document_needs_saving (doc));

				/* FIXME: manage the case of local readonly files owned by the
				   user is running lapiz - Paolo (Dec. 8, 2005) */
				if (lapiz_document_is_untitled (doc) ||
				    lapiz_document_get_readonly (doc))
				{
					g_object_set_data (G_OBJECT (t),
							   LAPIZ_IS_CLOSING_TAB,
							   GBOOLEAN_TO_POINTER (TRUE));

					tabs_to_save_as = g_slist_prepend (tabs_to_save_as,
									   t);
				}
				else
				{
					tabs_to_save_and_close = g_slist_prepend (tabs_to_save_and_close,
										  t);
				}
			}
			else
			{
				/* The document must be closed without saving */
				tabs_to_close = g_list_prepend (tabs_to_close,
								t);
			}
		}

		l = g_list_next (l);
	}

	g_list_free (tabs);

	/* Close all tabs to close (in a sync way) */
	lapiz_window_close_tabs (window, tabs_to_close);
	g_list_free (tabs_to_close);

	/* Save and close all the files in tabs_to_save_and_close */
	sl = tabs_to_save_and_close;
	while (sl != NULL)
	{
		save_and_close (LAPIZ_TAB (sl->data),
				window);
		sl = g_slist_next (sl);
	}
	g_slist_free (tabs_to_save_and_close);

	/* Save As and close all the files in tabs_to_save_as  */
	if (tabs_to_save_as != NULL)
	{
		LapizTab *tab;

		tabs_to_save_as = g_slist_reverse (tabs_to_save_as );

		g_return_if_fail (g_object_get_data (G_OBJECT (window),
						     LAPIZ_LIST_OF_TABS_TO_SAVE_AS) == NULL);

		g_object_set_data (G_OBJECT (window),
				   LAPIZ_LIST_OF_TABS_TO_SAVE_AS,
				   tabs_to_save_as);

		tab = LAPIZ_TAB (tabs_to_save_as->data);

		save_as_and_close (tab, window);
	}
}

static void
save_and_close_document (const GList  *docs,
			 LapizWindow  *window)
{
	LapizTab *tab;

	lapiz_debug (DEBUG_COMMANDS);

	g_return_if_fail (docs->next == NULL);

	tab = lapiz_tab_get_from_document (LAPIZ_DOCUMENT (docs->data));
	g_return_if_fail (tab != NULL);

	save_and_close (tab, window);
}

static void
close_all_tabs (LapizWindow *window)
{
	gboolean is_quitting;

	lapiz_debug (DEBUG_COMMANDS);

	/* There is no document to save -> close all tabs */
	lapiz_window_close_all_tabs (window);

	is_quitting = GPOINTER_TO_BOOLEAN (g_object_get_data (G_OBJECT (window),
							      LAPIZ_IS_QUITTING));

	if (is_quitting)
		ctk_widget_destroy (CTK_WIDGET (window));

	return;
}

static void
close_document (LapizWindow   *window,
		LapizDocument *doc)
{
	LapizTab *tab;

	lapiz_debug (DEBUG_COMMANDS);

	tab = lapiz_tab_get_from_document (doc);
	g_return_if_fail (tab != NULL);

	lapiz_window_close_tab (window, tab);
}

static void
close_confirmation_dialog_response_handler (LapizCloseConfirmationDialog *dlg,
					    gint                          response_id,
					    LapizWindow                  *window)
{
	GList *selected_documents;
	gboolean is_closing_all;

	lapiz_debug (DEBUG_COMMANDS);

	is_closing_all = GPOINTER_TO_BOOLEAN (g_object_get_data (G_OBJECT (window),
					    			 LAPIZ_IS_CLOSING_ALL));

	ctk_widget_hide (CTK_WIDGET (dlg));

	switch (response_id)
	{
		case CTK_RESPONSE_YES: /* Save and Close */
			selected_documents = lapiz_close_confirmation_dialog_get_selected_documents (dlg);
			if (selected_documents == NULL)
			{
				if (is_closing_all)
				{
					/* There is no document to save -> close all tabs */
					/* We call ctk_widget_destroy before close_all_tabs
					 * because close_all_tabs could destroy the lapiz window */
					ctk_widget_destroy (CTK_WIDGET (dlg));

					close_all_tabs (window);

					return;
				}
				else
					g_return_if_reached ();
			}
			else
			{
				if (is_closing_all)
				{
					save_and_close_all_documents (selected_documents,
								      window);
				}
				else
				{
					save_and_close_document (selected_documents,
								 window);
				}
			}

			g_list_free (selected_documents);

			break;

		case CTK_RESPONSE_NO: /* Close without Saving */
			if (is_closing_all)
			{
				/* We call ctk_widget_destroy before close_all_tabs
				 * because close_all_tabs could destroy the lapiz window */
				ctk_widget_destroy (CTK_WIDGET (dlg));

				close_all_tabs (window);

				return;
			}
			else
			{
				const GList *unsaved_documents;

				unsaved_documents = lapiz_close_confirmation_dialog_get_unsaved_documents (dlg);
				g_return_if_fail (unsaved_documents->next == NULL);

				close_document (window,
						LAPIZ_DOCUMENT (unsaved_documents->data));
			}

			break;
		default: /* Do not close */

			/* Reset is_quitting flag */
			g_object_set_data (G_OBJECT (window),
					   LAPIZ_IS_QUITTING,
					   GBOOLEAN_TO_POINTER (FALSE));

			break;
	}

	ctk_widget_destroy (CTK_WIDGET (dlg));
}

/* Returns TRUE if the tab can be immediately closed */
static gboolean
tab_can_close (LapizTab  *tab,
	       CtkWindow *window)
{
	LapizDocument *doc;

	lapiz_debug (DEBUG_COMMANDS);

	doc = lapiz_tab_get_document (tab);

	if (!_lapiz_tab_can_close (tab))
	{
		CtkWidget     *dlg;

		dlg = lapiz_close_confirmation_dialog_new_single (
						window,
						doc,
						FALSE);

		g_signal_connect (dlg,
				  "response",
				  G_CALLBACK (close_confirmation_dialog_response_handler),
				  window);

		ctk_widget_show (dlg);

		return FALSE;
	}

	return TRUE;
}

/* CHECK: we probably need this one public for plugins...
 * maybe even a _list variant. Or maybe it's better make
 * lapiz_window_close_tab always run the confirm dialog?
 * we should not allow closing a tab without resetting the
 * LAPIZ_IS_CLOSING_ALL flag!
 */
void
_lapiz_cmd_file_close_tab (LapizTab    *tab,
			   LapizWindow *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	g_return_if_fail (CTK_WIDGET (window) == ctk_widget_get_toplevel (CTK_WIDGET (tab)));

	g_object_set_data (G_OBJECT (window),
			   LAPIZ_IS_CLOSING_ALL,
			   GBOOLEAN_TO_POINTER (FALSE));

	g_object_set_data (G_OBJECT (window),
			   LAPIZ_IS_QUITTING,
			   GBOOLEAN_TO_POINTER (FALSE));

	g_object_set_data (G_OBJECT (window),
	                   LAPIZ_IS_QUITTING_ALL,
	                   GINT_TO_POINTER (FALSE));


	if (tab_can_close (tab, CTK_WINDOW (window)))
		lapiz_window_close_tab (window, tab);
}

void
_lapiz_cmd_file_close (CtkAction   *action,
		      LapizWindow *window)
{
	LapizTab *active_tab;

	lapiz_debug (DEBUG_COMMANDS);

	active_tab = lapiz_window_get_active_tab (window);

	if (active_tab == NULL)
	{
		return;
	}

	_lapiz_cmd_file_close_tab (active_tab, window);
}

/* Close all tabs */
static void
file_close_all (LapizWindow *window,
		gboolean     is_quitting)
{
	GList     *unsaved_docs;
	CtkWidget *dlg;

	lapiz_debug (DEBUG_COMMANDS);

	g_return_if_fail (!(lapiz_window_get_state (window) &
	                    (LAPIZ_WINDOW_STATE_SAVING |
	                     LAPIZ_WINDOW_STATE_PRINTING |
	                     LAPIZ_WINDOW_STATE_SAVING_SESSION)));

	g_object_set_data (G_OBJECT (window),
			   LAPIZ_IS_CLOSING_ALL,
			   GBOOLEAN_TO_POINTER (TRUE));

	g_object_set_data (G_OBJECT (window),
			   LAPIZ_IS_QUITTING,
			   GBOOLEAN_TO_POINTER (is_quitting));

	unsaved_docs = lapiz_window_get_unsaved_documents (window);

	if (unsaved_docs == NULL)
	{
		/* There is no document to save -> close all tabs */
		lapiz_window_close_all_tabs (window);

		if (is_quitting)
			ctk_widget_destroy (CTK_WIDGET (window));

		return;
	}

	if (unsaved_docs->next == NULL)
	{
		/* There is only one unsaved document */
		LapizTab      *tab;
		LapizDocument *doc;

		doc = LAPIZ_DOCUMENT (unsaved_docs->data);

		tab = lapiz_tab_get_from_document (doc);
		g_return_if_fail (tab != NULL);

		lapiz_window_set_active_tab (window, tab);

		dlg = lapiz_close_confirmation_dialog_new_single (
						CTK_WINDOW (window),
						doc,
						FALSE);
	}
	else
	{
		dlg = lapiz_close_confirmation_dialog_new (CTK_WINDOW (window),
							   unsaved_docs,
							   FALSE);
	}

	g_list_free (unsaved_docs);

	g_signal_connect (dlg,
			  "response",
			  G_CALLBACK (close_confirmation_dialog_response_handler),
			  window);

	ctk_widget_show (dlg);
}

void
_lapiz_cmd_file_close_all (CtkAction   *action,
			  LapizWindow *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	g_return_if_fail (!(lapiz_window_get_state (window) &
	                    (LAPIZ_WINDOW_STATE_SAVING |
	                    LAPIZ_WINDOW_STATE_PRINTING |
	                    LAPIZ_WINDOW_STATE_SAVING_SESSION)));

	file_close_all (window, FALSE);
}

void
_lapiz_cmd_file_quit (CtkAction   *action,
		     LapizWindow *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	g_return_if_fail (!(lapiz_window_get_state (window) &
	                    (LAPIZ_WINDOW_STATE_SAVING |
	                     LAPIZ_WINDOW_STATE_PRINTING |
	                     LAPIZ_WINDOW_STATE_SAVING_SESSION)));

	file_close_all (window, TRUE);
}
