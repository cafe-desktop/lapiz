/*
 * lapiz-documents-panel.c
 * This file is part of lapiz
 *
 * Copyright (C) 2005 - Paolo Maggi
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

#include "lapiz-documents-panel.h"
#include "lapiz-utils.h"
#include "lapiz-notebook.h"

#include <glib/gi18n.h>

struct _LapizDocumentsPanelPrivate
{
	LapizWindow  *window;

	CtkWidget    *treeview;
	CtkTreeModel *model;

	guint         adding_tab : 1;
	guint         is_reodering : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizDocumentsPanel, lapiz_documents_panel, CTK_TYPE_BOX)

enum
{
	PROP_0,
	PROP_WINDOW
};

enum
{
	PIXBUF_COLUMN,
	NAME_COLUMN,
	TAB_COLUMN,
	N_COLUMNS
};

#define MAX_DOC_NAME_LENGTH 60

static gchar *
tab_get_name (LapizTab *tab)
{
	LapizDocument *doc;
	gchar *name;
	gchar *docname;
	gchar *tab_name;

	g_return_val_if_fail (LAPIZ_IS_TAB (tab), NULL);

	doc = lapiz_tab_get_document (tab);

	name = lapiz_document_get_short_name_for_display (doc);

	/* Truncate the name so it doesn't get insanely wide. */
	docname = lapiz_utils_str_middle_truncate (name, MAX_DOC_NAME_LENGTH);

	if (ctk_text_buffer_get_modified (CTK_TEXT_BUFFER (doc)))
	{
		if (lapiz_document_get_readonly (doc))
		{
			tab_name = g_markup_printf_escaped ("<i>%s</i> [<i>%s</i>]",
							    docname,
							    _("Read-Only"));
		}
		else
		{
			tab_name = g_markup_printf_escaped ("<i>%s</i>",
							    docname);
		}
	}
	else
	{
		if (lapiz_document_get_readonly (doc))
		{
			tab_name = g_markup_printf_escaped ("%s [<i>%s</i>]",
							    docname,
							    _("Read-Only"));
		}
		else
		{
			tab_name = g_markup_escape_text (docname, -1);
		}
	}

	g_free (docname);
	g_free (name);

	return tab_name;
}

static void
get_iter_from_tab (LapizDocumentsPanel *panel, LapizTab *tab, CtkTreeIter *iter)
{
	gint num;
	CtkWidget *nb;
	CtkTreePath *path;

	nb = _lapiz_window_get_notebook (panel->priv->window);
	num = ctk_notebook_page_num (CTK_NOTEBOOK (nb),
				     CTK_WIDGET (tab));

	path = ctk_tree_path_new_from_indices (num, -1);
	ctk_tree_model_get_iter (panel->priv->model,
		                 iter,
		                 path);
	ctk_tree_path_free (path);
}

static void
window_active_tab_changed (LapizWindow         *window,
			   LapizTab            *tab,
			   LapizDocumentsPanel *panel)
{
	g_return_if_fail (tab != NULL);

	if (!_lapiz_window_is_removing_tabs (window))
	{
		CtkTreeIter iter;
		CtkTreeSelection *selection;

		get_iter_from_tab (panel, tab, &iter);

		if (ctk_list_store_iter_is_valid (CTK_LIST_STORE (panel->priv->model),
						  &iter))
		{
			selection = ctk_tree_view_get_selection (
					CTK_TREE_VIEW (panel->priv->treeview));

			ctk_tree_selection_select_iter (selection, &iter);
		}
	}
}

static void
refresh_list (LapizDocumentsPanel *panel)
{
	/* TODO: refresh the list only if the panel is visible */

	GList *tabs;
	GList *l;
	CtkWidget *nb;
	CtkListStore *list_store;
	LapizTab *active_tab;

	/* g_debug ("refresh_list"); */

	list_store = CTK_LIST_STORE (panel->priv->model);

	ctk_list_store_clear (list_store);

	active_tab = lapiz_window_get_active_tab (panel->priv->window);

	nb = _lapiz_window_get_notebook (panel->priv->window);

	tabs = ctk_container_get_children (CTK_CONTAINER (nb));
	l = tabs;

	panel->priv->adding_tab = TRUE;

	while (l != NULL)
	{
		CdkPixbuf *pixbuf;
		gchar *name;
		CtkTreeIter iter;

		name = tab_get_name (LAPIZ_TAB (l->data));
		pixbuf = _lapiz_tab_get_icon (LAPIZ_TAB (l->data));

		/* Add a new row to the model */
		ctk_list_store_append (list_store, &iter);
		ctk_list_store_set (list_store,
				    &iter,
				    PIXBUF_COLUMN, pixbuf,
				    NAME_COLUMN, name,
				    TAB_COLUMN, l->data,
				    -1);

		g_free (name);
		if (pixbuf != NULL)
			g_object_unref (pixbuf);

		if (l->data == active_tab)
		{
			CtkTreeSelection *selection;

			selection = ctk_tree_view_get_selection (
					CTK_TREE_VIEW (panel->priv->treeview));

			ctk_tree_selection_select_iter (selection, &iter);
		}

		l = g_list_next (l);
	}

	panel->priv->adding_tab = FALSE;

	g_list_free (tabs);
}

static void
sync_name_and_icon (LapizTab            *tab,
		    GParamSpec          *pspec,
		    LapizDocumentsPanel *panel)
{
	CdkPixbuf *pixbuf;
	gchar *name;
	CtkTreeIter iter;

	get_iter_from_tab (panel, tab, &iter);

	name = tab_get_name (tab);
	pixbuf = _lapiz_tab_get_icon (tab);

	ctk_list_store_set (CTK_LIST_STORE (panel->priv->model),
			    &iter,
			    PIXBUF_COLUMN, pixbuf,
			    NAME_COLUMN, name,
			    TAB_COLUMN, tab,
			    -1);

	g_free (name);
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
}

static void
window_tab_removed (LapizWindow         *window,
		    LapizTab            *tab,
		    LapizDocumentsPanel *panel)
{
	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_name_and_icon),
					      panel);

	if (_lapiz_window_is_removing_tabs (window))
		ctk_list_store_clear (CTK_LIST_STORE (panel->priv->model));
	else
		refresh_list (panel);
}

static void
window_tab_added (LapizWindow         *window,
		  LapizTab            *tab,
		  LapizDocumentsPanel *panel)
{
	CtkTreeIter iter;
	CtkTreeIter sibling;
	CdkPixbuf *pixbuf;
	gchar *name;

	g_signal_connect (tab,
			 "notify::name",
			  G_CALLBACK (sync_name_and_icon),
			  panel);

	g_signal_connect (tab,
			 "notify::state",
			  G_CALLBACK (sync_name_and_icon),
			  panel);

	get_iter_from_tab (panel, tab, &sibling);

	panel->priv->adding_tab = TRUE;

	if (ctk_list_store_iter_is_valid (CTK_LIST_STORE (panel->priv->model),
					  &sibling))
	{
		ctk_list_store_insert_after (CTK_LIST_STORE (panel->priv->model),
					     &iter,
					     &sibling);
	}
	else
	{
		LapizTab *active_tab;

		ctk_list_store_append (CTK_LIST_STORE (panel->priv->model),
				       &iter);

		active_tab = lapiz_window_get_active_tab (panel->priv->window);

		if (tab == active_tab)
		{
			CtkTreeSelection *selection;

			selection = ctk_tree_view_get_selection (
						CTK_TREE_VIEW (panel->priv->treeview));

			ctk_tree_selection_select_iter (selection, &iter);
		}
	}

	name = tab_get_name (tab);
	pixbuf = _lapiz_tab_get_icon (tab);

	ctk_list_store_set (CTK_LIST_STORE (panel->priv->model),
			    &iter,
		            PIXBUF_COLUMN, pixbuf,
		            NAME_COLUMN, name,
		            TAB_COLUMN, tab,
		            -1);

	g_free (name);
	if (pixbuf != NULL)
		g_object_unref (pixbuf);

	panel->priv->adding_tab = FALSE;
}

static void
window_tabs_reordered (LapizWindow         *window,
		       LapizDocumentsPanel *panel)
{
	if (panel->priv->is_reodering)
		return;

	refresh_list (panel);
}

static void
set_window (LapizDocumentsPanel *panel,
	    LapizWindow         *window)
{
	g_return_if_fail (panel->priv->window == NULL);
	g_return_if_fail (LAPIZ_IS_WINDOW (window));

	panel->priv->window = g_object_ref (window);

	g_signal_connect (window,
			  "tab_added",
			  G_CALLBACK (window_tab_added),
			  panel);
	g_signal_connect (window,
			  "tab_removed",
			  G_CALLBACK (window_tab_removed),
			  panel);
	g_signal_connect (window,
			  "tabs_reordered",
			  G_CALLBACK (window_tabs_reordered),
			  panel);
	g_signal_connect (window,
			  "active_tab_changed",
			  G_CALLBACK (window_active_tab_changed),
			  panel);
}

static void
treeview_cursor_changed (CtkTreeView         *view,
			 LapizDocumentsPanel *panel)
{
	CtkTreeIter iter;
	CtkTreeSelection *selection;
	gpointer tab;

	selection = ctk_tree_view_get_selection (
				CTK_TREE_VIEW (panel->priv->treeview));

	if (ctk_tree_selection_get_selected (selection, NULL, &iter))
	{
		ctk_tree_model_get (panel->priv->model,
				    &iter,
				    TAB_COLUMN,
				    &tab,
				    -1);

		if (lapiz_window_get_active_tab (panel->priv->window) != tab)
		{
			lapiz_window_set_active_tab (panel->priv->window,
						     LAPIZ_TAB (tab));
		}
	}
}

static void
lapiz_documents_panel_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
	LapizDocumentsPanel *panel = LAPIZ_DOCUMENTS_PANEL (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			set_window (panel, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_documents_panel_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
	LapizDocumentsPanel *panel = LAPIZ_DOCUMENTS_PANEL (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			panel->priv = lapiz_documents_panel_get_instance_private (panel);
			g_value_set_object (value, panel->priv->window);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_documents_panel_finalize (GObject *object)
{
	/* LapizDocumentsPanel *tab = LAPIZ_DOCUMENTS_PANEL (object); */

	/* TODO: disconnect signal with window */

	G_OBJECT_CLASS (lapiz_documents_panel_parent_class)->finalize (object);
}

static void
lapiz_documents_panel_dispose (GObject *object)
{
	LapizDocumentsPanel *panel = LAPIZ_DOCUMENTS_PANEL (object);

	if (panel->priv->window != NULL)
	{
		g_object_unref (panel->priv->window);
		panel->priv->window = NULL;
	}

	G_OBJECT_CLASS (lapiz_documents_panel_parent_class)->dispose (object);
}

static void
lapiz_documents_panel_class_init (LapizDocumentsPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_documents_panel_finalize;
	object_class->dispose = lapiz_documents_panel_dispose;
	object_class->get_property = lapiz_documents_panel_get_property;
	object_class->set_property = lapiz_documents_panel_set_property;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "The LapizWindow this LapizDocumentsPanel is associated with",
							      LAPIZ_TYPE_WINDOW,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));
}

static gboolean
show_popup_menu (LapizDocumentsPanel *panel,
		 CdkEventButton      *event)
{
	CtkWidget *menu;

	menu = ctk_ui_manager_get_widget (lapiz_window_get_ui_manager (panel->priv->window),
					 "/NotebookPopup");
	g_return_val_if_fail (menu != NULL, FALSE);

	if (event != NULL)
	{
		ctk_menu_popup_at_pointer (CTK_MENU (menu), NULL);
	}
	else
	{
		menu_popup_at_treeview_selection (menu, panel->priv->treeview);
		ctk_menu_shell_select_first (CTK_MENU_SHELL (menu), FALSE);
	}

	return TRUE;
}

static gboolean
panel_button_press_event (CtkTreeView         *treeview,
			  CdkEventButton      *event,
			  LapizDocumentsPanel *panel)
{
	if ((CDK_BUTTON_PRESS == event->type) && (3 == event->button))
	{
		CtkTreePath* path = NULL;

		if (event->window == ctk_tree_view_get_bin_window (treeview))
		{
			/* Change the cursor position */
			if (ctk_tree_view_get_path_at_pos (treeview,
							   event->x,
							   event->y,
							   &path,
							   NULL,
							   NULL,
							   NULL))
			{

				ctk_tree_view_set_cursor (treeview,
							  path,
							  NULL,
							  FALSE);

				ctk_tree_path_free (path);

				/* A row exists at mouse position */
				return show_popup_menu (panel, event);
			}
		}
	}

	return FALSE;
}

static gboolean
panel_popup_menu (CtkWidget           *treeview,
		  LapizDocumentsPanel *panel)
{
	/* Only respond if the treeview is the actual focus */
	if (ctk_window_get_focus (CTK_WINDOW (panel->priv->window)) == treeview)
	{
		return show_popup_menu (panel, NULL);
	}

	return FALSE;
}

static gboolean
treeview_query_tooltip (CtkWidget  *widget,
			gint        x,
			gint        y,
			gboolean    keyboard_tip,
			CtkTooltip *tooltip,
			gpointer    data)
{
	CtkTreeIter iter;
	CtkTreeView *tree_view = CTK_TREE_VIEW (widget);
	CtkTreeModel *model = ctk_tree_view_get_model (tree_view);
	CtkTreePath *path = NULL;
	gpointer *tab;
	gchar *tip;

	if (keyboard_tip)
	{
		ctk_tree_view_get_cursor (tree_view, &path, NULL);

		if (path == NULL)
		{
			return FALSE;
		}
	}
	else
	{
		gint bin_x, bin_y;

		ctk_tree_view_convert_widget_to_bin_window_coords (tree_view,
								   x, y,
								   &bin_x, &bin_y);

		if (!ctk_tree_view_get_path_at_pos (tree_view,
						    bin_x, bin_y,
						    &path,
						    NULL, NULL, NULL))
		{
			return FALSE;
		}
	}

	ctk_tree_model_get_iter (model, &iter, path);
	ctk_tree_model_get (model,
			    &iter,
			    TAB_COLUMN,
			    &tab,
			    -1);

	tip = _lapiz_tab_get_tooltips (LAPIZ_TAB (tab));
	ctk_tooltip_set_markup (tooltip, tip);

	g_free (tip);
	ctk_tree_path_free (path);

	return TRUE;
}

static void
treeview_row_inserted (CtkTreeModel        *tree_model,
		       CtkTreePath         *path,
		       CtkTreeIter         *iter,
		       LapizDocumentsPanel *panel)
{
	LapizTab *tab;
	gint *indeces;
	CtkWidget *nb;
	gint old_position;
	gint new_position;

	if (panel->priv->adding_tab)
		return;

	tab = lapiz_window_get_active_tab (panel->priv->window);
	g_return_if_fail (tab != NULL);

	panel->priv->is_reodering = TRUE;

	indeces = ctk_tree_path_get_indices (path);

	/* g_debug ("New Index: %d (path: %s)", indeces[0], ctk_tree_path_to_string (path));*/

	nb = _lapiz_window_get_notebook (panel->priv->window);

	new_position = indeces[0];
	old_position = ctk_notebook_page_num (CTK_NOTEBOOK (nb),
				    	      CTK_WIDGET (tab));
	if (new_position > old_position)
		new_position = MAX (0, new_position - 1);

	lapiz_notebook_reorder_tab (LAPIZ_NOTEBOOK (nb),
				    tab,
				    new_position);

	panel->priv->is_reodering = FALSE;
}

static void
lapiz_documents_panel_init (LapizDocumentsPanel *panel)
{
	CtkWidget 		*sw;
	CtkTreeViewColumn	*column;
	CtkCellRenderer 	*cell;
	CtkTreeSelection 	*selection;

	panel->priv = lapiz_documents_panel_get_instance_private (panel);

	panel->priv->adding_tab = FALSE;
	panel->priv->is_reodering = FALSE;

	ctk_orientable_set_orientation (CTK_ORIENTABLE (panel),
	                                CTK_ORIENTATION_VERTICAL);

	/* Create the scrolled window */
	sw = ctk_scrolled_window_new (NULL, NULL);
	g_return_if_fail (sw != NULL);

	ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (sw),
					CTK_POLICY_AUTOMATIC,
					CTK_POLICY_AUTOMATIC);
	ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (sw),
                                             CTK_SHADOW_IN);
	ctk_widget_show (sw);
	ctk_box_pack_start (CTK_BOX (panel), sw, TRUE, TRUE, 0);

	/* Create the empty model */
	panel->priv->model = CTK_TREE_MODEL (ctk_list_store_new (N_COLUMNS,
								 CDK_TYPE_PIXBUF,
								 G_TYPE_STRING,
								 G_TYPE_POINTER));

	/* Create the treeview */
	panel->priv->treeview = ctk_tree_view_new_with_model (panel->priv->model);
	g_object_unref (G_OBJECT (panel->priv->model));
	ctk_container_add (CTK_CONTAINER (sw), panel->priv->treeview);
	ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (panel->priv->treeview), FALSE);
	ctk_tree_view_set_reorderable (CTK_TREE_VIEW (panel->priv->treeview), TRUE);

	g_object_set (panel->priv->treeview, "has-tooltip", TRUE, NULL);

	ctk_widget_show (panel->priv->treeview);

	column = ctk_tree_view_column_new ();
	ctk_tree_view_column_set_title (column, _("Documents"));

	cell = ctk_cell_renderer_pixbuf_new ();
	ctk_tree_view_column_pack_start (column, cell, FALSE);
	ctk_tree_view_column_add_attribute (column, cell, "pixbuf", PIXBUF_COLUMN);
	cell = ctk_cell_renderer_text_new ();
	ctk_tree_view_column_pack_start (column, cell, TRUE);
	ctk_tree_view_column_add_attribute (column, cell, "markup", NAME_COLUMN);

	ctk_tree_view_append_column (CTK_TREE_VIEW (panel->priv->treeview),
				     column);

	selection = ctk_tree_view_get_selection (
			CTK_TREE_VIEW (panel->priv->treeview));

	ctk_tree_selection_set_mode (selection, CTK_SELECTION_SINGLE);

	g_signal_connect (panel->priv->treeview,
			  "cursor_changed",
			  G_CALLBACK (treeview_cursor_changed),
			  panel);
	g_signal_connect (panel->priv->treeview,
			  "button-press-event",
			  G_CALLBACK (panel_button_press_event),
			  panel);
	g_signal_connect (panel->priv->treeview,
			  "popup-menu",
			  G_CALLBACK (panel_popup_menu),
			  panel);
	g_signal_connect (panel->priv->treeview,
			  "query-tooltip",
			  G_CALLBACK (treeview_query_tooltip),
			  NULL);

	g_signal_connect (panel->priv->model,
			  "row-inserted",
			  G_CALLBACK (treeview_row_inserted),
			  panel);
}

CtkWidget *
lapiz_documents_panel_new (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	return CTK_WIDGET (g_object_new (LAPIZ_TYPE_DOCUMENTS_PANEL,
					 "window", window,
					 NULL));
}
