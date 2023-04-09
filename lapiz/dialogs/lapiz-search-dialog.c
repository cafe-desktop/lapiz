/*
 * lapiz-search-dialog.c
 * This file is part of lapiz
 *
 * Copyright (C) 2005 Paolo Maggi
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
 * Modified by the lapiz Team, 2005. See the AUTHORS file for a
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
#include <gdk/gdkkeysyms.h>

#include "lapiz-search-dialog.h"
#include "lapiz-history-entry.h"
#include "lapiz-utils.h"
#include "lapiz-marshal.h"
#include "lapiz-dirs.h"

/* Signals */
enum
{
	SHOW_REPLACE,
	LAST_SIGNAL
};

static guint dialog_signals [LAST_SIGNAL] = { 0 };

struct _LapizSearchDialogPrivate
{
	gboolean   show_replace;

	GtkWidget *grid;
	GtkWidget *search_label;
	GtkWidget *search_entry;
	GtkWidget *search_text_entry;
	GtkWidget *replace_label;
	GtkWidget *replace_entry;
	GtkWidget *replace_text_entry;
	GtkWidget *match_case_checkbutton;
	GtkWidget *match_regex_checkbutton;
	GtkWidget *entire_word_checkbutton;
	GtkWidget *backwards_checkbutton;
	GtkWidget *wrap_around_checkbutton;
	GtkWidget *parse_escapes_checkbutton;
	GtkWidget *find_button;
	GtkWidget *replace_button;
	GtkWidget *replace_all_button;

	gboolean   ui_error;
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizSearchDialog, lapiz_search_dialog, CTK_TYPE_DIALOG)

enum
{
	PROP_0,
	PROP_SHOW_REPLACE
};

static void
lapiz_search_dialog_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	LapizSearchDialog *dlg = LAPIZ_SEARCH_DIALOG (object);

	switch (prop_id)
	{
		case PROP_SHOW_REPLACE:
			lapiz_search_dialog_set_show_replace (dlg,
							      g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_search_dialog_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
	LapizSearchDialog *dlg = LAPIZ_SEARCH_DIALOG (object);

	switch (prop_id)
	{
		case PROP_SHOW_REPLACE:
			g_value_set_boolean (value, dlg->priv->show_replace);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

void
lapiz_search_dialog_present_with_time (LapizSearchDialog *dialog,
				       guint32            timestamp)
{
	g_return_if_fail (LAPIZ_SEARCH_DIALOG (dialog));

	ctk_window_present_with_time (CTK_WINDOW (dialog), timestamp);

	ctk_widget_grab_focus (dialog->priv->search_text_entry);
}

static gboolean
show_replace (LapizSearchDialog *dlg)
{
	lapiz_search_dialog_set_show_replace (dlg, TRUE);

	return TRUE;
}

static void
lapiz_search_dialog_class_init (LapizSearchDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkBindingSet *binding_set;

	object_class->set_property = lapiz_search_dialog_set_property;
	object_class->get_property = lapiz_search_dialog_get_property;

	klass->show_replace = show_replace;

	dialog_signals[SHOW_REPLACE] =
    		g_signal_new ("show_replace",
		  	      G_TYPE_FROM_CLASS (object_class),
		  	      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  	      G_STRUCT_OFFSET (LapizSearchDialogClass, show_replace),
			      NULL, NULL,
			      lapiz_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN, 0);

	g_object_class_install_property (object_class, PROP_SHOW_REPLACE,
					 g_param_spec_boolean ("show-replace",
					 		       "Show Replace",
					 		       "Whether the dialog is used for Search&Replace",
					 		       FALSE,
					 		       G_PARAM_READWRITE));

	binding_set = ctk_binding_set_by_class (klass);

	/* Note: we cannot use the keyval/modifier associated with the
	 * CTK_STOCK_FIND_AND_REPLACE stock item since CAFE HIG suggests Ctrl+h
	 * for Replace while ctk+ uses Ctrl+r */
	ctk_binding_entry_add_signal (binding_set, GDK_KEY_h, GDK_CONTROL_MASK, "show_replace", 0);
	ctk_binding_entry_add_signal (binding_set, GDK_KEY_H, GDK_CONTROL_MASK, "show_replace", 0);
}

static void
insert_text_handler (GtkEditable *editable,
		     const gchar *text,
		     gint         length,
		     gint        *position,
		     gpointer     data)
{
	static gboolean insert_text = FALSE;
	gchar *escaped_text;
	gint new_len;

	/* To avoid recursive behavior */
	if (insert_text)
		return;

	escaped_text = lapiz_utils_escape_search_text (text);

	new_len = strlen (escaped_text);

	if (new_len == length)
	{
		g_free (escaped_text);
		return;
	}

	insert_text = TRUE;

	g_signal_stop_emission_by_name (editable, "insert_text");

	ctk_editable_insert_text (editable, escaped_text, new_len, position);

	insert_text = FALSE;

	g_free (escaped_text);
}

static void
search_text_entry_changed (GtkEditable       *editable,
			   LapizSearchDialog *dialog)
{
	const gchar *search_string;

	search_string = ctk_entry_get_text (CTK_ENTRY (editable));
	g_return_if_fail (search_string != NULL);

	if (*search_string != '\0')
	{
		ctk_dialog_set_response_sensitive (CTK_DIALOG (dialog),
			LAPIZ_SEARCH_DIALOG_FIND_RESPONSE, TRUE);
		ctk_dialog_set_response_sensitive (CTK_DIALOG (dialog),
			LAPIZ_SEARCH_DIALOG_REPLACE_ALL_RESPONSE, TRUE);
	}
	else
	{
		ctk_dialog_set_response_sensitive (CTK_DIALOG (dialog),
			LAPIZ_SEARCH_DIALOG_FIND_RESPONSE, FALSE);
		ctk_dialog_set_response_sensitive (CTK_DIALOG (dialog),
			LAPIZ_SEARCH_DIALOG_REPLACE_RESPONSE, FALSE);
		ctk_dialog_set_response_sensitive (CTK_DIALOG (dialog),
			LAPIZ_SEARCH_DIALOG_REPLACE_ALL_RESPONSE, FALSE);
	}
}

static void
response_handler (LapizSearchDialog *dialog,
		  gint               response_id,
		  gpointer           data)
{
	const gchar *str;

	switch (response_id)
	{
		case LAPIZ_SEARCH_DIALOG_REPLACE_RESPONSE:
		case LAPIZ_SEARCH_DIALOG_REPLACE_ALL_RESPONSE:
			str = ctk_entry_get_text (CTK_ENTRY (dialog->priv->replace_text_entry));
			if (*str != '\0')
			{
				gchar *text;

				text = lapiz_utils_unescape_search_text (str);
				lapiz_history_entry_prepend_text
						(LAPIZ_HISTORY_ENTRY (dialog->priv->replace_entry),
						 text);

				g_free (text);
			}
			/* fall through, so that we also save the find entry */
		case LAPIZ_SEARCH_DIALOG_FIND_RESPONSE:
			str = ctk_entry_get_text (CTK_ENTRY (dialog->priv->search_text_entry));
			if (*str != '\0')
			{
				gchar *text;

				text = lapiz_utils_unescape_search_text (str);
				lapiz_history_entry_prepend_text
						(LAPIZ_HISTORY_ENTRY (dialog->priv->search_entry),
						 text);

				g_free (text);
			}
	}
}

static void
show_replace_widgets (LapizSearchDialog *dlg,
		      gboolean           show_replace)
{
	if (show_replace)
	{
		ctk_widget_show (dlg->priv->replace_label);
		ctk_widget_show (dlg->priv->replace_entry);
		ctk_widget_show (dlg->priv->replace_all_button);
		ctk_widget_show (dlg->priv->replace_button);

		ctk_window_set_title (CTK_WINDOW (dlg), _("Replace"));
	}
	else
	{
		ctk_widget_hide (dlg->priv->replace_label);
		ctk_widget_hide (dlg->priv->replace_entry);
		ctk_widget_hide (dlg->priv->replace_all_button);
		ctk_widget_hide (dlg->priv->replace_button);

		ctk_window_set_title (CTK_WINDOW (dlg), _("Find"));
	}

	ctk_widget_show (dlg->priv->find_button);
}

static void
lapiz_search_dialog_init (LapizSearchDialog *dlg)
{
	GtkWidget *content;
	GtkWidget *error_widget;
	gboolean ret;
	gchar *file;
	gchar *root_objects[] = {
		"search_dialog_content",
		NULL
	};

	dlg->priv = lapiz_search_dialog_get_instance_private (dlg);

	ctk_window_set_resizable (CTK_WINDOW (dlg), FALSE);
	ctk_window_set_destroy_with_parent (CTK_WINDOW (dlg), TRUE);

	lapiz_dialog_add_button (CTK_DIALOG (dlg), _("_Close"), "window-close", CTK_RESPONSE_CANCEL);

	/* HIG defaults */
	ctk_container_set_border_width (CTK_CONTAINER (dlg), 5);
	ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			     2); /* 2 * 5 + 2 = 12 */

	file = lapiz_dirs_get_ui_file ("lapiz-search-dialog.ui");

	ret = lapiz_utils_get_ui_objects (file,
					  root_objects,
					  &error_widget,
					  "search_dialog_content", &content,
					  "grid", &dlg->priv->grid,
					  "search_label", &dlg->priv->search_label,
					  "replace_with_label", &dlg->priv->replace_label,
					  "match_case_checkbutton", &dlg->priv->match_case_checkbutton,
					  "match_regex_checkbutton",&dlg->priv->match_regex_checkbutton,
					  "entire_word_checkbutton", &dlg->priv->entire_word_checkbutton,
					  "search_backwards_checkbutton", &dlg->priv->backwards_checkbutton,
					  "wrap_around_checkbutton", &dlg->priv->wrap_around_checkbutton,
					  "parse_escapes_checkbutton", &dlg->priv->parse_escapes_checkbutton,
					  NULL);
	g_free (file);

	if (!ret)
	{
		ctk_widget_show (error_widget);

		ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
		                    error_widget,
		                    TRUE, TRUE, 0);
		ctk_container_set_border_width (CTK_CONTAINER (error_widget),
						5);

		dlg->priv->ui_error = TRUE;

		return;
	}

	dlg->priv->search_entry = lapiz_history_entry_new ("history-search-for",
							   TRUE);
	ctk_widget_set_size_request (dlg->priv->search_entry, 300, -1);
	lapiz_history_entry_set_escape_func
			(LAPIZ_HISTORY_ENTRY (dlg->priv->search_entry),
			 (LapizHistoryEntryEscapeFunc) lapiz_utils_escape_search_text);

	ctk_widget_set_hexpand (CTK_WIDGET (dlg->priv->search_entry), TRUE);
	dlg->priv->search_text_entry = lapiz_history_entry_get_entry
			(LAPIZ_HISTORY_ENTRY (dlg->priv->search_entry));
	ctk_entry_set_activates_default (CTK_ENTRY (dlg->priv->search_text_entry),
					 TRUE);
	ctk_widget_show (dlg->priv->search_entry);
	ctk_grid_attach_next_to (CTK_GRID (dlg->priv->grid),
				 dlg->priv->search_entry,
				 dlg->priv->search_label,
				 CTK_POS_RIGHT, 1, 1);

	dlg->priv->replace_entry = lapiz_history_entry_new ("history-replace-with",
							    TRUE);
	lapiz_history_entry_set_escape_func
			(LAPIZ_HISTORY_ENTRY (dlg->priv->replace_entry),
			 (LapizHistoryEntryEscapeFunc) lapiz_utils_escape_search_text);

	ctk_widget_set_hexpand (CTK_WIDGET (dlg->priv->replace_entry), TRUE);
	dlg->priv->replace_text_entry = lapiz_history_entry_get_entry
			(LAPIZ_HISTORY_ENTRY (dlg->priv->replace_entry));
	ctk_entry_set_activates_default (CTK_ENTRY (dlg->priv->replace_text_entry),
					 TRUE);
	ctk_widget_show (dlg->priv->replace_entry);
	ctk_grid_attach_next_to (CTK_GRID (dlg->priv->grid),
				 dlg->priv->replace_entry,
				 dlg->priv->replace_label,
				 CTK_POS_RIGHT, 1, 1);

	ctk_label_set_mnemonic_widget (CTK_LABEL (dlg->priv->search_label),
				       dlg->priv->search_entry);
	ctk_label_set_mnemonic_widget (CTK_LABEL (dlg->priv->replace_label),
				       dlg->priv->replace_entry);

	dlg->priv->find_button = ctk_button_new_with_mnemonic (_("_Find"));
	ctk_button_set_image (CTK_BUTTON (dlg->priv->find_button), ctk_image_new_from_icon_name ("edit-find", CTK_ICON_SIZE_BUTTON));

	dlg->priv->replace_all_button = ctk_button_new_with_mnemonic (_("Replace _All"));
	dlg->priv->replace_button = lapiz_ctk_button_new_with_icon (_("_Replace"),
								    "edit-find-replace");

	ctk_dialog_add_action_widget (CTK_DIALOG (dlg),
				      dlg->priv->replace_all_button,
				      LAPIZ_SEARCH_DIALOG_REPLACE_ALL_RESPONSE);
	ctk_dialog_add_action_widget (CTK_DIALOG (dlg),
				      dlg->priv->replace_button,
				      LAPIZ_SEARCH_DIALOG_REPLACE_RESPONSE);
	ctk_dialog_add_action_widget (CTK_DIALOG (dlg),
				      dlg->priv->find_button,
				      LAPIZ_SEARCH_DIALOG_FIND_RESPONSE);
	g_object_set (G_OBJECT (dlg->priv->find_button),
		      "can-default", TRUE,
		      NULL);

	ctk_dialog_set_default_response (CTK_DIALOG (dlg),
					 LAPIZ_SEARCH_DIALOG_FIND_RESPONSE);

	/* insensitive by default */
	ctk_dialog_set_response_sensitive (CTK_DIALOG (dlg),
					   LAPIZ_SEARCH_DIALOG_FIND_RESPONSE,
					   FALSE);
	ctk_dialog_set_response_sensitive (CTK_DIALOG (dlg),
					   LAPIZ_SEARCH_DIALOG_REPLACE_RESPONSE,
					   FALSE);
	ctk_dialog_set_response_sensitive (CTK_DIALOG (dlg),
					   LAPIZ_SEARCH_DIALOG_REPLACE_ALL_RESPONSE,
					   FALSE);

	ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			    content, FALSE, FALSE, 0);
	g_object_unref (content);
	ctk_container_set_border_width (CTK_CONTAINER (content), 5);

	g_signal_connect (dlg->priv->search_text_entry,
			  "insert_text",
			  G_CALLBACK (insert_text_handler),
			  NULL);
	g_signal_connect (dlg->priv->replace_text_entry,
			  "insert_text",
			  G_CALLBACK (insert_text_handler),
			  NULL);
	g_signal_connect (dlg->priv->search_text_entry,
			  "changed",
			  G_CALLBACK (search_text_entry_changed),
			  dlg);

	g_signal_connect (dlg,
			  "response",
			  G_CALLBACK (response_handler),
			  NULL);
}

GtkWidget *
lapiz_search_dialog_new (GtkWindow *parent,
			 gboolean   show_replace)
{
	LapizSearchDialog *dlg;

	dlg = g_object_new (LAPIZ_TYPE_SEARCH_DIALOG,
			    "show-replace", show_replace,
			    NULL);

	if (parent != NULL)
	{
		ctk_window_set_transient_for (CTK_WINDOW (dlg),
					      parent);

		ctk_window_set_destroy_with_parent (CTK_WINDOW (dlg),
						    TRUE);
	}

	return CTK_WIDGET (dlg);
}

gboolean
lapiz_search_dialog_get_show_replace (LapizSearchDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog), FALSE);

	return dialog->priv->show_replace;
}

void
lapiz_search_dialog_set_show_replace (LapizSearchDialog *dialog,
				      gboolean           show_replace)
{
	g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog));

	if (dialog->priv->ui_error)
		return;

	dialog->priv->show_replace = show_replace != FALSE;
	show_replace_widgets (dialog, dialog->priv->show_replace);

	g_object_notify (G_OBJECT (dialog), "show-replace");
}

void
lapiz_search_dialog_set_search_text (LapizSearchDialog *dialog,
				     const gchar       *text)
{
	g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog));
	g_return_if_fail (text != NULL);

	ctk_entry_set_text (CTK_ENTRY (dialog->priv->search_text_entry),
			    text);

	ctk_dialog_set_response_sensitive (CTK_DIALOG (dialog),
					   LAPIZ_SEARCH_DIALOG_FIND_RESPONSE,
					   (*text != '\0'));

	ctk_dialog_set_response_sensitive (CTK_DIALOG (dialog),
					   LAPIZ_SEARCH_DIALOG_REPLACE_ALL_RESPONSE,
					   (*text != '\0'));
}

/*
 * The text must be unescaped before searching.
 */
const gchar *
lapiz_search_dialog_get_search_text (LapizSearchDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog), NULL);

	return ctk_entry_get_text (CTK_ENTRY (dialog->priv->search_text_entry));
}

void
lapiz_search_dialog_set_replace_text (LapizSearchDialog *dialog,
				      const gchar       *text)
{
	g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog));
	g_return_if_fail (text != NULL);

	ctk_entry_set_text (CTK_ENTRY (dialog->priv->replace_text_entry),
			    text);
}

const gchar *
lapiz_search_dialog_get_replace_text (LapizSearchDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog), NULL);

	return ctk_entry_get_text (CTK_ENTRY (dialog->priv->replace_text_entry));
}

void
lapiz_search_dialog_set_match_case (LapizSearchDialog *dialog,
				    gboolean           match_case)
{
	g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog));

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->match_case_checkbutton),
				      match_case);
}

gboolean
lapiz_search_dialog_get_match_case (LapizSearchDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog), FALSE);

	return ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->match_case_checkbutton));
}

void
lapiz_search_dialog_set_match_regex (LapizSearchDialog *dialog,
                    gboolean           match_case)
{
	g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog));

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->match_regex_checkbutton),
                      match_case);
}

gboolean
lapiz_search_dialog_get_match_regex (LapizSearchDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog), FALSE);

	return ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->match_regex_checkbutton));
}

void
lapiz_search_dialog_set_entire_word (LapizSearchDialog *dialog,
				     gboolean           entire_word)
{
	g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog));

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->entire_word_checkbutton),
				      entire_word);
}

gboolean
lapiz_search_dialog_get_entire_word (LapizSearchDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog), FALSE);

	return ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->entire_word_checkbutton));
}

void
lapiz_search_dialog_set_backwards (LapizSearchDialog *dialog,
				  gboolean           backwards)
{
	g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog));

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->backwards_checkbutton),
				      backwards);
}

gboolean
lapiz_search_dialog_get_backwards (LapizSearchDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog), FALSE);

	return ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->backwards_checkbutton));
}

void
lapiz_search_dialog_set_wrap_around (LapizSearchDialog *dialog,
				     gboolean           wrap_around)
{
	g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog));

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->wrap_around_checkbutton),
				      wrap_around);
}

gboolean
lapiz_search_dialog_get_wrap_around (LapizSearchDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog), FALSE);

	return ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->wrap_around_checkbutton));
}

void
lapiz_search_dialog_set_parse_escapes (LapizSearchDialog *dialog,
				       gboolean           parse_escapes)
{
	g_return_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog));

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->parse_escapes_checkbutton),
				      parse_escapes);
}

gboolean
lapiz_search_dialog_get_parse_escapes (LapizSearchDialog *dialog)
{
	g_return_val_if_fail (LAPIZ_IS_SEARCH_DIALOG (dialog), FALSE);

	return ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->parse_escapes_checkbutton));
}
