/*
 * lapiz-close-confirmation-dialog.c
 * This file is part of lapiz
 *
 * Copyright (C) 2004-2005 GNOME Foundation
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
 * Modified by the lapiz Team, 2004-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "lapiz-close-confirmation-dialog.h"
#include <lapiz/lapiz-app.h>
#include <lapiz/lapiz-utils.h>
#include <lapiz/lapiz-window.h>


/* Properties */
enum
{
	PROP_0,
	PROP_UNSAVED_DOCUMENTS,
	PROP_LOGOUT_MODE
};

/* Mode */
enum
{
	SINGLE_DOC_MODE,
	MULTIPLE_DOCS_MODE
};

/* Columns */
enum
{
	SAVE_COLUMN,
	NAME_COLUMN,
	DOC_COLUMN, /* a handy pointer to the document */
	N_COLUMNS
};

struct _LapizCloseConfirmationDialogPrivate
{
	gboolean     logout_mode;

	GList       *unsaved_documents;

	GList       *selected_documents;

	CtkTreeModel *list_store;

	gboolean     disable_save_to_disk;
};

#define GET_MODE(priv) (((priv->unsaved_documents != NULL) && \
			 (priv->unsaved_documents->next == NULL)) ? \
			  SINGLE_DOC_MODE : MULTIPLE_DOCS_MODE)

G_DEFINE_TYPE_WITH_PRIVATE (LapizCloseConfirmationDialog, lapiz_close_confirmation_dialog, CTK_TYPE_DIALOG)

static void 	 set_unsaved_document 		(LapizCloseConfirmationDialog *dlg,
						 const GList                  *list);

static GList 	*get_selected_docs 		(CtkTreeModel                 *store);

/*  Since we connect in the costructor we are sure this handler will be called
 *  before the user ones
 */
static void
response_cb (LapizCloseConfirmationDialog *dlg,
             gint                          response_id,
             gpointer                      data G_GNUC_UNUSED)
{
	LapizCloseConfirmationDialogPrivate *priv;

	g_return_if_fail (LAPIZ_IS_CLOSE_CONFIRMATION_DIALOG (dlg));

	priv = dlg->priv;

	if (priv->selected_documents != NULL)
		g_list_free (priv->selected_documents);

	if (response_id == CTK_RESPONSE_YES)
	{
		if (GET_MODE (priv) == SINGLE_DOC_MODE)
		{
			priv->selected_documents =
				g_list_copy (priv->unsaved_documents);
		}
		else
		{
			g_return_if_fail (priv->list_store);

			priv->selected_documents =
				get_selected_docs (priv->list_store);
		}
	}
	else
		priv->selected_documents = NULL;
}

static void
set_logout_mode (LapizCloseConfirmationDialog *dlg,
		 gboolean                      logout_mode)
{
	dlg->priv->logout_mode = logout_mode;

	if (logout_mode)
	{
		ctk_dialog_add_button (CTK_DIALOG (dlg),
				       _("Log Out _without Saving"),
				       CTK_RESPONSE_NO);

		lapiz_dialog_add_button (CTK_DIALOG (dlg),
					 _("_Cancel Logout"),
					 "process-stop",
					 CTK_RESPONSE_CANCEL);
	}
	else
	{
		ctk_dialog_add_button (CTK_DIALOG (dlg),
				       _("Close _without Saving"),
				       CTK_RESPONSE_NO);

		lapiz_dialog_add_button (CTK_DIALOG (dlg),
					 _("_Cancel"),
					 "process-stop",
					 CTK_RESPONSE_CANCEL);
	}

	if (dlg->priv->disable_save_to_disk)
	{
		ctk_dialog_set_default_response	(CTK_DIALOG (dlg),
						 CTK_RESPONSE_NO);
	}
	else
	{
		const gchar *icon_id = "document-save";

		if (GET_MODE (dlg->priv) == SINGLE_DOC_MODE)
		{
			LapizDocument *doc;

			doc = LAPIZ_DOCUMENT (dlg->priv->unsaved_documents->data);

			if (lapiz_document_get_readonly (doc) ||
			    lapiz_document_is_untitled (doc))
				icon_id = "document-save-as";
		}

		if (g_strcmp0 (icon_id, "document-save") == 0)
			lapiz_dialog_add_button (CTK_DIALOG (dlg),
						 _("_Save"),
						 icon_id,
						 CTK_RESPONSE_YES);
		else
			lapiz_dialog_add_button (CTK_DIALOG (dlg),
						 _("Save _As"),
						 icon_id,
						 CTK_RESPONSE_YES);

		ctk_dialog_set_default_response	(CTK_DIALOG (dlg),
						 CTK_RESPONSE_YES);
	}
}

static void
lapiz_close_confirmation_dialog_init (LapizCloseConfirmationDialog *dlg)
{
	AtkObject *atk_obj;

	dlg->priv = lapiz_close_confirmation_dialog_get_instance_private (dlg);

	dlg->priv->disable_save_to_disk =
			lapiz_app_get_lockdown (lapiz_app_get_default ())
			& LAPIZ_LOCKDOWN_SAVE_TO_DISK;

	ctk_container_set_border_width (CTK_CONTAINER (dlg), 5);
	ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			     14);
	ctk_window_set_resizable (CTK_WINDOW (dlg), FALSE);
	ctk_window_set_skip_taskbar_hint (CTK_WINDOW (dlg), TRUE);

	ctk_window_set_title (CTK_WINDOW (dlg), "");

	ctk_window_set_modal (CTK_WINDOW (dlg), TRUE);
	ctk_window_set_destroy_with_parent (CTK_WINDOW (dlg), TRUE);

	atk_obj = ctk_widget_get_accessible (CTK_WIDGET (dlg));
	atk_object_set_role (atk_obj, ATK_ROLE_ALERT);
	atk_object_set_name (atk_obj, _("Question"));

	g_signal_connect (dlg,
			  "response",
			  G_CALLBACK (response_cb),
			  NULL);
}

static void
lapiz_close_confirmation_dialog_finalize (GObject *object)
{
	LapizCloseConfirmationDialogPrivate *priv;

	priv = LAPIZ_CLOSE_CONFIRMATION_DIALOG (object)->priv;

	if (priv->unsaved_documents != NULL)
		g_list_free (priv->unsaved_documents);

	if (priv->selected_documents != NULL)
		g_list_free (priv->selected_documents);

	/* Call the parent's destructor */
	G_OBJECT_CLASS (lapiz_close_confirmation_dialog_parent_class)->finalize (object);
}

static void
lapiz_close_confirmation_dialog_set_property (GObject      *object,
					      guint         prop_id,
					      const GValue *value,
					      GParamSpec   *pspec)
{
	LapizCloseConfirmationDialog *dlg;

	dlg = LAPIZ_CLOSE_CONFIRMATION_DIALOG (object);

	switch (prop_id)
	{
		case PROP_UNSAVED_DOCUMENTS:
			set_unsaved_document (dlg, g_value_get_pointer (value));
			break;

		case PROP_LOGOUT_MODE:
			set_logout_mode (dlg, g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_close_confirmation_dialog_get_property (GObject    *object,
					      guint       prop_id,
					      GValue     *value,
					      GParamSpec *pspec)
{
	LapizCloseConfirmationDialogPrivate *priv;

	priv = LAPIZ_CLOSE_CONFIRMATION_DIALOG (object)->priv;

	switch( prop_id )
	{
		case PROP_UNSAVED_DOCUMENTS:
			g_value_set_pointer (value, priv->unsaved_documents);
			break;

		case PROP_LOGOUT_MODE:
			g_value_set_boolean (value, priv->logout_mode);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_close_confirmation_dialog_class_init (LapizCloseConfirmationDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = lapiz_close_confirmation_dialog_set_property;
	gobject_class->get_property = lapiz_close_confirmation_dialog_get_property;
	gobject_class->finalize = lapiz_close_confirmation_dialog_finalize;

	g_object_class_install_property (gobject_class,
					 PROP_UNSAVED_DOCUMENTS,
					 g_param_spec_pointer ("unsaved_documents",
						 	       "Unsaved Documents",
							       "List of Unsaved Documents",
							       (G_PARAM_READWRITE |
							        G_PARAM_CONSTRUCT_ONLY)));

	g_object_class_install_property (gobject_class,
					 PROP_LOGOUT_MODE,
					 g_param_spec_boolean ("logout_mode",
						 	       "Logout Mode",
							       "Whether the dialog is in logout mode",
							       FALSE,
							       (G_PARAM_READWRITE |
							        G_PARAM_CONSTRUCT_ONLY)));
}

static GList *
get_selected_docs (CtkTreeModel *store)
{
	GList      *list;
	gboolean     valid;
	CtkTreeIter  iter;

	list = NULL;
	valid = ctk_tree_model_get_iter_first (store, &iter);

	while (valid)
	{
		gboolean       to_save;
		LapizDocument *doc;

		ctk_tree_model_get (store, &iter,
				    SAVE_COLUMN, &to_save,
				    DOC_COLUMN, &doc,
				    -1);
		if (to_save)
			list = g_list_prepend (list, doc);

		valid = ctk_tree_model_iter_next (store, &iter);
	}

	list = g_list_reverse (list);

	return list;
}

GList *
lapiz_close_confirmation_dialog_get_selected_documents (LapizCloseConfirmationDialog *dlg)
{
	g_return_val_if_fail (LAPIZ_IS_CLOSE_CONFIRMATION_DIALOG (dlg), NULL);

	return g_list_copy (dlg->priv->selected_documents);
}

CtkWidget *
lapiz_close_confirmation_dialog_new (CtkWindow *parent,
				     GList     *unsaved_documents,
				     gboolean   logout_mode)
{
	CtkWidget *dlg;
	g_return_val_if_fail (unsaved_documents != NULL, NULL);

	dlg = CTK_WIDGET (g_object_new (LAPIZ_TYPE_CLOSE_CONFIRMATION_DIALOG,
				        "unsaved_documents", unsaved_documents,
				        "logout_mode", logout_mode,
				        NULL));
	g_return_val_if_fail (dlg != NULL, NULL);

	if (parent != NULL)
	{
		ctk_window_group_add_window (lapiz_window_get_group (LAPIZ_WINDOW (parent)),
					     CTK_WINDOW (dlg));

		ctk_window_set_transient_for (CTK_WINDOW (dlg), parent);
	}

	return dlg;
}

CtkWidget *
lapiz_close_confirmation_dialog_new_single (CtkWindow     *parent,
					    LapizDocument *doc,
					    gboolean       logout_mode)
{
	CtkWidget *dlg;
	GList *unsaved_documents;
	g_return_val_if_fail (doc != NULL, NULL);

	unsaved_documents = g_list_prepend (NULL, doc);

	dlg = lapiz_close_confirmation_dialog_new (parent,
						   unsaved_documents,
						   logout_mode);

	g_list_free (unsaved_documents);

	return dlg;
}

static gchar *
get_text_secondary_label (LapizDocument *doc)
{
	glong  seconds;
	gchar *secondary_msg;

	seconds = MAX (1, _lapiz_document_get_seconds_since_last_save_or_load (doc));

	if (seconds < 55)
	{
		secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last %ld second "
					    	  "will be permanently lost.",
						  "If you don't save, changes from the last %ld seconds "
					    	  "will be permanently lost.",
						  seconds),
					seconds);
	}
	else if (seconds < 75) /* 55 <= seconds < 75 */
	{
		secondary_msg = g_strdup (_("If you don't save, changes from the last minute "
					    "will be permanently lost."));
	}
	else if (seconds < 110) /* 75 <= seconds < 110 */
	{
		secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last minute and %ld "
						  "second will be permanently lost.",
						  "If you don't save, changes from the last minute and %ld "
						  "seconds will be permanently lost.",
						  seconds - 60 ),
					seconds - 60);
	}
	else if (seconds < 3600)
	{
		secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last %ld minute "
					    	  "will be permanently lost.",
						  "If you don't save, changes from the last %ld minutes "
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
			secondary_msg = g_strdup (_("If you don't save, changes from the last hour "
						    "will be permanently lost."));
		}
		else
		{
			secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last hour and %d "
						  "minute will be permanently lost.",
						  "If you don't save, changes from the last hour and %d "
						  "minutes will be permanently lost.",
						  minutes),
					minutes);
		}
	}
	else
	{
		gint hours;

		hours = seconds / 3600;

		secondary_msg = g_strdup_printf (
					ngettext ("If you don't save, changes from the last %d hour "
					    	  "will be permanently lost.",
						  "If you don't save, changes from the last %d hours "
					    	  "will be permanently lost.",
						  hours),
					hours);
	}

	return secondary_msg;
}

static void
build_single_doc_dialog (LapizCloseConfirmationDialog *dlg)
{
	CtkWidget     *hbox;
	CtkWidget     *vbox;
	CtkWidget     *primary_label;
	CtkWidget     *secondary_label;
	CtkWidget     *image;
	LapizDocument *doc;
	gchar         *doc_name;
	gchar         *str;
	gchar         *markup_str;

	g_return_if_fail (dlg->priv->unsaved_documents->data != NULL);
	doc = LAPIZ_DOCUMENT (dlg->priv->unsaved_documents->data);

	/* Image */
	image = ctk_image_new_from_icon_name ("dialog-warning",
					  CTK_ICON_SIZE_DIALOG);
	ctk_widget_set_halign (image, CTK_ALIGN_START);
	ctk_widget_set_valign (image, CTK_ALIGN_END);

	/* Primary label */
	primary_label = ctk_label_new (NULL);
	ctk_label_set_line_wrap (CTK_LABEL (primary_label), TRUE);
	ctk_label_set_use_markup (CTK_LABEL (primary_label), TRUE);
	ctk_label_set_xalign (CTK_LABEL (primary_label), 0.0);
	ctk_label_set_selectable (CTK_LABEL (primary_label), TRUE);
	ctk_widget_set_can_focus (CTK_WIDGET (primary_label), FALSE);
	ctk_label_set_max_width_chars (CTK_LABEL (primary_label), 72);

	doc_name = lapiz_document_get_short_name_for_display (doc);

	if (dlg->priv->disable_save_to_disk)
	{
		str = g_markup_printf_escaped (_("Changes to document \"%s\" will be permanently lost."),
					       doc_name);
	}
	else
	{
		str = g_markup_printf_escaped (_("Save changes to document \"%s\" before closing?"),
					       doc_name);
	}

	g_free (doc_name);

	markup_str = g_strconcat ("<span weight=\"bold\" size=\"larger\">", str, "</span>", NULL);
	g_free (str);

	ctk_label_set_markup (CTK_LABEL (primary_label), markup_str);
	g_free (markup_str);

	/* Secondary label */
	if (dlg->priv->disable_save_to_disk)
		str = g_strdup (_("Saving has been disabled by the system administrator."));
	else
		str = get_text_secondary_label (doc);
	secondary_label = ctk_label_new (str);
	g_free (str);
	ctk_label_set_line_wrap (CTK_LABEL (secondary_label), TRUE);
	ctk_label_set_xalign (CTK_LABEL (secondary_label), 0.0);
	ctk_label_set_selectable (CTK_LABEL (secondary_label), TRUE);
	ctk_widget_set_can_focus (CTK_WIDGET (secondary_label), FALSE);
	ctk_label_set_max_width_chars (CTK_LABEL (secondary_label), 72);

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
	ctk_container_set_border_width (CTK_CONTAINER (hbox), 5);

	ctk_box_pack_start (CTK_BOX (hbox), image, FALSE, FALSE, 0);

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 12);

	ctk_box_pack_start (CTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	ctk_box_pack_start (CTK_BOX (vbox), primary_label, FALSE, FALSE, 0);

	ctk_box_pack_start (CTK_BOX (vbox), secondary_label, FALSE, FALSE, 0);

	ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			    hbox,
	                    FALSE,
			    FALSE,
			    0);

	ctk_widget_show_all (hbox);
}

static void
populate_model (CtkTreeModel *store, GList *docs)
{
	CtkTreeIter iter;

	while (docs != NULL)
	{
		LapizDocument *doc;
		gchar *name;

		doc = LAPIZ_DOCUMENT (docs->data);

		name = lapiz_document_get_short_name_for_display (doc);

		ctk_list_store_append (CTK_LIST_STORE (store), &iter);
		ctk_list_store_set (CTK_LIST_STORE (store), &iter,
				    SAVE_COLUMN, TRUE,
				    NAME_COLUMN, name,
				    DOC_COLUMN, doc,
			            -1);

		g_free (name);

		docs = g_list_next (docs);
	}
}

static void
save_toggled (CtkCellRendererToggle *renderer G_GNUC_UNUSED,
	      gchar                 *path_str,
	      CtkTreeModel          *store)
{
	CtkTreePath *path = ctk_tree_path_new_from_string (path_str);
	CtkTreeIter iter;
	gboolean active;

	ctk_tree_model_get_iter (store, &iter, path);
	ctk_tree_model_get (store, &iter, SAVE_COLUMN, &active, -1);

	active ^= 1;

	ctk_list_store_set (CTK_LIST_STORE (store), &iter,
			    SAVE_COLUMN, active, -1);

	ctk_tree_path_free (path);
}

static CtkWidget *
create_treeview (LapizCloseConfirmationDialogPrivate *priv)
{
	CtkListStore *store;
	CtkWidget *treeview;
	CtkCellRenderer *renderer;
	CtkTreeViewColumn *column;

	treeview = ctk_tree_view_new ();
	ctk_widget_set_size_request (treeview, 260, 120);
	ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (treeview), FALSE);
	ctk_tree_view_set_enable_search (CTK_TREE_VIEW (treeview), FALSE);

	/* Create and populate the model */
	store = ctk_list_store_new (N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
	populate_model (CTK_TREE_MODEL (store), priv->unsaved_documents);

	/* Set model to the treeview */
	ctk_tree_view_set_model (CTK_TREE_VIEW (treeview), CTK_TREE_MODEL (store));
	g_object_unref (store);

	priv->list_store = CTK_TREE_MODEL (store);

	/* Add columns */
	if (!priv->disable_save_to_disk)
	{
		renderer = ctk_cell_renderer_toggle_new ();
		g_signal_connect (renderer, "toggled",
				  G_CALLBACK (save_toggled), store);

		column = ctk_tree_view_column_new_with_attributes ("Save?",
								   renderer,
								   "active",
								   SAVE_COLUMN,
								   NULL);
		ctk_tree_view_append_column (CTK_TREE_VIEW (treeview), column);
	}

	renderer = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes ("Name",
							   renderer,
							   "text",
							   NAME_COLUMN,
							   NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (treeview), column);

	return treeview;
}

static void
build_multiple_docs_dialog (LapizCloseConfirmationDialog *dlg)
{
	LapizCloseConfirmationDialogPrivate *priv;
	CtkWidget   *hbox;
	CtkWidget   *image;
	CtkWidget   *vbox;
	CtkWidget   *primary_label;
	CtkWidget   *vbox2;
	CtkWidget   *select_label;
	CtkWidget   *scrolledwindow;
	CtkWidget   *treeview;
	CtkWidget   *secondary_label;
	CdkDisplay  *display;
	CdkRectangle mon_geo;
	gchar       *str;
	gchar       *markup_str;
	gint         new_width;
	gint         new_height;
	gint         max_height;

	priv = dlg->priv;

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
	ctk_container_set_border_width (CTK_CONTAINER (hbox), 5);
	ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			    hbox, TRUE, TRUE, 0);

	/* Image */
	image = ctk_image_new_from_icon_name ("dialog-warning",
					  CTK_ICON_SIZE_DIALOG);
	ctk_widget_set_halign (image, CTK_ALIGN_CENTER);
	ctk_widget_set_valign (image, CTK_ALIGN_START);
	ctk_box_pack_start (CTK_BOX (hbox), image, FALSE, FALSE, 0);

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 12);
	ctk_box_pack_start (CTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	/* Primary label */
	primary_label = ctk_label_new (NULL);
	ctk_label_set_line_wrap (CTK_LABEL (primary_label), TRUE);
	ctk_label_set_use_markup (CTK_LABEL (primary_label), TRUE);
	ctk_label_set_xalign (CTK_LABEL (primary_label), 0.0);
	ctk_label_set_selectable (CTK_LABEL (primary_label), TRUE);
	ctk_widget_set_can_focus (CTK_WIDGET (primary_label), FALSE);
	ctk_label_set_max_width_chars (CTK_LABEL (primary_label), 72);

	if (priv->disable_save_to_disk)
		str = g_strdup_printf (
				ngettext ("Changes to %d document will be permanently lost.",
					  "Changes to %d documents will be permanently lost.",
					  g_list_length (priv->unsaved_documents)),
				g_list_length (priv->unsaved_documents));
	else
		str = g_strdup_printf (
				ngettext ("There is %d document with unsaved changes. "
					  "Save changes before closing?",
					  "There are %d documents with unsaved changes. "
					  "Save changes before closing?",
					  g_list_length (priv->unsaved_documents)),
				g_list_length (priv->unsaved_documents));

	markup_str = g_strconcat ("<span weight=\"bold\" size=\"larger\">", str, "</span>", NULL);
	g_free (str);

	ctk_label_set_markup (CTK_LABEL (primary_label), markup_str);
	g_free (markup_str);
	ctk_box_pack_start (CTK_BOX (vbox), primary_label, FALSE, FALSE, 0);

	vbox2 = ctk_box_new (CTK_ORIENTATION_VERTICAL, 8);
	ctk_box_pack_start (CTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

	if (priv->disable_save_to_disk)
		select_label = ctk_label_new_with_mnemonic (_("Docum_ents with unsaved changes:"));
	else
		select_label = ctk_label_new_with_mnemonic (_("S_elect the documents you want to save:"));

	ctk_box_pack_start (CTK_BOX (vbox2), select_label, FALSE, FALSE, 0);
	ctk_label_set_line_wrap (CTK_LABEL (select_label), TRUE);
	ctk_label_set_max_width_chars (CTK_LABEL (select_label), 72);
	ctk_label_set_xalign (CTK_LABEL (select_label), 0.0);
	ctk_label_set_selectable (CTK_LABEL (select_label), TRUE);
	ctk_widget_set_can_focus (CTK_WIDGET (select_label), FALSE);

	scrolledwindow = ctk_scrolled_window_new (NULL, NULL);
	ctk_scrolled_window_set_propagate_natural_height (CTK_SCROLLED_WINDOW (scrolledwindow), TRUE);
	ctk_box_pack_start (CTK_BOX (vbox2), scrolledwindow, TRUE, TRUE, 0);
	ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (scrolledwindow),
					CTK_POLICY_AUTOMATIC,
					CTK_POLICY_AUTOMATIC);
	ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (scrolledwindow),
					     CTK_SHADOW_IN);

	treeview = create_treeview (priv);
	ctk_container_add (CTK_CONTAINER (scrolledwindow), treeview);

	/* Secondary label */
	if (priv->disable_save_to_disk)
		secondary_label = ctk_label_new (_("Saving has been disabled by the system administrator."));
	else
		secondary_label = ctk_label_new (_("If you don't save, "
						   "all your changes will be permanently lost."));

	ctk_box_pack_start (CTK_BOX (vbox2), secondary_label, FALSE, FALSE, 0);
	ctk_label_set_line_wrap (CTK_LABEL (secondary_label), TRUE);
	ctk_label_set_xalign (CTK_LABEL (secondary_label), 0.0);
	ctk_label_set_selectable (CTK_LABEL (secondary_label), TRUE);
	ctk_widget_set_can_focus (CTK_WIDGET (secondary_label), FALSE);
	ctk_label_set_max_width_chars (CTK_LABEL (secondary_label), 72);

	ctk_label_set_mnemonic_widget (CTK_LABEL (select_label), treeview);

	ctk_widget_show_all (hbox);

	ctk_window_get_size (CTK_WINDOW (CTK_DIALOG (dlg)), &new_width, &new_height);

	display = ctk_widget_get_display (scrolledwindow);

	cdk_monitor_get_geometry (cdk_display_get_primary_monitor (display), &mon_geo);

	max_height = mon_geo.height * 70 / 100;

	if (new_height > max_height)
	{
		ctk_window_set_resizable (CTK_WINDOW (CTK_DIALOG (dlg)), TRUE);
		ctk_window_resize (CTK_WINDOW (CTK_DIALOG (dlg)), new_width, max_height);
	}
	else
		ctk_window_set_default_size (CTK_WINDOW (CTK_DIALOG (dlg)), new_width, new_height);
}

static void
set_unsaved_document (LapizCloseConfirmationDialog *dlg,
		      const GList                  *list)
{
	LapizCloseConfirmationDialogPrivate *priv;

	g_return_if_fail (list != NULL);

	priv = dlg->priv;
	g_return_if_fail (priv->unsaved_documents == NULL);

	priv->unsaved_documents = g_list_copy ((GList *)list);

	if (GET_MODE (priv) == SINGLE_DOC_MODE)
	{
		build_single_doc_dialog (dlg);
	}
	else
	{
		build_multiple_docs_dialog (dlg);
	}
}

const GList *
lapiz_close_confirmation_dialog_get_unsaved_documents (LapizCloseConfirmationDialog *dlg)
{
	g_return_val_if_fail (LAPIZ_IS_CLOSE_CONFIRMATION_DIALOG (dlg), NULL);

	return dlg->priv->unsaved_documents;
}
