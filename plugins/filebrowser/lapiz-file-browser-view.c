/*
 * lapiz-file-browser-view.c - Lapiz plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
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
 */

#include <string.h>
#include <gio/gio.h>
#include <ctk/ctk.h>
#include <cdk/cdkkeysyms.h>

#include "lapiz-file-browser-store.h"
#include "lapiz-file-bookmarks-store.h"
#include "lapiz-file-browser-view.h"
#include "lapiz-file-browser-marshal.h"
#include "lapiz-file-browser-enum-types.h"

struct _LapizFileBrowserViewPrivate
{
	CtkTreeViewColumn *column;
	CtkCellRenderer *pixbuf_renderer;
	CtkCellRenderer *text_renderer;

	CtkTreeModel *model;
	CtkTreeRowReference *editable;

	CdkCursor *busy_cursor;

	/* CLick policy */
	LapizFileBrowserViewClickPolicy click_policy;
	CtkTreePath *double_click_path[2]; /* Both clicks in a double click need to be on the same row */
	CtkTreePath *hover_path;
	CdkCursor *hand_cursor;
	gboolean ignore_release;
	gboolean selected_on_button_down;
	gint drag_button;
	gboolean drag_started;

	gboolean restore_expand_state;
	gboolean is_refresh;
	GHashTable * expand_state;
};

/* Properties */
enum
{
	PROP_0,

	PROP_CLICK_POLICY,
	PROP_RESTORE_EXPAND_STATE
};

/* Signals */
enum
{
	ERROR,
	FILE_ACTIVATED,
	DIRECTORY_ACTIVATED,
	BOOKMARK_ACTIVATED,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0 };

static const CtkTargetEntry drag_source_targets[] = {
	{ "text/uri-list", 0, 0 }
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizFileBrowserView,
                                lapiz_file_browser_view,
                                CTK_TYPE_TREE_VIEW,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizFileBrowserView))

static void on_cell_edited 		(CtkCellRendererText 	* cell,
				 	 gchar 			* path,
				 	 gchar 			* new_text,
				 	LapizFileBrowserView 	* tree_view);

static void on_begin_refresh 		(LapizFileBrowserStore 	* model,
					 LapizFileBrowserView 	* view);
static void on_end_refresh 		(LapizFileBrowserStore 	* model,
					 LapizFileBrowserView 	* view);

static void on_unload			(LapizFileBrowserStore 	* model,
					 gchar const		* uri,
					 LapizFileBrowserView 	* view);

static void on_row_inserted		(LapizFileBrowserStore 	* model,
					 CtkTreePath		* path,
					 CtkTreeIter		* iter,
					 LapizFileBrowserView 	* view);

static void
lapiz_file_browser_view_finalize (GObject * object)
{
	LapizFileBrowserView *obj = LAPIZ_FILE_BROWSER_VIEW(object);

	if (obj->priv->hand_cursor)
		g_object_unref (obj->priv->hand_cursor);

	if (obj->priv->hover_path)
		ctk_tree_path_free (obj->priv->hover_path);

	if (obj->priv->expand_state)
	{
		g_hash_table_destroy (obj->priv->expand_state);
		obj->priv->expand_state = NULL;
	}

	g_object_unref (obj->priv->busy_cursor);

	G_OBJECT_CLASS (lapiz_file_browser_view_parent_class)->
	    finalize (object);
}

static void
add_expand_state (LapizFileBrowserView * view,
		  gchar const * uri)
{
	GFile * file;

	if (!uri)
		return;

	file = g_file_new_for_uri (uri);

	if (view->priv->expand_state)
		g_hash_table_insert (view->priv->expand_state, file, file);
	else
		g_object_unref (file);
}

static void
remove_expand_state (LapizFileBrowserView * view,
		     gchar const * uri)
{
	GFile * file;

	if (!uri)
		return;

	file = g_file_new_for_uri (uri);

	if (view->priv->expand_state)
		g_hash_table_remove (view->priv->expand_state, file);

	g_object_unref (file);
}

static void
row_expanded (CtkTreeView * tree_view,
	      CtkTreeIter * iter,
	      CtkTreePath * path)
{
	LapizFileBrowserView *view = LAPIZ_FILE_BROWSER_VIEW (tree_view);
	gchar * uri;

	if (CTK_TREE_VIEW_CLASS (lapiz_file_browser_view_parent_class)->row_expanded)
		CTK_TREE_VIEW_CLASS (lapiz_file_browser_view_parent_class)->row_expanded (tree_view, iter, path);

	if (!LAPIZ_IS_FILE_BROWSER_STORE (view->priv->model))
		return;

	if (view->priv->restore_expand_state)
	{
		ctk_tree_model_get (view->priv->model,
				    iter,
				    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI,
				    &uri,
				    -1);

		add_expand_state (view, uri);
		g_free (uri);
	}

	_lapiz_file_browser_store_iter_expanded (LAPIZ_FILE_BROWSER_STORE (view->priv->model),
						 iter);
}

static void
row_collapsed (CtkTreeView * tree_view,
	       CtkTreeIter * iter,
	       CtkTreePath * path)
{
	LapizFileBrowserView *view = LAPIZ_FILE_BROWSER_VIEW (tree_view);
	gchar * uri;

	if (CTK_TREE_VIEW_CLASS (lapiz_file_browser_view_parent_class)->row_collapsed)
		CTK_TREE_VIEW_CLASS (lapiz_file_browser_view_parent_class)->row_collapsed (tree_view, iter, path);

	if (!LAPIZ_IS_FILE_BROWSER_STORE (view->priv->model))
		return;

	if (view->priv->restore_expand_state)
	{
		ctk_tree_model_get (view->priv->model,
				    iter,
				    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI,
				    &uri,
				    -1);

		remove_expand_state (view, uri);
		g_free (uri);
	}

	_lapiz_file_browser_store_iter_collapsed (LAPIZ_FILE_BROWSER_STORE (view->priv->model),
						  iter);
}

static gboolean
leave_notify_event (CtkWidget *widget,
		    CdkEventCrossing *event)
{
	LapizFileBrowserView *view = LAPIZ_FILE_BROWSER_VIEW (widget);

	if (view->priv->click_policy == LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE &&
	    view->priv->hover_path != NULL) {
		ctk_tree_path_free (view->priv->hover_path);
		view->priv->hover_path = NULL;
	}

	// Chainup
	return CTK_WIDGET_CLASS (lapiz_file_browser_view_parent_class)->leave_notify_event (widget, event);
}

static gboolean
enter_notify_event (CtkWidget *widget,
		    CdkEventCrossing *event)
{
	LapizFileBrowserView *view = LAPIZ_FILE_BROWSER_VIEW (widget);

	if (view->priv->click_policy == LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE) {
		if (view->priv->hover_path != NULL)
			ctk_tree_path_free (view->priv->hover_path);

		ctk_tree_view_get_path_at_pos (CTK_TREE_VIEW (widget),
					       event->x, event->y,
					       &view->priv->hover_path,
					       NULL, NULL, NULL);

		if (view->priv->hover_path != NULL)
			cdk_window_set_cursor (ctk_widget_get_window (widget),
					       view->priv->hand_cursor);
	}

	// Chainup
	return CTK_WIDGET_CLASS (lapiz_file_browser_view_parent_class)->enter_notify_event (widget, event);
}

static gboolean
motion_notify_event (CtkWidget * widget,
		     CdkEventMotion * event)
{
	CtkTreePath *old_hover_path;
	LapizFileBrowserView *view = LAPIZ_FILE_BROWSER_VIEW (widget);

	if (view->priv->click_policy == LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE) {
		old_hover_path = view->priv->hover_path;
		ctk_tree_view_get_path_at_pos (CTK_TREE_VIEW (widget),
					       event->x, event->y,
					       &view->priv->hover_path,
					       NULL, NULL, NULL);

		if ((old_hover_path != NULL) != (view->priv->hover_path != NULL)) {
			if (view->priv->hover_path != NULL)
				cdk_window_set_cursor (ctk_widget_get_window (widget),
						       view->priv->hand_cursor);
			else
				cdk_window_set_cursor (ctk_widget_get_window (widget),
						       NULL);
		}

		if (old_hover_path != NULL)
			ctk_tree_path_free (old_hover_path);
	}

	// Chainup
	return CTK_WIDGET_CLASS (lapiz_file_browser_view_parent_class)->motion_notify_event (widget, event);
}

static void
set_click_policy_property (LapizFileBrowserView            *obj,
			   LapizFileBrowserViewClickPolicy  click_policy)
{
	CtkTreeIter iter;
	CdkDisplay *display;
	CdkWindow *win;

	display = ctk_widget_get_display (CTK_WIDGET (obj));

	obj->priv->click_policy = click_policy;

	if (click_policy == LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE) {
		if (obj->priv->hand_cursor == NULL)
			obj->priv->hand_cursor = cdk_cursor_new_for_display (display, CDK_HAND2);
	} else if (click_policy == LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_DOUBLE) {
		if (obj->priv->hover_path != NULL) {
			if (ctk_tree_model_get_iter (CTK_TREE_MODEL (obj->priv->model),
						     &iter, obj->priv->hover_path))
				ctk_tree_model_row_changed (CTK_TREE_MODEL (obj->priv->model),
							    obj->priv->hover_path, &iter);

			ctk_tree_path_free (obj->priv->hover_path);
			obj->priv->hover_path = NULL;
		}

		if (ctk_widget_get_realized (CTK_WIDGET (obj))) {
			win = ctk_widget_get_window (CTK_WIDGET (obj));
			cdk_window_set_cursor (win, NULL);

			display = ctk_widget_get_display (CTK_WIDGET (obj));

			if (display != NULL)
				cdk_display_flush (display);
		}

		if (obj->priv->hand_cursor) {
			g_object_unref (obj->priv->hand_cursor);
			obj->priv->hand_cursor = NULL;
		}
	}
}

static void
directory_activated (LapizFileBrowserView *view,
		     CtkTreeIter          *iter)
{
	lapiz_file_browser_store_set_virtual_root (LAPIZ_FILE_BROWSER_STORE (view->priv->model), iter);
}

static void
activate_selected_files (LapizFileBrowserView *view) {
	CtkTreeView *tree_view = CTK_TREE_VIEW (view);
	CtkTreeSelection *selection = ctk_tree_view_get_selection (tree_view);
	GList *rows, *row;
	CtkTreePath *directory = NULL;
	CtkTreePath *path;
	CtkTreeIter iter;
	LapizFileBrowserStoreFlag flags;

	rows = ctk_tree_selection_get_selected_rows (selection, &view->priv->model);

	for (row = rows; row; row = row->next) {
		path = (CtkTreePath *)(row->data);

		/* Get iter from path */
		if (!ctk_tree_model_get_iter (view->priv->model, &iter, path))
			continue;

		ctk_tree_model_get (view->priv->model, &iter, LAPIZ_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags, -1);

		if (FILE_IS_DIR (flags)) {
			if (directory == NULL)
				directory = path;

		} else if (!FILE_IS_DUMMY (flags)) {
			g_signal_emit (view, signals[FILE_ACTIVATED], 0, &iter);
		}
	}

	if (directory != NULL) {
		if (ctk_tree_model_get_iter (view->priv->model, &iter, directory))
			g_signal_emit (view, signals[DIRECTORY_ACTIVATED], 0, &iter);
	}

	g_list_foreach (rows, (GFunc)ctk_tree_path_free, NULL);
	g_list_free (rows);
}

static void
activate_selected_bookmark (LapizFileBrowserView *view) {
	CtkTreeView *tree_view = CTK_TREE_VIEW (view);
	CtkTreeSelection *selection = ctk_tree_view_get_selection (tree_view);
	CtkTreeIter iter;

	if (ctk_tree_selection_get_selected (selection, &view->priv->model, &iter))
		g_signal_emit (view, signals[BOOKMARK_ACTIVATED], 0, &iter);
}

static void
activate_selected_items (LapizFileBrowserView *view)
{
	if (LAPIZ_IS_FILE_BROWSER_STORE (view->priv->model))
		activate_selected_files (view);
	else if (LAPIZ_IS_FILE_BOOKMARKS_STORE (view->priv->model))
		activate_selected_bookmark (view);
}

static void
toggle_hidden_filter (LapizFileBrowserView *view)
{
	LapizFileBrowserStoreFilterMode mode;

	if (LAPIZ_IS_FILE_BROWSER_STORE (view->priv->model))
	{
		mode = lapiz_file_browser_store_get_filter_mode
			(LAPIZ_FILE_BROWSER_STORE (view->priv->model));
		mode ^=	LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN;
		lapiz_file_browser_store_set_filter_mode
			(LAPIZ_FILE_BROWSER_STORE (view->priv->model), mode);
	}
}

static gboolean
button_event_modifies_selection (CdkEventButton *event)
{
	return (event->state & (CDK_CONTROL_MASK | CDK_SHIFT_MASK)) != 0;
}

static void
drag_begin (CtkWidget      *widget,
	    CdkDragContext *context)
{
	LapizFileBrowserView *view = LAPIZ_FILE_BROWSER_VIEW (widget);

	view->priv->drag_button = 0;
	view->priv->drag_started = TRUE;

	/* Chain up */
	CTK_WIDGET_CLASS (lapiz_file_browser_view_parent_class)->drag_begin (widget, context);
}

static void
did_not_drag (LapizFileBrowserView *view,
	      CdkEventButton       *event)
{
	CtkTreeView *tree_view;
	CtkTreeSelection *selection;
	CtkTreePath *path;

	tree_view = CTK_TREE_VIEW (view);
	selection = ctk_tree_view_get_selection (tree_view);

	if (ctk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					   &path, NULL, NULL, NULL)) {
		if ((view->priv->click_policy == LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE)
		    && !button_event_modifies_selection(event)
		    && (event->button == 1 || event->button == 2)) {
		    	/* Activate all selected items, and leave them selected */
			activate_selected_items (view);
		} else if ((event->button == 1 || event->button == 2)
		    && ((event->state & CDK_CONTROL_MASK) != 0 ||
			(event->state & CDK_SHIFT_MASK) == 0)
		    && view->priv->selected_on_button_down) {
			if (!button_event_modifies_selection (event)) {
				ctk_tree_selection_unselect_all (selection);
				ctk_tree_selection_select_path (selection, path);
			} else {
				ctk_tree_selection_unselect_path (selection, path);
			}
		}

		ctk_tree_path_free (path);
	}
}

static gboolean
button_release_event (CtkWidget       *widget,
		      CdkEventButton *event)
{
	LapizFileBrowserView *view = LAPIZ_FILE_BROWSER_VIEW (widget);

	if (event->button == view->priv->drag_button) {
		view->priv->drag_button = 0;

		if (!view->priv->drag_started &&
		    !view->priv->ignore_release)
			did_not_drag (view, event);
	}

	/* Chain up */
	return CTK_WIDGET_CLASS (lapiz_file_browser_view_parent_class)->button_release_event (widget, event);
}

static gboolean
button_press_event (CtkWidget      *widget,
		    CdkEventButton *event)
{
	int double_click_time;
	static int click_count = 0;
	static guint32 last_click_time = 0;
	LapizFileBrowserView *view;
	CtkTreeView *tree_view;
	CtkTreeSelection *selection;
	CtkTreePath *path;
	int expander_size;
	int horizontal_separator;
	gboolean on_expander;
	gboolean call_parent;
	gboolean selected;
	CtkWidgetClass *widget_parent = CTK_WIDGET_CLASS(lapiz_file_browser_view_parent_class);

	tree_view = CTK_TREE_VIEW (widget);
	view = LAPIZ_FILE_BROWSER_VIEW (widget);
	selection = ctk_tree_view_get_selection (tree_view);

	/* Get double click time */
	g_object_get (G_OBJECT (ctk_widget_get_settings (widget)),
		      "ctk-double-click-time", &double_click_time,
		      NULL);

	/* Determine click count */
	if (event->time - last_click_time < double_click_time)
		click_count++;
	else
		click_count = 0;

	last_click_time = event->time;

	/* Ignore double click if we are in single click mode */
	if (view->priv->click_policy == LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE &&
	    click_count >= 2) {
		return TRUE;
	}

	view->priv->ignore_release = FALSE;
	call_parent = TRUE;

	if (ctk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					   &path, NULL, NULL, NULL)) {
		/* Keep track of path of last click so double clicks only happen
		 * on the same item */
		if ((event->button == 1 || event->button == 2)  &&
		    event->type == CDK_BUTTON_PRESS) {
			if (view->priv->double_click_path[1])
				ctk_tree_path_free (view->priv->double_click_path[1]);

			view->priv->double_click_path[1] = view->priv->double_click_path[0];
			view->priv->double_click_path[0] = ctk_tree_path_copy (path);
		}

		if (event->type == CDK_2BUTTON_PRESS) {
			if (view->priv->double_click_path[1] &&
			    ctk_tree_path_compare (view->priv->double_click_path[0], view->priv->double_click_path[1]) == 0)
				activate_selected_items (view);

			/* Chain up */
			widget_parent->button_press_event (widget, event);
		} else {
			/* We're going to filter out some situations where
			 * we can't let the default code run because all
			 * but one row would be would be deselected. We don't
			 * want that; we want the right click menu or single
			 * click to apply to everything that's currently selected. */
			selected = ctk_tree_selection_path_is_selected (selection, path);

			if (event->button == 3 && selected)
				call_parent = FALSE;

			if ((event->button == 1 || event->button == 2) &&
			    ((event->state & CDK_CONTROL_MASK) != 0 ||
			     (event->state & CDK_SHIFT_MASK) == 0)) {
				ctk_widget_style_get (widget,
						      "expander-size", &expander_size,
						      "horizontal-separator", &horizontal_separator,
						      NULL);
				on_expander = (event->x <= horizontal_separator / 2 +
					       ctk_tree_path_get_depth (path) * expander_size);

				view->priv->selected_on_button_down = selected;

				if (selected) {
					call_parent = on_expander || ctk_tree_selection_count_selected_rows (selection) == 1;
					view->priv->ignore_release = call_parent && view->priv->click_policy != LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE;
				} else if  ((event->state & CDK_CONTROL_MASK) != 0) {
					call_parent = FALSE;
					ctk_tree_selection_select_path (selection, path);
				} else {
					view->priv->ignore_release = on_expander;
				}
			}

			if (call_parent) {
				/* Chain up */
				widget_parent->button_press_event (widget, event);
			} else if (selected) {
				ctk_widget_grab_focus (widget);
			}

			if ((event->button == 1 || event->button == 2) &&
			    event->type == CDK_BUTTON_PRESS) {
				view->priv->drag_started = FALSE;
				view->priv->drag_button = event->button;
			}
		}

		ctk_tree_path_free (path);
	} else {
		if ((event->button == 1 || event->button == 2)  &&
		    event->type == CDK_BUTTON_PRESS) {
			if (view->priv->double_click_path[1])
				ctk_tree_path_free (view->priv->double_click_path[1]);

			view->priv->double_click_path[1] = view->priv->double_click_path[0];
			view->priv->double_click_path[0] = NULL;
		}

		ctk_tree_selection_unselect_all (selection);
		/* Chain up */
		widget_parent->button_press_event (widget, event);
	}

	/* We already chained up if nescessary, so just return TRUE */
	return TRUE;
}

static gboolean
key_press_event (CtkWidget   *widget,
		 CdkEventKey *event)
{
	LapizFileBrowserView *view;
	guint modifiers;
	gboolean handled;

	view = LAPIZ_FILE_BROWSER_VIEW (widget);
	handled = FALSE;

	modifiers = ctk_accelerator_get_default_mod_mask ();

	switch (event->keyval) {
	case CDK_KEY_space:
		if (event->state & CDK_CONTROL_MASK) {
			handled = FALSE;
			break;
		}
		if (!ctk_widget_has_focus (widget)) {
			handled = FALSE;
			break;
		}

		activate_selected_items (view);
		handled = TRUE;
		break;

	case CDK_KEY_Return:
	case CDK_KEY_KP_Enter:
		activate_selected_items (view);
		handled = TRUE;
		break;

	case CDK_KEY_h:
		if ((event->state & modifiers) == CDK_CONTROL_MASK) {
			toggle_hidden_filter (view);
			handled = TRUE;
			break;
		}

	default:
		handled = FALSE;
	}

	/* Chain up */
	if (!handled)
		return CTK_WIDGET_CLASS (lapiz_file_browser_view_parent_class)->key_press_event (widget, event);

	return TRUE;
}

static void
fill_expand_state (LapizFileBrowserView * view, CtkTreeIter * iter)
{
	CtkTreePath * path;
	CtkTreeIter child;
	gchar * uri;

	if (!ctk_tree_model_iter_has_child (view->priv->model, iter))
		return;

	path = ctk_tree_model_get_path (view->priv->model, iter);

	if (ctk_tree_view_row_expanded (CTK_TREE_VIEW (view), path))
	{
		ctk_tree_model_get (view->priv->model,
				    iter,
				    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI,
				    &uri,
				    -1);

		add_expand_state (view, uri);
		g_free (uri);
	}

	if (ctk_tree_model_iter_children (view->priv->model, &child, iter))
	{
		do {
			fill_expand_state (view, &child);
		} while (ctk_tree_model_iter_next (view->priv->model, &child));
	}

	ctk_tree_path_free (path);
}

static void
uninstall_restore_signals (LapizFileBrowserView * tree_view,
			   CtkTreeModel * model)
{
	g_signal_handlers_disconnect_by_func (model,
					      on_begin_refresh,
					      tree_view);

	g_signal_handlers_disconnect_by_func (model,
					      on_end_refresh,
					      tree_view);

	g_signal_handlers_disconnect_by_func (model,
					      on_unload,
					      tree_view);

	g_signal_handlers_disconnect_by_func (model,
					      on_row_inserted,
					      tree_view);
}

static void
install_restore_signals (LapizFileBrowserView * tree_view,
			 CtkTreeModel * model)
{
	g_signal_connect (model,
			  "begin-refresh",
			  G_CALLBACK (on_begin_refresh),
			  tree_view);

	g_signal_connect (model,
			  "end-refresh",
			  G_CALLBACK (on_end_refresh),
			  tree_view);

	g_signal_connect (model,
			  "unload",
			  G_CALLBACK (on_unload),
			  tree_view);

	g_signal_connect_after (model,
			  "row-inserted",
			  G_CALLBACK (on_row_inserted),
			  tree_view);
}

static void
set_restore_expand_state (LapizFileBrowserView * view,
			  gboolean state)
{
	if (state == view->priv->restore_expand_state)
		return;

	if (view->priv->expand_state)
	{
		g_hash_table_destroy (view->priv->expand_state);
		view->priv->expand_state = NULL;
	}

	if (state)
	{
		view->priv->expand_state = g_hash_table_new_full (g_file_hash,
								  (GEqualFunc)g_file_equal,
								  g_object_unref,
								  NULL);

		if (view->priv->model && LAPIZ_IS_FILE_BROWSER_STORE (view->priv->model))
		{
			fill_expand_state (view, NULL);

			install_restore_signals (view, view->priv->model);
		}
	}
	else if (view->priv->model && LAPIZ_IS_FILE_BROWSER_STORE (view->priv->model))
	{
		uninstall_restore_signals (view, view->priv->model);
	}

	view->priv->restore_expand_state = state;
}

static void
get_property (GObject    *object,
	      guint       prop_id,
	      GValue     *value,
	      GParamSpec *pspec)
{
	LapizFileBrowserView *obj = LAPIZ_FILE_BROWSER_VIEW (object);

	switch (prop_id)
	{
		case PROP_CLICK_POLICY:
			g_value_set_enum (value, obj->priv->click_policy);
			break;
		case PROP_RESTORE_EXPAND_STATE:
			g_value_set_boolean (value, obj->priv->restore_expand_state);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
set_property (GObject      *object,
	      guint         prop_id,
	      const GValue *value,
	      GParamSpec   *pspec)
{
	LapizFileBrowserView *obj = LAPIZ_FILE_BROWSER_VIEW (object);

	switch (prop_id)
	{
		case PROP_CLICK_POLICY:
			set_click_policy_property (obj, g_value_get_enum (value));
			break;
		case PROP_RESTORE_EXPAND_STATE:
			set_restore_expand_state (obj, g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_file_browser_view_class_init (LapizFileBrowserViewClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CtkTreeViewClass *tree_view_class = CTK_TREE_VIEW_CLASS (klass);
	CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);

	object_class->finalize = lapiz_file_browser_view_finalize;
	object_class->get_property = get_property;
	object_class->set_property = set_property;

	/* Event handlers */
	widget_class->motion_notify_event = motion_notify_event;
	widget_class->enter_notify_event = enter_notify_event;
	widget_class->leave_notify_event = leave_notify_event;
	widget_class->button_press_event = button_press_event;
	widget_class->button_release_event = button_release_event;
	widget_class->drag_begin = drag_begin;
	widget_class->key_press_event = key_press_event;

	/* Tree view handlers */
	tree_view_class->row_expanded = row_expanded;
	tree_view_class->row_collapsed = row_collapsed;

	/* Default handlers */
	klass->directory_activated = directory_activated;

	g_object_class_install_property (object_class, PROP_CLICK_POLICY,
					 g_param_spec_enum ("click-policy",
					 		    "Click Policy",
					 		    "The click policy",
					 		     LAPIZ_TYPE_FILE_BROWSER_VIEW_CLICK_POLICY,
					 		     LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_DOUBLE,
					 		     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class, PROP_RESTORE_EXPAND_STATE,
					 g_param_spec_boolean ("restore-expand-state",
					 		       "Restore Expand State",
					 		       "Restore expanded state of loaded directories",
					 		       FALSE,
					 		       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	signals[ERROR] =
	    g_signal_new ("error",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizFileBrowserViewClass,
					   error), NULL, NULL,
			  lapiz_file_browser_marshal_VOID__UINT_STRING,
			  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING);
	signals[FILE_ACTIVATED] =
	    g_signal_new ("file-activated",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizFileBrowserViewClass,
					   file_activated), NULL, NULL,
			  g_cclosure_marshal_VOID__BOXED,
			  G_TYPE_NONE, 1, CTK_TYPE_TREE_ITER);
	signals[DIRECTORY_ACTIVATED] =
	    g_signal_new ("directory-activated",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizFileBrowserViewClass,
					   directory_activated), NULL, NULL,
			  g_cclosure_marshal_VOID__BOXED,
			  G_TYPE_NONE, 1, CTK_TYPE_TREE_ITER);
	signals[BOOKMARK_ACTIVATED] =
	    g_signal_new ("bookmark-activated",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizFileBrowserViewClass,
					   bookmark_activated), NULL, NULL,
			  g_cclosure_marshal_VOID__BOXED,
			  G_TYPE_NONE, 1, CTK_TYPE_TREE_ITER);
}

static void
lapiz_file_browser_view_class_finalize (LapizFileBrowserViewClass *klass)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
cell_data_cb (CtkTreeViewColumn * tree_column, CtkCellRenderer * cell,
	      CtkTreeModel * tree_model, CtkTreeIter * iter,
	      LapizFileBrowserView * obj)
{
	CtkTreePath *path;
	PangoUnderline underline = PANGO_UNDERLINE_NONE;
	gboolean editable = FALSE;

	path = ctk_tree_model_get_path (tree_model, iter);

	if (obj->priv->click_policy == LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE) {
		if (obj->priv->hover_path != NULL &&
		    ctk_tree_path_compare (path, obj->priv->hover_path) == 0)
			underline = PANGO_UNDERLINE_SINGLE;
	}

	if (LAPIZ_IS_FILE_BROWSER_STORE (tree_model))
	{
		if (obj->priv->editable != NULL &&
		    ctk_tree_row_reference_valid (obj->priv->editable))
		{
			CtkTreePath *edpath = ctk_tree_row_reference_get_path (obj->priv->editable);

			editable = edpath && ctk_tree_path_compare (path, edpath) == 0;
		}
	}

	ctk_tree_path_free (path);
	g_object_set (cell, "editable", editable, "underline", underline, NULL);
}

static void
lapiz_file_browser_view_init (LapizFileBrowserView * obj)
{
	CdkDisplay *display;

	obj->priv = lapiz_file_browser_view_get_instance_private (obj);

	obj->priv->column = ctk_tree_view_column_new ();

	obj->priv->pixbuf_renderer = ctk_cell_renderer_pixbuf_new ();
	ctk_tree_view_column_pack_start (obj->priv->column,
					 obj->priv->pixbuf_renderer,
					 FALSE);
	ctk_tree_view_column_add_attribute (obj->priv->column,
					    obj->priv->pixbuf_renderer,
					    "pixbuf",
					    LAPIZ_FILE_BROWSER_STORE_COLUMN_ICON);

	obj->priv->text_renderer = ctk_cell_renderer_text_new ();
	ctk_tree_view_column_pack_start (obj->priv->column,
					 obj->priv->text_renderer, TRUE);
	ctk_tree_view_column_add_attribute (obj->priv->column,
					    obj->priv->text_renderer,
					    "text",
					    LAPIZ_FILE_BROWSER_STORE_COLUMN_NAME);

	g_signal_connect (obj->priv->text_renderer, "edited",
			  G_CALLBACK (on_cell_edited), obj);

	ctk_tree_view_append_column (CTK_TREE_VIEW (obj),
				     obj->priv->column);
	ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (obj), FALSE);

	ctk_tree_view_enable_model_drag_source (CTK_TREE_VIEW (obj),
						CDK_BUTTON1_MASK,
						drag_source_targets,
						G_N_ELEMENTS (drag_source_targets),
						CDK_ACTION_COPY);

	display = ctk_widget_get_display (CTK_WIDGET (obj));
	obj->priv->busy_cursor = cdk_cursor_new_for_display (display, CDK_WATCH);
}

static gboolean
bookmarks_separator_func (CtkTreeModel * model, CtkTreeIter * iter,
			  gpointer user_data)
{
	guint flags;

	ctk_tree_model_get (model, iter,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS,
			    &flags, -1);

	return (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_SEPARATOR);
}

/* Public */
CtkWidget *
lapiz_file_browser_view_new (void)
{
	LapizFileBrowserView *obj =
	    LAPIZ_FILE_BROWSER_VIEW (g_object_new
				     (LAPIZ_TYPE_FILE_BROWSER_VIEW, NULL));

	return CTK_WIDGET (obj);
}

void
lapiz_file_browser_view_set_model (LapizFileBrowserView * tree_view,
				   CtkTreeModel * model)
{
	CtkTreeSelection *selection;

	if (tree_view->priv->model == model)
		return;

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (tree_view));

	if (LAPIZ_IS_FILE_BOOKMARKS_STORE (model)) {
		ctk_tree_selection_set_mode (selection, CTK_SELECTION_BROWSE);
		ctk_tree_view_set_row_separator_func (CTK_TREE_VIEW
						      (tree_view),
						      bookmarks_separator_func,
						      NULL, NULL);
		ctk_tree_view_column_set_cell_data_func (tree_view->priv->
							 column,
							 tree_view->priv->
							 text_renderer,
							 (CtkTreeCellDataFunc)
							 cell_data_cb,
							 tree_view, NULL);
	} else {
		ctk_tree_selection_set_mode (selection, CTK_SELECTION_MULTIPLE);
		ctk_tree_view_set_row_separator_func (CTK_TREE_VIEW
						      (tree_view), NULL,
						      NULL, NULL);
		ctk_tree_view_column_set_cell_data_func (tree_view->priv->
							 column,
							 tree_view->priv->
							 text_renderer,
							 (CtkTreeCellDataFunc)
							 cell_data_cb,
							 tree_view, NULL);

		if (tree_view->priv->restore_expand_state)
			install_restore_signals (tree_view, model);

	}

	if (tree_view->priv->hover_path != NULL) {
		ctk_tree_path_free (tree_view->priv->hover_path);
		tree_view->priv->hover_path = NULL;
	}

	if (LAPIZ_IS_FILE_BROWSER_STORE (tree_view->priv->model)) {
		if (tree_view->priv->restore_expand_state)
			uninstall_restore_signals (tree_view,
						   tree_view->priv->model);
	}

	tree_view->priv->model = model;
	ctk_tree_view_set_model (CTK_TREE_VIEW (tree_view), model);
}

void
lapiz_file_browser_view_start_rename (LapizFileBrowserView * tree_view,
				      CtkTreeIter * iter)
{
	guint flags;
	CtkTreeRowReference *rowref;
	CtkTreePath *path;

	g_return_if_fail (LAPIZ_IS_FILE_BROWSER_VIEW (tree_view));
	g_return_if_fail (LAPIZ_IS_FILE_BROWSER_STORE
			  (tree_view->priv->model));
	g_return_if_fail (iter != NULL);

	ctk_tree_model_get (tree_view->priv->model, iter,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
			    -1);

	if (!(FILE_IS_DIR (flags) || !FILE_IS_DUMMY (flags)))
		return;

	path = ctk_tree_model_get_path (tree_view->priv->model, iter);
	rowref = ctk_tree_row_reference_new (tree_view->priv->model, path);

	/* Start editing */
	ctk_widget_grab_focus (CTK_WIDGET (tree_view));

	if (ctk_tree_path_up (path))
		ctk_tree_view_expand_to_path (CTK_TREE_VIEW (tree_view),
					      path);

	ctk_tree_path_free (path);
	tree_view->priv->editable = rowref;

	ctk_tree_view_set_cursor (CTK_TREE_VIEW (tree_view),
				  ctk_tree_row_reference_get_path (tree_view->priv->editable),
				  tree_view->priv->column, TRUE);

	ctk_tree_view_scroll_to_cell (CTK_TREE_VIEW (tree_view),
				      ctk_tree_row_reference_get_path (tree_view->priv->editable),
				      tree_view->priv->column,
				      FALSE, 0.0, 0.0);
}

void
lapiz_file_browser_view_set_click_policy (LapizFileBrowserView *tree_view,
					  LapizFileBrowserViewClickPolicy policy)
{
	g_return_if_fail (LAPIZ_IS_FILE_BROWSER_VIEW (tree_view));

	set_click_policy_property (tree_view, policy);

	g_object_notify (G_OBJECT (tree_view), "click-policy");
}

void
lapiz_file_browser_view_set_restore_expand_state (LapizFileBrowserView * tree_view,
						  gboolean restore_expand_state)
{
	g_return_if_fail (LAPIZ_IS_FILE_BROWSER_VIEW (tree_view));

	set_restore_expand_state (tree_view, restore_expand_state);
	g_object_notify (G_OBJECT (tree_view), "restore-expand-state");
}

/* Signal handlers */
static void
on_cell_edited (CtkCellRendererText * cell, gchar * path, gchar * new_text,
		LapizFileBrowserView * tree_view)
{
	CtkTreePath * treepath;
	CtkTreeIter iter;
	gboolean ret;
	GError * error = NULL;

	ctk_tree_row_reference_free (tree_view->priv->editable);
	tree_view->priv->editable = NULL;

	if (new_text == NULL || *new_text == '\0')
		return;

	treepath = ctk_tree_path_new_from_string (path);
	ret = ctk_tree_model_get_iter (CTK_TREE_MODEL (tree_view->priv->model), &iter, treepath);
	ctk_tree_path_free (treepath);

	if (ret) {
		if (lapiz_file_browser_store_rename (LAPIZ_FILE_BROWSER_STORE (tree_view->priv->model),
		    &iter, new_text, &error)) {
			treepath = ctk_tree_model_get_path (CTK_TREE_MODEL (tree_view->priv->model), &iter);
			ctk_tree_view_scroll_to_cell (CTK_TREE_VIEW (tree_view),
						      treepath, NULL,
						      FALSE, 0.0, 0.0);
			ctk_tree_path_free (treepath);
		}
		else {
			if (error) {
				g_signal_emit (tree_view, signals[ERROR], 0,
					       error->code, error->message);
				g_error_free (error);
			}
		}
	}
}

static void
on_begin_refresh (LapizFileBrowserStore * model,
		  LapizFileBrowserView * view)
{
	/* Store the refresh state, so we can handle unloading of nodes while
	   refreshing properly */
	view->priv->is_refresh = TRUE;
}

static void
on_end_refresh (LapizFileBrowserStore * model,
		LapizFileBrowserView * view)
{
	/* Store the refresh state, so we can handle unloading of nodes while
	   refreshing properly */
	view->priv->is_refresh = FALSE;
}

static void
on_unload (LapizFileBrowserStore * model,
	   gchar const * uri,
	   LapizFileBrowserView * view)
{
	/* Don't remove the expand state if we are refreshing */
	if (!view->priv->restore_expand_state || view->priv->is_refresh)
		return;

	remove_expand_state (view, uri);
}

static void
restore_expand_state (LapizFileBrowserView * view,
		      LapizFileBrowserStore * model,
		      CtkTreeIter * iter)
{
	gchar * uri;
	GFile * file;
	CtkTreePath * path;

	ctk_tree_model_get (CTK_TREE_MODEL (model),
			    iter,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI,
			    &uri,
			    -1);

	if (!uri)
		return;

	file = g_file_new_for_uri (uri);
	path = ctk_tree_model_get_path (CTK_TREE_MODEL (model), iter);

	if (g_hash_table_lookup (view->priv->expand_state, file))
	{
		ctk_tree_view_expand_row (CTK_TREE_VIEW (view),
					  path,
					  FALSE);
	}

	ctk_tree_path_free (path);

	g_object_unref (file);
	g_free (uri);
}

static void
on_row_inserted (LapizFileBrowserStore * model,
		 CtkTreePath * path,
		 CtkTreeIter * iter,
		 LapizFileBrowserView * view)
{
	CtkTreeIter parent;
	CtkTreePath * copy;

	if (ctk_tree_model_iter_has_child (CTK_TREE_MODEL (model), iter))
		restore_expand_state (view, model, iter);

	copy = ctk_tree_path_copy (path);

	if (ctk_tree_path_up (copy) &&
	    (ctk_tree_path_get_depth (copy) != 0) &&
	    ctk_tree_model_get_iter (CTK_TREE_MODEL (model), &parent, copy))
	{
		restore_expand_state (view, model, &parent);
	}

	ctk_tree_path_free (copy);
}

void
_lapiz_file_browser_view_register_type (GTypeModule *type_module)
{
	lapiz_file_browser_view_register_type (type_module);
}

// ex:ts=8:noet:
