/*
 * lapiz-encodings-dialog.c
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

#include <glib.h>
#include <glib/gi18n.h>
#include <ctk/ctk.h>

#include "lapiz-encodings-dialog.h"
#include "lapiz-encodings.h"
#include "lapiz-prefs-manager.h"
#include "lapiz-utils.h"
#include "lapiz-debug.h"
#include "lapiz-help.h"
#include "lapiz-dirs.h"

struct _LapizEncodingsDialogPrivate
{
	CtkListStore	*available_liststore;
	CtkListStore	*displayed_liststore;
	CtkWidget	*available_treeview;
	CtkWidget	*displayed_treeview;
	CtkWidget	*add_button;
	CtkWidget	*remove_button;

	GSList		*show_in_menu_list;
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizEncodingsDialog, lapiz_encodings_dialog, CTK_TYPE_DIALOG)

static void
lapiz_encodings_dialog_finalize (GObject *object)
{
	LapizEncodingsDialogPrivate *priv = LAPIZ_ENCODINGS_DIALOG (object)->priv;

	g_slist_free (priv->show_in_menu_list);

	G_OBJECT_CLASS (lapiz_encodings_dialog_parent_class)->finalize (object);
}

static void
lapiz_encodings_dialog_class_init (LapizEncodingsDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_encodings_dialog_finalize;
}

enum {
	COLUMN_NAME,
	COLUMN_CHARSET,
	N_COLUMNS
};

static void
count_selected_items_func (CtkTreeModel *model,
			   CtkTreePath  *path,
			   CtkTreeIter  *iter,
			   gpointer      data)
{
	int *count = data;

	*count += 1;
}

static void
available_selection_changed_callback (CtkTreeSelection     *selection,
				      LapizEncodingsDialog *dialogs)
{
	int count;

	count = 0;
	ctk_tree_selection_selected_foreach (selection,
					     count_selected_items_func,
					     &count);

	ctk_widget_set_sensitive (dialogs->priv->add_button, count > 0);
}

static void
displayed_selection_changed_callback (CtkTreeSelection     *selection,
				      LapizEncodingsDialog *dialogs)
{
	int count;

	count = 0;
	ctk_tree_selection_selected_foreach (selection,
					     count_selected_items_func,
					     &count);

	ctk_widget_set_sensitive (dialogs->priv->remove_button, count > 0);
}

static void
get_selected_encodings_func (CtkTreeModel *model,
			     CtkTreePath  *path,
			     CtkTreeIter  *iter,
			     gpointer      data)
{
	GSList **list = data;
	gchar *charset;
	const LapizEncoding *enc;

	charset = NULL;
	ctk_tree_model_get (model, iter, COLUMN_CHARSET, &charset, -1);

	enc = lapiz_encoding_get_from_charset (charset);
	g_free (charset);

	*list = g_slist_prepend (*list, (gpointer)enc);
}

static void
update_shown_in_menu_tree_model (CtkListStore *store,
				 GSList       *list)
{
	CtkTreeIter iter;

	ctk_list_store_clear (store);

	while (list != NULL)
	{
		const LapizEncoding *enc;

		enc = (const LapizEncoding*) list->data;

		ctk_list_store_append (store, &iter);
		ctk_list_store_set (store, &iter,
				    COLUMN_CHARSET,
				    lapiz_encoding_get_charset (enc),
				    COLUMN_NAME,
				    lapiz_encoding_get_name (enc), -1);

		list = g_slist_next (list);
	}
}

static void
add_button_clicked_callback (CtkWidget            *button,
			     LapizEncodingsDialog *dialog)
{
	CtkTreeSelection *selection;
	GSList *encodings;
	GSList *tmp;

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dialog->priv->available_treeview));

	encodings = NULL;
	ctk_tree_selection_selected_foreach (selection,
					     get_selected_encodings_func,
					     &encodings);

	tmp = encodings;
	while (tmp != NULL)
	{
		if (g_slist_find (dialog->priv->show_in_menu_list, tmp->data) == NULL)
			dialog->priv->show_in_menu_list = g_slist_prepend (dialog->priv->show_in_menu_list,
									   tmp->data);

		tmp = g_slist_next (tmp);
	}

	g_slist_free (encodings);

	update_shown_in_menu_tree_model (CTK_LIST_STORE (dialog->priv->displayed_liststore),
					 dialog->priv->show_in_menu_list);
}

static void
remove_button_clicked_callback (CtkWidget            *button,
				LapizEncodingsDialog *dialog)
{
	CtkTreeSelection *selection;
	GSList *encodings;
	GSList *tmp;

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dialog->priv->displayed_treeview));

	encodings = NULL;
	ctk_tree_selection_selected_foreach (selection,
					     get_selected_encodings_func,
					     &encodings);

	tmp = encodings;
	while (tmp != NULL)
	{
		dialog->priv->show_in_menu_list = g_slist_remove (dialog->priv->show_in_menu_list,
								  tmp->data);

		tmp = g_slist_next (tmp);
	}

	g_slist_free (encodings);

	update_shown_in_menu_tree_model (CTK_LIST_STORE (dialog->priv->displayed_liststore),
					 dialog->priv->show_in_menu_list);
}

static void
init_shown_in_menu_tree_model (LapizEncodingsDialog *dialog)
{
	CtkTreeIter iter;
	GSList *list, *tmp;

	/* add data to the list store */
	list = lapiz_prefs_manager_get_shown_in_menu_encodings ();

	tmp = list;

	while (tmp != NULL)
	{
		const LapizEncoding *enc;

		enc = (const LapizEncoding *) tmp->data;

		dialog->priv->show_in_menu_list = g_slist_prepend (dialog->priv->show_in_menu_list,
								   tmp->data);

		ctk_list_store_append (dialog->priv->displayed_liststore,
				       &iter);
		ctk_list_store_set (dialog->priv->displayed_liststore,
				    &iter,
				    COLUMN_CHARSET,
				    lapiz_encoding_get_charset (enc),
				    COLUMN_NAME,
				    lapiz_encoding_get_name (enc), -1);

		tmp = g_slist_next (tmp);
	}

	g_slist_free (list);
}

static void
response_handler (CtkDialog            *dialog,
		  gint                  response_id,
                  LapizEncodingsDialog *dlg)
{
	if (response_id == CTK_RESPONSE_HELP)
	{
		lapiz_help_display (CTK_WINDOW (dialog), "lapiz", NULL);
		g_signal_stop_emission_by_name (dialog, "response");
		return;
	}

	if (response_id == CTK_RESPONSE_OK)
	{
		g_return_if_fail (lapiz_prefs_manager_shown_in_menu_encodings_can_set ());
		lapiz_prefs_manager_set_shown_in_menu_encodings (dlg->priv->show_in_menu_list);
	}
}

static void
lapiz_encodings_dialog_init (LapizEncodingsDialog *dlg)
{
	CtkWidget *content;
	CtkCellRenderer *cell_renderer;
	CtkTreeModel *sort_model;
	CtkTreeViewColumn *column;
	CtkTreeIter parent_iter;
	CtkTreeSelection *selection;
	const LapizEncoding *enc;
	CtkWidget *error_widget;
	int i;
	gboolean ret;
	gchar *file;
	gchar *root_objects[] = {
		"encodings-dialog-contents",
		NULL
	};

	dlg->priv = lapiz_encodings_dialog_get_instance_private (dlg);

	lapiz_dialog_add_button (CTK_DIALOG (dlg), _("_Cancel"), "process-stop", CTK_RESPONSE_CANCEL);
	lapiz_dialog_add_button (CTK_DIALOG (dlg), _("_OK"), "ctk-ok", CTK_RESPONSE_OK);
	lapiz_dialog_add_button (CTK_DIALOG (dlg), _("_Help"), "help-browser", CTK_RESPONSE_HELP);

	ctk_window_set_title (CTK_WINDOW (dlg), _("Character Encodings"));
	ctk_window_set_default_size (CTK_WINDOW (dlg), 650, 400);

	/* HIG defaults */
	ctk_container_set_border_width (CTK_CONTAINER (dlg), 5);
	ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			     2); /* 2 * 5 + 2 = 12 */

	ctk_dialog_set_default_response (CTK_DIALOG (dlg),
					 CTK_RESPONSE_OK);

	g_signal_connect (dlg,
			  "response",
			  G_CALLBACK (response_handler),
			  dlg);

	file = lapiz_dirs_get_ui_file ("lapiz-encodings-dialog.ui");
	ret = lapiz_utils_get_ui_objects (file,
					  root_objects,
					  &error_widget,
					  "encodings-dialog-contents", &content,
					  "add-button", &dlg->priv->add_button,
					  "remove-button", &dlg->priv->remove_button,
					  "available-treeview", &dlg->priv->available_treeview,
					  "displayed-treeview", &dlg->priv->displayed_treeview,
					  NULL);
	g_free (file);

	if (!ret)
	{
		ctk_widget_show (error_widget);

		ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
		                    error_widget,
		                    TRUE, TRUE, 0);
		ctk_container_set_border_width (CTK_CONTAINER (error_widget), 5);

		return;
	}

	ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			    content, TRUE, TRUE, 0);
	g_object_unref (content);
	ctk_container_set_border_width (CTK_CONTAINER (content), 5);

	ctk_button_set_image (CTK_BUTTON (dlg->priv->add_button), ctk_image_new_from_icon_name ("list-add", CTK_ICON_SIZE_BUTTON));
	ctk_button_set_image (CTK_BUTTON (dlg->priv->remove_button), ctk_image_new_from_icon_name ("list-remove", CTK_ICON_SIZE_BUTTON));

	g_signal_connect (dlg->priv->add_button,
			  "clicked",
			  G_CALLBACK (add_button_clicked_callback),
			  dlg);
	g_signal_connect (dlg->priv->remove_button,
			  "clicked",
			  G_CALLBACK (remove_button_clicked_callback),
			  dlg);

	/* Tree view of available encodings */
	dlg->priv->available_liststore = ctk_list_store_new (N_COLUMNS,
							     G_TYPE_STRING,
							     G_TYPE_STRING);

	cell_renderer = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("_Description"),
							   cell_renderer,
							   "text", COLUMN_NAME,
							   NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (dlg->priv->available_treeview),
				     column);
	ctk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);

	cell_renderer = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("_Encoding"),
							   cell_renderer,
							   "text",
							   COLUMN_CHARSET,
							   NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (dlg->priv->available_treeview),
				     column);
	ctk_tree_view_column_set_sort_column_id (column, COLUMN_CHARSET);

	/* Add the data */
	i = 0;
	while ((enc = lapiz_encoding_get_from_index (i)) != NULL)
	{
		ctk_list_store_append (dlg->priv->available_liststore,
				       &parent_iter);
		ctk_list_store_set (dlg->priv->available_liststore,
				    &parent_iter,
				    COLUMN_CHARSET,
				    lapiz_encoding_get_charset (enc),
				    COLUMN_NAME,
				    lapiz_encoding_get_name (enc), -1);

		++i;
	}

	/* Sort model */
	sort_model = ctk_tree_model_sort_new_with_model (CTK_TREE_MODEL (dlg->priv->available_liststore));
	ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (sort_model),
					      COLUMN_NAME,
					      CTK_SORT_ASCENDING);

	ctk_tree_view_set_model (CTK_TREE_VIEW (dlg->priv->available_treeview),
				 sort_model);
	g_object_unref (G_OBJECT (dlg->priv->available_liststore));
	g_object_unref (G_OBJECT (sort_model));

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dlg->priv->available_treeview));
	ctk_tree_selection_set_mode (CTK_TREE_SELECTION (selection),
				     CTK_SELECTION_MULTIPLE);

	available_selection_changed_callback (selection, dlg);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (available_selection_changed_callback),
			  dlg);

	/* Tree view of selected encodings */
	dlg->priv->displayed_liststore = ctk_list_store_new (N_COLUMNS,
							     G_TYPE_STRING,
							     G_TYPE_STRING);

	cell_renderer = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("_Description"),
							   cell_renderer,
							   "text", COLUMN_NAME,
							   NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (dlg->priv->displayed_treeview),
				     column);
	ctk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);

	cell_renderer = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("_Encoding"),
							   cell_renderer,
							   "text",
							   COLUMN_CHARSET,
							   NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (dlg->priv->displayed_treeview),
				     column);
	ctk_tree_view_column_set_sort_column_id (column, COLUMN_CHARSET);

	/* Add the data */
	init_shown_in_menu_tree_model (dlg);

	/* Sort model */
	sort_model = ctk_tree_model_sort_new_with_model (CTK_TREE_MODEL (dlg->priv->displayed_liststore));

	ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE
					      (sort_model), COLUMN_NAME,
					      CTK_SORT_ASCENDING);

	ctk_tree_view_set_model (CTK_TREE_VIEW (dlg->priv->displayed_treeview),
				 sort_model);
	g_object_unref (G_OBJECT (sort_model));
	g_object_unref (G_OBJECT (dlg->priv->displayed_liststore));

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dlg->priv->displayed_treeview));
	ctk_tree_selection_set_mode (CTK_TREE_SELECTION (selection),
				     CTK_SELECTION_MULTIPLE);

	displayed_selection_changed_callback (selection, dlg);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (displayed_selection_changed_callback),
			  dlg);
}

CtkWidget *
lapiz_encodings_dialog_new (void)
{
	CtkWidget *dlg;

	dlg = CTK_WIDGET (g_object_new (LAPIZ_TYPE_ENCODINGS_DIALOG, NULL));

	return dlg;
}

