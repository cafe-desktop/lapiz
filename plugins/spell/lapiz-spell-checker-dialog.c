/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-spell-checker-dialog.c
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

#include <string.h>
#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <lapiz/lapiz-utils.h>
#include "lapiz-spell-checker-dialog.h"
#include "lapiz-spell-marshal.h"

struct _LapizSpellCheckerDialog
{
	GtkWindow parent_instance;

	LapizSpellChecker 	*spell_checker;

	gchar			*misspelled_word;

	GtkWidget 		*misspelled_word_label;
	GtkWidget		*word_entry;
	GtkWidget		*check_word_button;
	GtkWidget		*ignore_button;
	GtkWidget		*ignore_all_button;
	GtkWidget		*change_button;
	GtkWidget		*change_all_button;
	GtkWidget		*add_word_button;
	GtkWidget		*close_button;
	GtkWidget		*suggestions_list;
	GtkWidget		*language_label;

	GtkTreeModel		*suggestions_list_model;
};

enum
{
	IGNORE,
	IGNORE_ALL,
	CHANGE,
	CHANGE_ALL,
	ADD_WORD_TO_PERSONAL,
	LAST_SIGNAL
};

enum
{
	COLUMN_SUGGESTIONS,
	NUM_COLUMNS
};

static void	update_suggestions_list_model 			(LapizSpellCheckerDialog *dlg,
								 GSList *suggestions);

static void	word_entry_changed_handler			(GtkEditable *editable,
								 LapizSpellCheckerDialog *dlg);
static void	close_button_clicked_handler 			(GtkButton *button,
								 LapizSpellCheckerDialog *dlg);
static void	suggestions_list_selection_changed_handler 	(GtkTreeSelection *selection,
								 LapizSpellCheckerDialog *dlg);
static void	check_word_button_clicked_handler 		(GtkButton *button,
								 LapizSpellCheckerDialog *dlg);
static void	add_word_button_clicked_handler 		(GtkButton *button,
								 LapizSpellCheckerDialog *dlg);
static void	ignore_button_clicked_handler 			(GtkButton *button,
								 LapizSpellCheckerDialog *dlg);
static void	ignore_all_button_clicked_handler 		(GtkButton *button,
								 LapizSpellCheckerDialog *dlg);
static void	change_button_clicked_handler 			(GtkButton *button,
								 LapizSpellCheckerDialog *dlg);
static void	change_all_button_clicked_handler 		(GtkButton *button,
								 LapizSpellCheckerDialog *dlg);
static void	suggestions_list_row_activated_handler		(GtkTreeView *view,
								 GtkTreePath *path,
								 GtkTreeViewColumn *column,
								 LapizSpellCheckerDialog *dlg);


static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(LapizSpellCheckerDialog, lapiz_spell_checker_dialog, CTK_TYPE_WINDOW)

static void
lapiz_spell_checker_dialog_dispose (GObject *object)
{
	LapizSpellCheckerDialog *dlg = LAPIZ_SPELL_CHECKER_DIALOG (object);

	if (dlg->spell_checker != NULL)
	{
		g_object_unref (dlg->spell_checker);
		dlg->spell_checker = NULL;
	}

	if (dlg->misspelled_word != NULL)
	{
		g_free (dlg->misspelled_word);
		dlg->misspelled_word = NULL;
	}

	G_OBJECT_CLASS (lapiz_spell_checker_dialog_parent_class)->dispose (object);
}

static void
lapiz_spell_checker_dialog_class_init (LapizSpellCheckerDialogClass * klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = lapiz_spell_checker_dialog_dispose;

	signals[IGNORE] =
		g_signal_new ("ignore",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizSpellCheckerDialogClass, ignore),
			      NULL, NULL,
			      lapiz_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	signals[IGNORE_ALL] =
		g_signal_new ("ignore_all",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizSpellCheckerDialogClass, ignore_all),
			      NULL, NULL,
			      lapiz_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	signals[CHANGE] =
		g_signal_new ("change",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizSpellCheckerDialogClass, change),
			      NULL, NULL,
			      lapiz_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	signals[CHANGE_ALL] =
		g_signal_new ("change_all",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizSpellCheckerDialogClass, change_all),
			      NULL, NULL,
			      lapiz_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	signals[ADD_WORD_TO_PERSONAL] =
		g_signal_new ("add_word_to_personal",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizSpellCheckerDialogClass, add_word_to_personal),
			      NULL, NULL,
			      lapiz_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);
}

static void
create_dialog (LapizSpellCheckerDialog *dlg,
	       const gchar *data_dir)
{
	GtkWidget *error_widget;
	GtkWidget *content;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GtkTreeSelection *selection;
	gchar *root_objects[] = {
		"content",
		"check_word_image",
		"add_word_image",
                "ignore_image",
                "change_image",
                "ignore_all_image",
                "change_all_image",
		NULL
	};
	gboolean ret;
	gchar *ui_file;

	g_return_if_fail (dlg != NULL);

	dlg->spell_checker = NULL;
	dlg->misspelled_word = NULL;

	ui_file = g_build_filename (data_dir, "spell-checker.ui", NULL);
	ret = lapiz_utils_get_ui_objects (ui_file,
		root_objects,
		&error_widget,

		"content", &content,
		"misspelled_word_label", &dlg->misspelled_word_label,
		"word_entry", &dlg->word_entry,
		"check_word_button", &dlg->check_word_button,
		"ignore_button", &dlg->ignore_button,
		"ignore_all_button", &dlg->ignore_all_button,
		"change_button", &dlg->change_button,
		"change_all_button", &dlg->change_all_button,
		"add_word_button", &dlg->add_word_button,
		"close_button", &dlg->close_button,
		"suggestions_list", &dlg->suggestions_list,
		"language_label", &dlg->language_label,
		NULL);
	g_free (ui_file);

	if (!ret)
	{
		ctk_widget_show (error_widget);

		ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
				    error_widget, TRUE, TRUE, 0);

		return;
	}

	ctk_label_set_label (CTK_LABEL (dlg->misspelled_word_label), "");
	ctk_widget_set_sensitive (dlg->word_entry, FALSE);
	ctk_widget_set_sensitive (dlg->check_word_button, FALSE);
	ctk_widget_set_sensitive (dlg->ignore_button, FALSE);
	ctk_widget_set_sensitive (dlg->ignore_all_button, FALSE);
	ctk_widget_set_sensitive (dlg->change_button, FALSE);
	ctk_widget_set_sensitive (dlg->change_all_button, FALSE);
	ctk_widget_set_sensitive (dlg->add_word_button, FALSE);

	ctk_label_set_label (CTK_LABEL (dlg->language_label), "");

	ctk_container_add (CTK_CONTAINER (dlg), content);
	g_object_unref (content);

	ctk_window_set_resizable (CTK_WINDOW (dlg), FALSE);
	ctk_window_set_title (CTK_WINDOW (dlg), _("Check Spelling"));

	/* Suggestion list */
	dlg->suggestions_list_model = CTK_TREE_MODEL (
			ctk_list_store_new (NUM_COLUMNS, G_TYPE_STRING));

	ctk_tree_view_set_model (CTK_TREE_VIEW (dlg->suggestions_list),
			dlg->suggestions_list_model);

	/* Add the suggestions column */
	cell = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("Suggestions"), cell,
			"text", COLUMN_SUGGESTIONS, NULL);

	ctk_tree_view_append_column (CTK_TREE_VIEW (dlg->suggestions_list), column);

	ctk_tree_view_set_search_column (CTK_TREE_VIEW (dlg->suggestions_list),
					 COLUMN_SUGGESTIONS);

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dlg->suggestions_list));

	ctk_tree_selection_set_mode (selection, CTK_SELECTION_SINGLE);

	/* Set default button */
	ctk_widget_set_can_default (dlg->change_button, TRUE);
	ctk_widget_grab_default (dlg->change_button);

	ctk_entry_set_activates_default (CTK_ENTRY (dlg->word_entry), TRUE);

	ctk_button_set_image (CTK_BUTTON (dlg->close_button),
			      ctk_image_new_from_icon_name ("window-close", CTK_ICON_SIZE_BUTTON));

	/* Connect signals */
	g_signal_connect (dlg->word_entry, "changed",
			  G_CALLBACK (word_entry_changed_handler), dlg);
	g_signal_connect (dlg->close_button, "clicked",
			  G_CALLBACK (close_button_clicked_handler), dlg);
	g_signal_connect (selection, "changed",
			  G_CALLBACK (suggestions_list_selection_changed_handler),
			  dlg);
	g_signal_connect (dlg->check_word_button, "clicked",
			  G_CALLBACK (check_word_button_clicked_handler), dlg);
	g_signal_connect (dlg->add_word_button, "clicked",
			  G_CALLBACK (add_word_button_clicked_handler), dlg);
	g_signal_connect (dlg->ignore_button, "clicked",
			  G_CALLBACK (ignore_button_clicked_handler), dlg);
	g_signal_connect (dlg->ignore_all_button, "clicked",
			  G_CALLBACK (ignore_all_button_clicked_handler), dlg);
	g_signal_connect (dlg->change_button, "clicked",
			  G_CALLBACK (change_button_clicked_handler), dlg);
	g_signal_connect (dlg->change_all_button, "clicked",
			  G_CALLBACK (change_all_button_clicked_handler), dlg);
	g_signal_connect (dlg->suggestions_list, "row-activated",
			  G_CALLBACK (suggestions_list_row_activated_handler), dlg);
}

static void
lapiz_spell_checker_dialog_init (LapizSpellCheckerDialog *dlg)
{
}

GtkWidget *
lapiz_spell_checker_dialog_new (const gchar *data_dir)
{
	LapizSpellCheckerDialog *dlg;

	dlg = LAPIZ_SPELL_CHECKER_DIALOG (
			g_object_new (LAPIZ_TYPE_SPELL_CHECKER_DIALOG, NULL));

	g_return_val_if_fail (dlg != NULL, NULL);

	create_dialog (dlg, data_dir);

	return CTK_WIDGET (dlg);
}

GtkWidget *
lapiz_spell_checker_dialog_new_from_spell_checker (LapizSpellChecker *spell,
						   const gchar *data_dir)
{
	LapizSpellCheckerDialog *dlg;

	g_return_val_if_fail (spell != NULL, NULL);

	dlg = LAPIZ_SPELL_CHECKER_DIALOG (
			g_object_new (LAPIZ_TYPE_SPELL_CHECKER_DIALOG, NULL));

	g_return_val_if_fail (dlg != NULL, NULL);

	create_dialog (dlg, data_dir);

	lapiz_spell_checker_dialog_set_spell_checker (dlg, spell);

	return CTK_WIDGET (dlg);
}

void
lapiz_spell_checker_dialog_set_spell_checker (LapizSpellCheckerDialog *dlg, LapizSpellChecker *spell)
{
	const LapizSpellCheckerLanguage* language;
	const gchar *lang;
	gchar *tmp;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));
	g_return_if_fail (spell != NULL);

	if (dlg->spell_checker != NULL)
		g_object_unref (dlg->spell_checker);

	dlg->spell_checker = spell;
	g_object_ref (dlg->spell_checker);

	language = lapiz_spell_checker_get_language (dlg->spell_checker);

	lang = lapiz_spell_checker_language_to_string (language);
	tmp = g_strdup_printf("<b>%s</b>", lang);

	ctk_label_set_label (CTK_LABEL (dlg->language_label), tmp);
	g_free (tmp);

	if (dlg->misspelled_word != NULL)
		lapiz_spell_checker_dialog_set_misspelled_word (dlg, dlg->misspelled_word, -1);
	else
		ctk_list_store_clear (CTK_LIST_STORE (dlg->suggestions_list_model));

	/* TODO: reset all widgets */
}

void
lapiz_spell_checker_dialog_set_misspelled_word (LapizSpellCheckerDialog *dlg,
						const gchar             *word,
						gint                     len)
{
	gchar *tmp;
	GSList *sug;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));
	g_return_if_fail (word != NULL);

	g_return_if_fail (dlg->spell_checker != NULL);
	g_return_if_fail (!lapiz_spell_checker_check_word (dlg->spell_checker, word, -1));

	/* build_suggestions_list */
	if (dlg->misspelled_word != NULL)
		g_free (dlg->misspelled_word);

	dlg->misspelled_word = g_strdup (word);

	tmp = g_strdup_printf("<b>%s</b>", word);
	ctk_label_set_label (CTK_LABEL (dlg->misspelled_word_label), tmp);
	g_free (tmp);

	sug = lapiz_spell_checker_get_suggestions (dlg->spell_checker,
		       				   dlg->misspelled_word,
		       				   -1);

	update_suggestions_list_model (dlg, sug);

	/* free the suggestion list */
	g_slist_foreach (sug, (GFunc)g_free, NULL);
	g_slist_free (sug);

	ctk_widget_set_sensitive (dlg->ignore_button, TRUE);
	ctk_widget_set_sensitive (dlg->ignore_all_button, TRUE);
	ctk_widget_set_sensitive (dlg->add_word_button, TRUE);
}

static void
update_suggestions_list_model (LapizSpellCheckerDialog *dlg, GSList *suggestions)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeSelection *sel;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));
	g_return_if_fail (CTK_IS_LIST_STORE (dlg->suggestions_list_model));

	store = CTK_LIST_STORE (dlg->suggestions_list_model);
	ctk_list_store_clear (store);

	ctk_widget_set_sensitive (dlg->word_entry, TRUE);

	if (suggestions == NULL)
	{
		ctk_list_store_append (store, &iter);
		ctk_list_store_set (store, &iter,
		                    /* Translators: Displayed in the "Check Spelling" dialog if there are no suggestions
		                     * for the current misspelled word */
				    COLUMN_SUGGESTIONS, _("(no suggested words)"),
				    -1);

		ctk_entry_set_text (CTK_ENTRY (dlg->word_entry), "");

		ctk_widget_set_sensitive (dlg->suggestions_list, FALSE);

		return;
	}

	ctk_widget_set_sensitive (dlg->suggestions_list, TRUE);

	ctk_entry_set_text (CTK_ENTRY (dlg->word_entry), (gchar*)suggestions->data);

	while (suggestions != NULL)
	{
		ctk_list_store_append (store, &iter);
		ctk_list_store_set (store, &iter,
				    COLUMN_SUGGESTIONS, (gchar*)suggestions->data,
				    -1);

		suggestions = g_slist_next (suggestions);
	}

	sel = ctk_tree_view_get_selection (CTK_TREE_VIEW (dlg->suggestions_list));
	ctk_tree_model_get_iter_first (dlg->suggestions_list_model, &iter);
	ctk_tree_selection_select_iter (sel, &iter);
}

static void
word_entry_changed_handler (GtkEditable *editable, LapizSpellCheckerDialog *dlg)
{
	const gchar *text;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));

	text =  ctk_entry_get_text (CTK_ENTRY (dlg->word_entry));

	if (g_utf8_strlen (text, -1) > 0)
	{
		ctk_widget_set_sensitive (dlg->check_word_button, TRUE);
		ctk_widget_set_sensitive (dlg->change_button, TRUE);
		ctk_widget_set_sensitive (dlg->change_all_button, TRUE);
	}
	else
	{
		ctk_widget_set_sensitive (dlg->check_word_button, FALSE);
		ctk_widget_set_sensitive (dlg->change_button, FALSE);
		ctk_widget_set_sensitive (dlg->change_all_button, FALSE);
	}
}

static void
close_button_clicked_handler (GtkButton *button, LapizSpellCheckerDialog *dlg)
{
	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));

	ctk_widget_destroy (CTK_WIDGET (dlg));
}

static void
suggestions_list_selection_changed_handler (GtkTreeSelection *selection,
		LapizSpellCheckerDialog *dlg)
{
 	GtkTreeIter iter;
	GValue value = {0, };
	const gchar *text;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));

	if (! ctk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	ctk_tree_model_get_value (dlg->suggestions_list_model, &iter,
			    COLUMN_SUGGESTIONS,
			    &value);

	text = g_value_get_string (&value);

	ctk_entry_set_text (CTK_ENTRY (dlg->word_entry), text);

	g_value_unset (&value);
}

static void
check_word_button_clicked_handler (GtkButton *button, LapizSpellCheckerDialog *dlg)
{
	const gchar *word;
	gssize len;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));

	word = ctk_entry_get_text (CTK_ENTRY (dlg->word_entry));
	len = strlen (word);
	g_return_if_fail (len > 0);

	if (lapiz_spell_checker_check_word (dlg->spell_checker, word, len))
	{
		GtkListStore *store;
		GtkTreeIter iter;

		store = CTK_LIST_STORE (dlg->suggestions_list_model);
		ctk_list_store_clear (store);

		ctk_list_store_append (store, &iter);
		ctk_list_store_set (store, &iter,
		                    /* Translators: Displayed in the "Check Spelling" dialog if the current word isn't misspelled */
				    COLUMN_SUGGESTIONS, _("(correct spelling)"),
				    -1);

		ctk_widget_set_sensitive (dlg->suggestions_list, FALSE);
	}
	else
	{
		GSList *sug;

		sug = lapiz_spell_checker_get_suggestions (dlg->spell_checker,
							   word,
							   len);

		update_suggestions_list_model (dlg, sug);

		/* free the suggestion list */
		g_slist_foreach (sug, (GFunc)g_free, NULL);
		g_slist_free (sug);
	}
}

static void
add_word_button_clicked_handler (GtkButton *button, LapizSpellCheckerDialog *dlg)
{
	gchar *word;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));
	g_return_if_fail (dlg->misspelled_word != NULL);

	lapiz_spell_checker_add_word_to_personal (dlg->spell_checker,
						  dlg->misspelled_word,
						  -1);

	word = g_strdup (dlg->misspelled_word);

	g_signal_emit (G_OBJECT (dlg), signals [ADD_WORD_TO_PERSONAL], 0, word);

	g_free (word);
}

static void
ignore_button_clicked_handler (GtkButton *button, LapizSpellCheckerDialog *dlg)
{
	gchar *word;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));
	g_return_if_fail (dlg->misspelled_word != NULL);

	word = g_strdup (dlg->misspelled_word);

	g_signal_emit (G_OBJECT (dlg), signals [IGNORE], 0, word);

	g_free (word);
}

static void
ignore_all_button_clicked_handler (GtkButton *button, LapizSpellCheckerDialog *dlg)
{
	gchar *word;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));
	g_return_if_fail (dlg->misspelled_word != NULL);

	lapiz_spell_checker_add_word_to_session (dlg->spell_checker,
						 dlg->misspelled_word,
						 -1);

	word = g_strdup (dlg->misspelled_word);

	g_signal_emit (G_OBJECT (dlg), signals [IGNORE_ALL], 0, word);

	g_free (word);
}

static void
change_button_clicked_handler (GtkButton *button, LapizSpellCheckerDialog *dlg)
{
	const gchar *entry_text;
	gchar *change;
	gchar *word;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));
	g_return_if_fail (dlg->misspelled_word != NULL);

	entry_text = ctk_entry_get_text (CTK_ENTRY (dlg->word_entry));
	g_return_if_fail (entry_text != NULL);
	g_return_if_fail (*entry_text != '\0');
	change = g_strdup (entry_text);

	lapiz_spell_checker_set_correction (dlg->spell_checker,
					    dlg->misspelled_word, -1,
					    change, -1);

	word = g_strdup (dlg->misspelled_word);

	g_signal_emit (G_OBJECT (dlg), signals [CHANGE], 0, word, change);

	g_free (word);
	g_free (change);
}

/* double click on one of the suggestions is like clicking on "change" */
static void
suggestions_list_row_activated_handler (GtkTreeView *view,
		GtkTreePath *path,
		GtkTreeViewColumn *column,
		LapizSpellCheckerDialog *dlg)
{
	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));

	change_button_clicked_handler (CTK_BUTTON (dlg->change_button), dlg);
}

static void
change_all_button_clicked_handler (GtkButton *button, LapizSpellCheckerDialog *dlg)
{
	const gchar *entry_text;
	gchar *change;
	gchar *word;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));
	g_return_if_fail (dlg->misspelled_word != NULL);

	entry_text = ctk_entry_get_text (CTK_ENTRY (dlg->word_entry));
	g_return_if_fail (entry_text != NULL);
	g_return_if_fail (*entry_text != '\0');
	change = g_strdup (entry_text);

	lapiz_spell_checker_set_correction (dlg->spell_checker,
					    dlg->misspelled_word, -1,
					    change, -1);

	word = g_strdup (dlg->misspelled_word);

	g_signal_emit (G_OBJECT (dlg), signals [CHANGE_ALL], 0, word, change);

	g_free (word);
	g_free (change);
}

void
lapiz_spell_checker_dialog_set_completed (LapizSpellCheckerDialog *dlg)
{
	gchar *tmp;

	g_return_if_fail (LAPIZ_IS_SPELL_CHECKER_DIALOG (dlg));

	tmp = g_strdup_printf("<b>%s</b>", _("Completed spell checking"));
	ctk_label_set_label (CTK_LABEL (dlg->misspelled_word_label),
			     tmp);
	g_free (tmp);

	ctk_list_store_clear (CTK_LIST_STORE (dlg->suggestions_list_model));
	ctk_entry_set_text (CTK_ENTRY (dlg->word_entry), "");

	ctk_widget_set_sensitive (dlg->word_entry, FALSE);
	ctk_widget_set_sensitive (dlg->check_word_button, FALSE);
	ctk_widget_set_sensitive (dlg->ignore_button, FALSE);
	ctk_widget_set_sensitive (dlg->ignore_all_button, FALSE);
	ctk_widget_set_sensitive (dlg->change_button, FALSE);
	ctk_widget_set_sensitive (dlg->change_all_button, FALSE);
	ctk_widget_set_sensitive (dlg->add_word_button, FALSE);
	ctk_widget_set_sensitive (dlg->suggestions_list, FALSE);
}

