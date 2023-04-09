/*
 * lapiz-file-chooser-dialog.c
 * This file is part of lapiz
 *
 * Copyright (C) 2005-2007 - Paolo Maggi
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
 * Modified by the lapiz Team, 2005-2007. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

/* TODO: Override set_extra_widget */
/* TODO: add encoding property */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <ctksourceview/ctksource.h>

#include "lapiz-file-chooser-dialog.h"
#include "lapiz-encodings-combo-box.h"
#include "lapiz-language-manager.h"
#include "lapiz-prefs-manager-app.h"
#include "lapiz-debug.h"
#include "lapiz-enum-types.h"
#include "lapiz-utils.h"

#define ALL_FILES		_("All Files")
#define ALL_TEXT_FILES		_("All Text Files")

struct _LapizFileChooserDialogPrivate
{
	GtkWidget *option_menu;
	GtkWidget *extra_widget;

	GtkWidget *newline_label;
	GtkWidget *newline_combo;
	GtkListStore *newline_store;
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizFileChooserDialog, lapiz_file_chooser_dialog, CTK_TYPE_FILE_CHOOSER_DIALOG)

static void
lapiz_file_chooser_dialog_class_init (LapizFileChooserDialogClass *klass)
{
}

static void
create_option_menu (LapizFileChooserDialog *dialog)
{
	GtkWidget *label;
	GtkWidget *menu;

	label = ctk_label_new_with_mnemonic (_("C_haracter Encoding:"));
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);

	menu = lapiz_encodings_combo_box_new (
		ctk_file_chooser_get_action (CTK_FILE_CHOOSER (dialog)) == CTK_FILE_CHOOSER_ACTION_SAVE);

	ctk_label_set_mnemonic_widget (CTK_LABEL (label), menu);

	ctk_box_pack_start (CTK_BOX (dialog->priv->extra_widget),
	                    label,
	                    FALSE,
	                    TRUE,
	                    0);

	ctk_box_pack_start (CTK_BOX (dialog->priv->extra_widget),
	                    menu,
	                    TRUE,
	                    TRUE,
	                    0);

	ctk_widget_show (label);
	ctk_widget_show (menu);

	dialog->priv->option_menu = menu;
}

static void
update_newline_visibility (LapizFileChooserDialog *dialog)
{
	if (ctk_file_chooser_get_action (CTK_FILE_CHOOSER (dialog)) == CTK_FILE_CHOOSER_ACTION_SAVE)
	{
		ctk_widget_show (dialog->priv->newline_label);
		ctk_widget_show (dialog->priv->newline_combo);
	}
	else
	{
		ctk_widget_hide (dialog->priv->newline_label);
		ctk_widget_hide (dialog->priv->newline_combo);
	}
}

static void
newline_combo_append (GtkComboBox              *combo,
                      GtkListStore             *store,
                      GtkTreeIter              *iter,
                      const gchar              *label,
                      LapizDocumentNewlineType  newline_type)
{
	ctk_list_store_append (store, iter);
	ctk_list_store_set (store, iter, 0, label, 1, newline_type, -1);

	if (newline_type == LAPIZ_DOCUMENT_NEWLINE_TYPE_DEFAULT)
	{
		ctk_combo_box_set_active_iter (combo, iter);
	}
}

static void
create_newline_combo (LapizFileChooserDialog *dialog)
{
	GtkWidget *label, *combo;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeIter iter;

	label = ctk_label_new_with_mnemonic (_("L_ine Ending:"));
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);

	store = ctk_list_store_new (2, G_TYPE_STRING, LAPIZ_TYPE_DOCUMENT_NEWLINE_TYPE);
	combo = ctk_combo_box_new_with_model (CTK_TREE_MODEL (store));
	renderer = ctk_cell_renderer_text_new ();

	ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (combo),
	                            renderer,
	                            TRUE);

	ctk_cell_layout_add_attribute (CTK_CELL_LAYOUT (combo),
	                               renderer,
	                               "text",
	                               0);

	newline_combo_append (CTK_COMBO_BOX (combo),
	                      store,
	                      &iter,
	                      _("Unix/Linux"),
	                      LAPIZ_DOCUMENT_NEWLINE_TYPE_LF);

	newline_combo_append (CTK_COMBO_BOX (combo),
	                      store,
	                      &iter,
	                      _("Mac OS Classic"),
	                      LAPIZ_DOCUMENT_NEWLINE_TYPE_CR);

	newline_combo_append (CTK_COMBO_BOX (combo),
	                      store,
	                      &iter,
	                      _("Windows"),
	                      LAPIZ_DOCUMENT_NEWLINE_TYPE_CR_LF);

	ctk_label_set_mnemonic_widget (CTK_LABEL (label), combo);

	ctk_box_pack_start (CTK_BOX (dialog->priv->extra_widget),
	                    label,
	                    FALSE,
	                    TRUE,
	                    0);

	ctk_box_pack_start (CTK_BOX (dialog->priv->extra_widget),
	                    combo,
	                    TRUE,
	                    TRUE,
	                    0);

	dialog->priv->newline_combo = combo;
	dialog->priv->newline_label = label;
	dialog->priv->newline_store = store;

	update_newline_visibility (dialog);
}

static void
create_extra_widget (LapizFileChooserDialog *dialog)
{
	dialog->priv->extra_widget = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);

	ctk_widget_show (dialog->priv->extra_widget);

	create_option_menu (dialog);
	create_newline_combo (dialog);

	ctk_file_chooser_set_extra_widget (CTK_FILE_CHOOSER (dialog),
					   dialog->priv->extra_widget);

}

static void
action_changed (LapizFileChooserDialog *dialog,
		GParamSpec	       *pspec,
		gpointer		data)
{
	GtkFileChooserAction action;

	action = ctk_file_chooser_get_action (CTK_FILE_CHOOSER (dialog));

	switch (action)
	{
		case CTK_FILE_CHOOSER_ACTION_OPEN:
			g_object_set (dialog->priv->option_menu,
				      "save_mode", FALSE,
				      NULL);
			ctk_widget_show (dialog->priv->option_menu);
			break;
		case CTK_FILE_CHOOSER_ACTION_SAVE:
			g_object_set (dialog->priv->option_menu,
				      "save_mode", TRUE,
				      NULL);
			ctk_widget_show (dialog->priv->option_menu);
			break;
		default:
			ctk_widget_hide (dialog->priv->option_menu);
	}

	update_newline_visibility (dialog);
}

static void
filter_changed (LapizFileChooserDialog *dialog,
		GParamSpec	       *pspec,
		gpointer		data)
{
	GtkFileFilter *filter;

	if (!lapiz_prefs_manager_active_file_filter_can_set ())
		return;

	filter = ctk_file_chooser_get_filter (CTK_FILE_CHOOSER (dialog));
	if (filter != NULL)
	{
		const gchar *name;
		gint id = 0;

		name = ctk_file_filter_get_name (filter);
		g_return_if_fail (name != NULL);

		if (strcmp (name, ALL_TEXT_FILES) == 0)
			id = 1;

		lapiz_debug_message (DEBUG_COMMANDS, "Active filter: %s (%d)", name, id);

		lapiz_prefs_manager_set_active_file_filter (id);
	}
}

/* FIXME: use globs too - Paolo (Aug. 27, 2007) */
static gboolean
all_text_files_filter (const GtkFileFilterInfo *filter_info,
		       gpointer                 data)
{
	static GSList *known_mime_types = NULL;
	GSList *mime_types;

	if (known_mime_types == NULL)
	{
		GtkSourceLanguageManager *lm;
		const gchar * const *languages;

		lm = lapiz_get_language_manager ();
		languages = ctk_source_language_manager_get_language_ids (lm);

		while ((languages != NULL) && (*languages != NULL))
		{
			gchar **mime_types;
			gint i;
			GtkSourceLanguage *lang;

			lang = ctk_source_language_manager_get_language (lm, *languages);
			g_return_val_if_fail (CTK_SOURCE_IS_LANGUAGE (lang), FALSE);
			++languages;

			mime_types = ctk_source_language_get_mime_types (lang);
			if (mime_types == NULL)
				continue;

			for (i = 0; mime_types[i] != NULL; i++)
			{
				if (!g_content_type_is_a (mime_types[i], "text/plain"))
				{
					lapiz_debug_message (DEBUG_COMMANDS,
							     "Mime-type %s is not related to text/plain",
							     mime_types[i]);

					known_mime_types = g_slist_prepend (known_mime_types,
									    g_strdup (mime_types[i]));
				}
			}

			g_strfreev (mime_types);
		}

		/* known_mime_types always has "text/plain" as first item" */
		known_mime_types = g_slist_prepend (known_mime_types, g_strdup ("text/plain"));
	}

	/* known mime_types contains "text/plain" and then the list of mime-types unrelated to "text/plain"
	 * that lapiz recognizes */

	if (filter_info->mime_type == NULL)
		return FALSE;

	/*
	 * The filter is matching:
	 * - the mime-types beginning with "text/"
	 * - the mime-types inheriting from a known mime-type (note the text/plain is
	 *   the first known mime-type)
	 */

	if (strncmp (filter_info->mime_type, "text/", 5) == 0)
		return TRUE;

	mime_types = known_mime_types;
	while (mime_types != NULL)
	{
		if (g_content_type_is_a (filter_info->mime_type, (const gchar*)mime_types->data))
			return TRUE;

		mime_types = g_slist_next (mime_types);
	}

	return FALSE;
}

static void
lapiz_file_chooser_dialog_init (LapizFileChooserDialog *dialog)
{
	dialog->priv = lapiz_file_chooser_dialog_get_instance_private (dialog);
}

static GtkWidget *
lapiz_file_chooser_dialog_new_valist (const gchar          *title,
				      GtkWindow            *parent,
				      GtkFileChooserAction  action,
				      const LapizEncoding  *encoding,
				      const gchar          *first_button_text,
				      va_list               varargs)
{
	GtkWidget *result;
	const char *button_text = first_button_text;
	gint response_id;
	GtkFileFilter *filter;
	gint active_filter;

	g_return_val_if_fail (parent != NULL, NULL);

	result = g_object_new (LAPIZ_TYPE_FILE_CHOOSER_DIALOG,
			       "title", title,
			       "local-only", FALSE,
			       "action", action,
			       "select-multiple", action == CTK_FILE_CHOOSER_ACTION_OPEN,
			       NULL);

	create_extra_widget (LAPIZ_FILE_CHOOSER_DIALOG (result));

	g_signal_connect (result,
			  "notify::action",
			  G_CALLBACK (action_changed),
			  NULL);

	if (encoding != NULL)
		lapiz_encodings_combo_box_set_selected_encoding (
				LAPIZ_ENCODINGS_COMBO_BOX (LAPIZ_FILE_CHOOSER_DIALOG (result)->priv->option_menu),
				encoding);

	active_filter = lapiz_prefs_manager_get_active_file_filter ();
	lapiz_debug_message (DEBUG_COMMANDS, "Active filter: %d", active_filter);

	/* Filters */
	filter = ctk_file_filter_new ();

	ctk_file_filter_set_name (filter, ALL_FILES);
	ctk_file_filter_add_pattern (filter, "*");
	ctk_file_chooser_add_filter (CTK_FILE_CHOOSER (result), filter);
	ctk_file_chooser_set_action (CTK_FILE_CHOOSER (result), action);

	if (active_filter != 1)
	{
		/* Make this filter the default */
		ctk_file_chooser_set_filter (CTK_FILE_CHOOSER (result), filter);
	}

	filter = ctk_file_filter_new ();
	ctk_file_filter_set_name (filter, ALL_TEXT_FILES);
	ctk_file_filter_add_custom (filter,
				    CTK_FILE_FILTER_MIME_TYPE,
				    all_text_files_filter,
				    NULL,
				    NULL);
	ctk_file_chooser_add_filter (CTK_FILE_CHOOSER (result), filter);

	if (active_filter == 1)
	{
		/* Make this filter the default */
		ctk_file_chooser_set_filter (CTK_FILE_CHOOSER (result), filter);
	}

	g_signal_connect (result,
			  "notify::filter",
			  G_CALLBACK (filter_changed),
			  NULL);

	ctk_window_set_transient_for (CTK_WINDOW (result), parent);
	ctk_window_set_destroy_with_parent (CTK_WINDOW (result), TRUE);

	while (button_text)
	{
		response_id = va_arg (varargs, gint);

		if (g_strcmp0 (button_text, "process-stop") == 0)
			lapiz_dialog_add_button (CTK_DIALOG (result), _("_Cancel"), button_text, response_id);
		else if (g_strcmp0 (button_text, "document-open") == 0)
			lapiz_dialog_add_button (CTK_DIALOG (result), _("_Open"), button_text, response_id);
		else if (g_strcmp0 (button_text, "document-save") == 0)
			lapiz_dialog_add_button (CTK_DIALOG (result), _("_Save"), button_text, response_id);
		else
			ctk_dialog_add_button (CTK_DIALOG (result), button_text, response_id);

		if ((response_id == CTK_RESPONSE_OK) ||
		    (response_id == CTK_RESPONSE_ACCEPT) ||
		    (response_id == CTK_RESPONSE_YES) ||
		    (response_id == CTK_RESPONSE_APPLY))
			ctk_dialog_set_default_response (CTK_DIALOG (result), response_id);

		button_text = va_arg (varargs, const gchar *);
	}

	return result;
}

/**
 * lapiz_file_chooser_dialog_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog
 * @first_button_text: (allow-none): icon name or text to go in
 * the first button, or %NULL
 * @...: (allow-none): response ID for the first button, then
 * additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #LapizFileChooserDialog.  This function is analogous to
 * ctk_dialog_new_with_buttons().
 *
 * Return value: a new #LapizFileChooserDialog
 *
 **/
GtkWidget *
lapiz_file_chooser_dialog_new (const gchar          *title,
			       GtkWindow            *parent,
			       GtkFileChooserAction  action,
			       const LapizEncoding  *encoding,
			       const gchar          *first_button_text,
			       ...)
{
	GtkWidget *result;
	va_list varargs;

	va_start (varargs, first_button_text);
	result = lapiz_file_chooser_dialog_new_valist (title, parent, action,
						       encoding, first_button_text,
						       varargs);
	va_end (varargs);

	return result;
}

void
lapiz_file_chooser_dialog_set_encoding (LapizFileChooserDialog *dialog,
					const LapizEncoding    *encoding)
{
	g_return_if_fail (LAPIZ_IS_FILE_CHOOSER_DIALOG (dialog));
	g_return_if_fail (LAPIZ_IS_ENCODINGS_COMBO_BOX (dialog->priv->option_menu));

	lapiz_encodings_combo_box_set_selected_encoding (
				LAPIZ_ENCODINGS_COMBO_BOX (dialog->priv->option_menu),
				encoding);
}

const LapizEncoding *
lapiz_file_chooser_dialog_get_encoding (LapizFileChooserDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_FILE_CHOOSER_DIALOG (dialog), NULL);
	g_return_val_if_fail (LAPIZ_IS_ENCODINGS_COMBO_BOX (dialog->priv->option_menu), NULL);
	g_return_val_if_fail ((ctk_file_chooser_get_action (CTK_FILE_CHOOSER (dialog)) == CTK_FILE_CHOOSER_ACTION_OPEN ||
			       ctk_file_chooser_get_action (CTK_FILE_CHOOSER (dialog)) == CTK_FILE_CHOOSER_ACTION_SAVE), NULL);

	return lapiz_encodings_combo_box_get_selected_encoding (
				LAPIZ_ENCODINGS_COMBO_BOX (dialog->priv->option_menu));
}

void
lapiz_file_chooser_dialog_set_newline_type (LapizFileChooserDialog  *dialog,
					    LapizDocumentNewlineType newline_type)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	g_return_if_fail (LAPIZ_IS_FILE_CHOOSER_DIALOG (dialog));
	g_return_if_fail (ctk_file_chooser_get_action (CTK_FILE_CHOOSER (dialog)) == CTK_FILE_CHOOSER_ACTION_SAVE);

	model = CTK_TREE_MODEL (dialog->priv->newline_store);

	if (!ctk_tree_model_get_iter_first (model, &iter))
	{
		return;
	}

	do
	{
		LapizDocumentNewlineType nt;

		ctk_tree_model_get (model, &iter, 1, &nt, -1);

		if (newline_type == nt)
		{
			ctk_combo_box_set_active_iter (CTK_COMBO_BOX (dialog->priv->newline_combo),
			                               &iter);
			break;
		}
	} while (ctk_tree_model_iter_next (model, &iter));
}

LapizDocumentNewlineType
lapiz_file_chooser_dialog_get_newline_type (LapizFileChooserDialog *dialog)
{
	GtkTreeIter iter;
	LapizDocumentNewlineType newline_type;

	g_return_val_if_fail (LAPIZ_IS_FILE_CHOOSER_DIALOG (dialog), LAPIZ_DOCUMENT_NEWLINE_TYPE_DEFAULT);
	g_return_val_if_fail (ctk_file_chooser_get_action (CTK_FILE_CHOOSER (dialog)) == CTK_FILE_CHOOSER_ACTION_SAVE,
	                      LAPIZ_DOCUMENT_NEWLINE_TYPE_DEFAULT);

	ctk_combo_box_get_active_iter (CTK_COMBO_BOX (dialog->priv->newline_combo),
	                               &iter);

	ctk_tree_model_get (CTK_TREE_MODEL (dialog->priv->newline_store),
	                    &iter,
	                    1,
	                    &newline_type,
	                    -1);

	return newline_type;
}
