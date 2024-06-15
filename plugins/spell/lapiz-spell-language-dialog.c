/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-spell-language-dialog.c
 * This file is part of lapiz
 *
 * Copyright (C) 2002 Paolo Maggi
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
 * Modified by the lapiz Team, 2002. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <lapiz/lapiz-utils.h>
#include <lapiz/lapiz-help.h>
#include "lapiz-spell-language-dialog.h"
#include "lapiz-spell-checker-language.h"


enum
{
	COLUMN_LANGUAGE_NAME = 0,
	COLUMN_LANGUAGE_POINTER,
	ENCODING_NUM_COLS
};


struct _LapizSpellLanguageDialog
{
	CtkDialog dialog;

	CtkWidget *languages_treeview;
	CtkTreeModel *model;
};

G_DEFINE_TYPE(LapizSpellLanguageDialog, lapiz_spell_language_dialog, CTK_TYPE_DIALOG)


static void
lapiz_spell_language_dialog_class_init (LapizSpellLanguageDialogClass *klass G_GNUC_UNUSED)
{
	/* GObjectClass *object_class = G_OBJECT_CLASS (klass); */
}

static void
dialog_response_handler (CtkDialog *dlg,
			 gint       res_id)
{
	if (res_id == CTK_RESPONSE_HELP)
	{
		lapiz_help_display (CTK_WINDOW (dlg),
				    NULL,
				    "lapiz-spell-checker-plugin");

		g_signal_stop_emission_by_name (dlg, "response");
	}
}

static void
scroll_to_selected (CtkTreeView *tree_view)
{
	CtkTreeModel *model;
	CtkTreeSelection *selection;
	CtkTreeIter iter;

	model = ctk_tree_view_get_model (tree_view);
	g_return_if_fail (model != NULL);

	/* Scroll to selected */
	selection = ctk_tree_view_get_selection (tree_view);
	g_return_if_fail (selection != NULL);

	if (ctk_tree_selection_get_selected (selection, NULL, &iter))
	{
		CtkTreePath* path;

		path = ctk_tree_model_get_path (model, &iter);
		g_return_if_fail (path != NULL);

		ctk_tree_view_scroll_to_cell (tree_view,
					      path, NULL, TRUE, 1.0, 0.0);
		ctk_tree_path_free (path);
	}
}

static void
language_row_activated (CtkTreeView              *tree_view G_GNUC_UNUSED,
			CtkTreePath              *path G_GNUC_UNUSED,
			CtkTreeViewColumn        *column G_GNUC_UNUSED,
			LapizSpellLanguageDialog *dialog)
{
	ctk_dialog_response (CTK_DIALOG (dialog), CTK_RESPONSE_OK);
}

static void
create_dialog (LapizSpellLanguageDialog *dlg,
	       const gchar *data_dir)
{
	CtkWidget *error_widget;
	CtkWidget *content;
	gboolean ret;
	CtkCellRenderer *cell;
	CtkTreeViewColumn *column;
	gchar *ui_file;
	gchar *root_objects[] = {
		"content",
		NULL
	};

	lapiz_dialog_add_button (CTK_DIALOG (dlg), _("_Cancel"), "process-stop", CTK_RESPONSE_CANCEL);
	lapiz_dialog_add_button (CTK_DIALOG (dlg), _("_OK"), "ctk-ok", CTK_RESPONSE_OK);
	lapiz_dialog_add_button (CTK_DIALOG (dlg), _("_Help"), "help-browser", CTK_RESPONSE_HELP);

	ctk_window_set_title (CTK_WINDOW (dlg), _("Set language"));
	ctk_window_set_modal (CTK_WINDOW (dlg), TRUE);
	ctk_window_set_destroy_with_parent (CTK_WINDOW (dlg), TRUE);

	/* HIG defaults */
	ctk_container_set_border_width (CTK_CONTAINER (dlg), 5);
	ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			     2); /* 2 * 5 + 2 = 12 */

	g_signal_connect (dlg,
			  "response",
			  G_CALLBACK (dialog_response_handler),
			  NULL);

	ui_file = g_build_filename (data_dir, "languages-dialog.ui", NULL);
	ret = lapiz_utils_get_ui_objects (ui_file,
					  root_objects,
					  &error_widget,
					  "content", &content,
					  "languages_treeview", &dlg->languages_treeview,
					  NULL);
	g_free (ui_file);

	if (!ret)
	{
		ctk_widget_show (error_widget);

		ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
		                    error_widget,
		                    TRUE, TRUE, 0);

		return;
	}

	ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			    content, TRUE, TRUE, 0);
	g_object_unref (content);
	ctk_container_set_border_width (CTK_CONTAINER (content), 5);

	dlg->model = CTK_TREE_MODEL (ctk_list_store_new (ENCODING_NUM_COLS,
							 G_TYPE_STRING,
							 G_TYPE_POINTER));

	ctk_tree_view_set_model (CTK_TREE_VIEW (dlg->languages_treeview),
				 dlg->model);

	g_object_unref (dlg->model);

	/* Add the encoding column */
	cell = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("Languages"),
							   cell,
							   "text",
							   COLUMN_LANGUAGE_NAME,
							   NULL);

	ctk_tree_view_append_column (CTK_TREE_VIEW (dlg->languages_treeview),
				     column);

	ctk_tree_view_set_search_column (CTK_TREE_VIEW (dlg->languages_treeview),
					 COLUMN_LANGUAGE_NAME);

	g_signal_connect (dlg->languages_treeview,
			  "realize",
			  G_CALLBACK (scroll_to_selected),
			  dlg);
	g_signal_connect (dlg->languages_treeview,
			  "row-activated",
			  G_CALLBACK (language_row_activated),
			  dlg);
}

static void
lapiz_spell_language_dialog_init (LapizSpellLanguageDialog *dlg G_GNUC_UNUSED)
{

}

static void
populate_language_list (LapizSpellLanguageDialog        *dlg,
			const LapizSpellCheckerLanguage *cur_lang)
{
	CtkListStore *store;
	CtkTreeIter iter;

	const GSList* langs;

	/* create list store */
	store = CTK_LIST_STORE (dlg->model);

	langs = lapiz_spell_checker_get_available_languages ();

	while (langs)
	{
		const gchar *name;

		name = lapiz_spell_checker_language_to_string ((const LapizSpellCheckerLanguage*)langs->data);

		ctk_list_store_append (store, &iter);
		ctk_list_store_set (store, &iter,
				    COLUMN_LANGUAGE_NAME, name,
				    COLUMN_LANGUAGE_POINTER, langs->data,
				    -1);

		if (langs->data == cur_lang)
		{
			CtkTreeSelection *selection;

			selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dlg->languages_treeview));
			g_return_if_fail (selection != NULL);

			ctk_tree_selection_select_iter (selection, &iter);
		}

		langs = g_slist_next (langs);
	}
}

CtkWidget *
lapiz_spell_language_dialog_new (CtkWindow                       *parent,
				 const LapizSpellCheckerLanguage *cur_lang,
				 const gchar *data_dir)
{
	LapizSpellLanguageDialog *dlg;

	g_return_val_if_fail (CTK_IS_WINDOW (parent), NULL);

	dlg = g_object_new (LAPIZ_TYPE_SPELL_LANGUAGE_DIALOG, NULL);

	create_dialog (dlg, data_dir);

	populate_language_list (dlg, cur_lang);

	ctk_window_set_transient_for (CTK_WINDOW (dlg), parent);
	ctk_widget_grab_focus (dlg->languages_treeview);

	return CTK_WIDGET (dlg);
}

const LapizSpellCheckerLanguage *
lapiz_spell_language_get_selected_language (LapizSpellLanguageDialog *dlg)
{
	GValue value = {0, };
	const LapizSpellCheckerLanguage* lang;

	CtkTreeIter iter;
	CtkTreeSelection *selection;

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dlg->languages_treeview));
	g_return_val_if_fail (selection != NULL, NULL);

	if (!ctk_tree_selection_get_selected (selection, NULL, &iter))
		return NULL;

	ctk_tree_model_get_value (dlg->model,
				  &iter,
				  COLUMN_LANGUAGE_POINTER,
				  &value);

	lang = (const LapizSpellCheckerLanguage* ) g_value_get_pointer (&value);

	return lang;
}

