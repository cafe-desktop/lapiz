/*
 * lapiz-notebook.c
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
 */

/* This file is a modified version of the epiphany file ephy-notebook.c
 * Here the relevant copyright:
 *
 *  Copyright (C) 2002 Christophe Fergeau
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <glib/gi18n.h>
#include <ctk/ctk.h>

#include "lapiz-notebook.h"
#include "lapiz-tab.h"
#include "lapiz-tab-label.h"
#include "lapiz-marshal.h"
#include "lapiz-window.h"
#include "lapiz-prefs-manager.h"
#include "lapiz-prefs-manager-private.h"

#define AFTER_ALL_TABS -1
#define NOT_IN_APP_WINDOWS -2

struct _LapizNotebookPrivate
{
	GList         *focused_pages;
	gulong         motion_notify_handler_id;
	gint           x_start;
	gint           y_start;
	gint           drag_in_progress : 1;
	gint           close_buttons_sensitive : 1;
	gint           tab_drag_and_drop_enabled : 1;
	guint          destroy_has_run : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizNotebook, lapiz_notebook, CTK_TYPE_NOTEBOOK)

static void lapiz_notebook_finalize (GObject *object);

static gboolean lapiz_notebook_change_current_page (CtkNotebook *notebook,
						    gint         offset);

static void move_current_tab_to_another_notebook  (LapizNotebook  *src,
						   LapizNotebook  *dest,
						   GdkEventMotion *event,
						   gint            dest_position);

/* Local variables */
static GdkCursor *cursor = NULL;
static gboolean leftdown = FALSE;
static gboolean drag_ready = FALSE;
static gboolean newfile_ready = TRUE;

/* Signals */
enum
{
	TAB_ADDED,
	TAB_REMOVED,
	TABS_REORDERED,
	TAB_DETACHED,
	TAB_CLOSE_REQUEST,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
lapiz_notebook_dispose (GObject *object)
{
	LapizNotebook *notebook = LAPIZ_NOTEBOOK (object);

	if (!notebook->priv->destroy_has_run)
	{
		GList *children, *l;

		children = ctk_container_get_children (CTK_CONTAINER (notebook));

		for (l = children; l != NULL; l = g_list_next (l))
		{
			lapiz_notebook_remove_tab (notebook,
						   LAPIZ_TAB (l->data));
		}

		g_list_free (children);
		notebook->priv->destroy_has_run = TRUE;
	}

	G_OBJECT_CLASS (lapiz_notebook_parent_class)->dispose (object);
}

static void
lapiz_notebook_class_init (LapizNotebookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CtkNotebookClass *notebook_class = CTK_NOTEBOOK_CLASS (klass);

	object_class->finalize = lapiz_notebook_finalize;
	object_class->dispose = lapiz_notebook_dispose;

	notebook_class->change_current_page = lapiz_notebook_change_current_page;

	signals[TAB_ADDED] =
		g_signal_new ("tab_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizNotebookClass, tab_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      LAPIZ_TYPE_TAB);
	signals[TAB_REMOVED] =
		g_signal_new ("tab_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizNotebookClass, tab_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      LAPIZ_TYPE_TAB);
	signals[TAB_DETACHED] =
		g_signal_new ("tab_detached",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizNotebookClass, tab_detached),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      LAPIZ_TYPE_TAB);
	signals[TABS_REORDERED] =
		g_signal_new ("tabs_reordered",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizNotebookClass, tabs_reordered),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[TAB_CLOSE_REQUEST] =
		g_signal_new ("tab-close-request",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizNotebookClass, tab_close_request),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      LAPIZ_TYPE_TAB);
}

static LapizNotebook *
find_notebook_at_pointer (gint abs_x, gint abs_y)
{
	GdkWindow *win_at_pointer;
	GdkWindow *toplevel_win;
	gpointer toplevel = NULL;
	GdkSeat *seat;
	GdkDevice *device;
	gint x, y;

	/* FIXME multi-head */
	seat = cdk_display_get_default_seat (cdk_display_get_default ());
	device = cdk_seat_get_pointer (seat);
	win_at_pointer = cdk_device_get_window_at_position (device, &x, &y);

	if (win_at_pointer == NULL)
	{
		/* We are outside all windows of the same application */
		return NULL;
	}

	toplevel_win = cdk_window_get_toplevel (win_at_pointer);

	/* get the CtkWidget which owns the toplevel GdkWindow */
	cdk_window_get_user_data (toplevel_win, &toplevel);

	/* toplevel should be an LapizWindow */
	if ((toplevel != NULL) &&
	    LAPIZ_IS_WINDOW (toplevel))
	{
		return LAPIZ_NOTEBOOK (_lapiz_window_get_notebook
						(LAPIZ_WINDOW (toplevel)));
	}

	/* We are outside all windows containing a notebook */
	return NULL;
}

static gboolean
is_in_notebook_window (LapizNotebook *notebook,
		       gint           abs_x,
		       gint           abs_y)
{
	LapizNotebook *nb_at_pointer;

	g_return_val_if_fail (notebook != NULL, FALSE);

	nb_at_pointer = find_notebook_at_pointer (abs_x, abs_y);

	return (nb_at_pointer == notebook);
}

static gint
find_tab_num_at_pos (LapizNotebook *notebook,
		     gint           abs_x,
		     gint           abs_y)
{
	CtkPositionType tab_pos;
	int page_num = 0;
	CtkNotebook *nb = CTK_NOTEBOOK (notebook);
	CtkWidget *page;

	tab_pos = ctk_notebook_get_tab_pos (CTK_NOTEBOOK (notebook));

	/* For some reason unfullscreen + quick click can
	   cause a wrong click event to be reported to the tab */
	if (!is_in_notebook_window (notebook, abs_x, abs_y))
	{
		return NOT_IN_APP_WINDOWS;
	}

	while ((page = ctk_notebook_get_nth_page (nb, page_num)) != NULL)
	{
		CtkAllocation allocation;
		CtkWidget *tab;
		gint max_x, max_y;
		gint x_root, y_root;

		tab = ctk_notebook_get_tab_label (nb, page);
		g_return_val_if_fail (tab != NULL, AFTER_ALL_TABS);

		if (!ctk_widget_get_mapped (tab))
		{
			++page_num;
			continue;
		}

		cdk_window_get_origin (CDK_WINDOW (ctk_widget_get_window (tab)),
				       &x_root, &y_root);

		ctk_widget_get_allocation(tab, &allocation);

		max_x = x_root + allocation.x + allocation.width;
		max_y = y_root + allocation.y + allocation.height;

		if (((tab_pos == CTK_POS_TOP) ||
		     (tab_pos == CTK_POS_BOTTOM)) &&
		    (abs_x <= max_x))
		{
			return page_num;
		}
		else if (((tab_pos == CTK_POS_LEFT) ||
		          (tab_pos == CTK_POS_RIGHT)) &&
		         (abs_y <= max_y))
		{
			return page_num;
		}

		++page_num;
	}

	return AFTER_ALL_TABS;
}

static gint
find_notebook_and_tab_at_pos (gint            abs_x,
			      gint            abs_y,
			      LapizNotebook **notebook,
			      gint           *page_num)
{
	*notebook = find_notebook_at_pointer (abs_x, abs_y);
	if (*notebook == NULL)
	{
		return NOT_IN_APP_WINDOWS;
	}

	*page_num = find_tab_num_at_pos (*notebook, abs_x, abs_y);

	if (*page_num < 0)
	{
		return *page_num;
	}
	else
	{
		return 0;
	}
}

/**
 * lapiz_notebook_move_tab:
 * @src: a #LapizNotebook
 * @dest: a #LapizNotebook
 * @tab: a #LapizTab
 * @dest_position: the position for @tab
 *
 * Moves @tab from @src to @dest.
 * If dest_position is greater than or equal to the number of tabs
 * of the destination nootebook or negative, tab will be moved to the
 * end of the tabs.
 */
void
lapiz_notebook_move_tab (LapizNotebook *src,
			 LapizNotebook *dest,
			 LapizTab      *tab,
			 gint           dest_position)
{
	g_return_if_fail (LAPIZ_IS_NOTEBOOK (src));
	g_return_if_fail (LAPIZ_IS_NOTEBOOK (dest));
	g_return_if_fail (src != dest);
	g_return_if_fail (LAPIZ_IS_TAB (tab));

	/* make sure the tab isn't destroyed while we move it */
	g_object_ref (tab);
	lapiz_notebook_remove_tab (src, tab);
	lapiz_notebook_add_tab (dest, tab, dest_position, TRUE);
	g_object_unref (tab);
}

/**
 * lapiz_notebook_reorder_tab:
 * @src: a #LapizNotebook
 * @tab: a #LapizTab
 * @dest_position: the position for @tab
 *
 * Reorders the page containing @tab, so that it appears in @dest_position position.
 * If dest_position is greater than or equal to the number of tabs
 * of the destination notebook or negative, tab will be moved to the
 * end of the tabs.
 */
void
lapiz_notebook_reorder_tab (LapizNotebook *src,
			    LapizTab      *tab,
			    gint           dest_position)
{
	gint old_position;

	g_return_if_fail (LAPIZ_IS_NOTEBOOK (src));
	g_return_if_fail (LAPIZ_IS_TAB (tab));

	old_position = ctk_notebook_page_num (CTK_NOTEBOOK (src),
				    	      CTK_WIDGET (tab));

	if (old_position == dest_position)
		return;

	ctk_notebook_reorder_child (CTK_NOTEBOOK (src),
				    CTK_WIDGET (tab),
				    dest_position);

	if (!src->priv->drag_in_progress)
	{
		g_signal_emit (G_OBJECT (src),
			       signals[TABS_REORDERED],
			       0);
	}
}

static void
drag_start (LapizNotebook *notebook,
            GdkEvent      *event)
{
	GdkSeat    *seat;
	GdkDevice  *device;
	GdkDisplay *display;

	display = ctk_widget_get_display (CTK_WIDGET (notebook));
	seat = cdk_display_get_default_seat (display);
	device = cdk_seat_get_pointer (seat);

	if (!leftdown) return;

	notebook->priv->drag_in_progress = TRUE;

	/* get a new cursor, if necessary */
	/* FIXME multi-head */
	if (cursor == NULL)
		cursor = cdk_cursor_new_for_display (display, CDK_FLEUR);

	/* grab the pointer */
	ctk_grab_add (CTK_WIDGET (notebook));

	/* FIXME multi-head */
	if (!cdk_display_device_is_grabbed (display, device))
	{
		cdk_seat_grab (seat,
			       ctk_widget_get_window (CTK_WIDGET (notebook)),
			       CDK_SEAT_CAPABILITY_POINTER,
			       FALSE,
			       cursor,
			       event,
			       NULL,
			       NULL);
	}
}

static void
drag_stop (LapizNotebook *notebook)
{
	if (notebook->priv->drag_in_progress)
	{
		g_signal_emit (G_OBJECT (notebook),
			       signals[TABS_REORDERED],
			       0);
	}

	notebook->priv->drag_in_progress = FALSE;
	if (notebook->priv->motion_notify_handler_id != 0)
	{
		g_signal_handler_disconnect (G_OBJECT (notebook),
					     notebook->priv->motion_notify_handler_id);
		notebook->priv->motion_notify_handler_id = 0;
	}
}

/* This function is only called during dnd, we don't need to emit TABS_REORDERED
 * here, instead we do it on drag_stop
 */
static void
move_current_tab (LapizNotebook *notebook,
	          gint           dest_position)
{
	gint cur_page_num;

	cur_page_num = ctk_notebook_get_current_page (CTK_NOTEBOOK (notebook));

	if (dest_position != cur_page_num)
	{
		CtkWidget *cur_tab;

		cur_tab = ctk_notebook_get_nth_page (CTK_NOTEBOOK (notebook),
						     cur_page_num);

		lapiz_notebook_reorder_tab (LAPIZ_NOTEBOOK (notebook),
					    LAPIZ_TAB (cur_tab),
					    dest_position);
	}
}

static gboolean
motion_notify_cb (LapizNotebook  *notebook,
		  GdkEventMotion *event,
		  gpointer        data)
{
	LapizNotebook *dest;
	gint page_num;
	gint result;

	if (notebook->priv->drag_in_progress == FALSE)
	{
		if (notebook->priv->tab_drag_and_drop_enabled == FALSE)
			return FALSE;

		if (ctk_drag_check_threshold (CTK_WIDGET (notebook),
					      notebook->priv->x_start,
					      notebook->priv->y_start,
					      event->x_root,
					      event->y_root))
		{
			if (drag_ready)
				drag_start (notebook, (GdkEvent *) event);
			else
				drag_stop (notebook);

			return TRUE;
		}

		return FALSE;
	}

	result = find_notebook_and_tab_at_pos ((gint)event->x_root,
					       (gint)event->y_root,
					       &dest,
					       &page_num);

	if (result != NOT_IN_APP_WINDOWS)
	{
		if (dest != notebook)
		{
			move_current_tab_to_another_notebook (notebook,
							      dest,
						      	      event,
						      	      page_num);
		}
		else
		{
			g_return_val_if_fail (page_num >= -1, FALSE);
			move_current_tab (notebook, page_num);
		}
	}

	return FALSE;
}

static void
move_current_tab_to_another_notebook (LapizNotebook  *src,
				      LapizNotebook  *dest,
				      GdkEventMotion *event,
				      gint            dest_position)
{
	LapizTab   *tab;
	gint        cur_page;
	GdkSeat    *seat;
	GdkDevice  *device;
	GdkDisplay *display;

	display = ctk_widget_get_display (CTK_WIDGET (CTK_NOTEBOOK (src)));
	seat = cdk_display_get_default_seat (display);
	device = cdk_seat_get_pointer (seat);

	/* This is getting tricky, the tab was dragged in a notebook
	 * in another window of the same app, we move the tab
	 * to that new notebook, and let this notebook handle the
	 * drag
	 */
	g_return_if_fail (LAPIZ_IS_NOTEBOOK (dest));
	g_return_if_fail (dest != src);

	cur_page = ctk_notebook_get_current_page (CTK_NOTEBOOK (src));
	tab = LAPIZ_TAB (ctk_notebook_get_nth_page (CTK_NOTEBOOK (src),
						    cur_page));

	/* stop drag in origin window */
	/* ungrab the pointer if it's grabbed */
	drag_stop (src);
	if (cdk_display_device_is_grabbed (display, device))
	{
		cdk_seat_ungrab (seat);
	}
	ctk_grab_remove (CTK_WIDGET (src));

	lapiz_notebook_move_tab (src, dest, tab, dest_position);

	/* start drag handling in dest notebook */
	dest->priv->motion_notify_handler_id =
		g_signal_connect (G_OBJECT (dest),
				  "motion-notify-event",
				  G_CALLBACK (motion_notify_cb),
				  NULL);

	drag_start (dest, (GdkEvent *) event);
}

static gboolean
button_release_cb (LapizNotebook  *notebook,
		   GdkEventButton *event,
		   gpointer        data)
{
	GdkSeat    *seat;
	GdkDevice  *device;
	GdkDisplay *display;

	display = ctk_widget_get_display (CTK_WIDGET (CTK_NOTEBOOK (notebook)));
	seat = cdk_display_get_default_seat (display);
	device = cdk_seat_get_pointer (seat);

	if (event->button == 1) leftdown = FALSE;

	if (notebook->priv->drag_in_progress)
	{
		gint cur_page_num;
		CtkWidget *cur_page;

		cur_page_num = ctk_notebook_get_current_page (CTK_NOTEBOOK (notebook));
		cur_page = ctk_notebook_get_nth_page (CTK_NOTEBOOK (notebook),
						      cur_page_num);

		/* CHECK: I don't follow the code here -- Paolo  */
		if (!is_in_notebook_window (notebook, event->x_root, event->y_root) &&
		    (ctk_notebook_get_n_pages (CTK_NOTEBOOK (notebook)) > 1))
		{
			/* Tab was detached */
			g_signal_emit (G_OBJECT (notebook),
				       signals[TAB_DETACHED],
				       0,
				       cur_page);
		}

		/* ungrab the pointer if it's grabbed */
		if (cdk_display_device_is_grabbed (display, device))
		{
			cdk_seat_ungrab (seat);
		}
		ctk_grab_remove (CTK_WIDGET (notebook));
	}

	/* This must be called even if a drag isn't happening */
	drag_stop (notebook);
	drag_ready = FALSE;

	return FALSE;
}

static gboolean
button_press_cb (LapizNotebook  *notebook,
		 GdkEventButton *event,
		 gpointer        data)
{
	static gboolean newfile = FALSE;
	static gint tab1click = -1;

	gint tab_clicked;

	if (notebook->priv->drag_in_progress)
		return TRUE;

	tab_clicked = find_tab_num_at_pos (notebook,
					   event->x_root,
					   event->y_root);

	if ((event->button == 1) &&
	    (event->type == CDK_BUTTON_PRESS) &&
	    (tab_clicked >= 0))
	{
		notebook->priv->x_start = event->x_root;
		notebook->priv->y_start = event->y_root;

		notebook->priv->motion_notify_handler_id =
			g_signal_connect (G_OBJECT (notebook),
					  "motion-notify-event",
					  G_CALLBACK (motion_notify_cb),
					  NULL);
	}
	else if ((event->type == CDK_BUTTON_PRESS) &&
		 (event->button == 3 || event->button == 2))
	{
		if (tab_clicked == -1)
		{
			// CHECK: do we really need it?

			/* consume event, so that we don't pop up the context menu when
			 * the mouse if not over a tab label
			 */
			return TRUE;
		}
		else
		{
			if (leftdown) {
				leftdown = FALSE;
				return TRUE;
			}

			/* Switch to the page the mouse is over, but don't consume the event */
			ctk_notebook_set_current_page (CTK_NOTEBOOK (notebook),
						       tab_clicked);
		}
	}

	if (event->button == 1)
	{
		if (event->type == CDK_BUTTON_PRESS)
		{
			tab1click = ctk_notebook_get_current_page (CTK_NOTEBOOK (notebook));

			if (newfile_ready)
				newfile = (tab_clicked == -1);
			else
				newfile = FALSE;
		}
		else if (event->type == CDK_2BUTTON_PRESS)
		{
			if ((tab1click != ctk_notebook_get_current_page (CTK_NOTEBOOK (notebook))) ||
			    (tab_clicked >= 0) || ((tab_clicked == -1) && (!newfile)) || (!leftdown))
				return TRUE;

			newfile = FALSE;
		}

	leftdown = TRUE;
	}

	return FALSE;
}

static gboolean
grab_focus_cb (LapizNotebook  *notebook,
	       GdkEventButton *event,
	       gpointer        data)
{
	drag_ready = TRUE;
	return FALSE;
}

static gboolean
focus_in_cb (LapizNotebook  *notebook,
	     GdkEventButton *event,
	     gpointer        data)
{
	newfile_ready = FALSE;
	return FALSE;
}

static gboolean
focus_out_cb (LapizNotebook  *notebook,
	      GdkEventButton *event,
	      gpointer        data)
{
	newfile_ready = TRUE;
	return FALSE;
}

/**
 * lapiz_notebook_new:
 *
 * Creates a new #LapizNotebook object.
 *
 * Returns: a new #LapizNotebook
 */
CtkWidget *
lapiz_notebook_new (void)
{
	return CTK_WIDGET (g_object_new (LAPIZ_TYPE_NOTEBOOK, NULL));
}

static void
lapiz_notebook_switch_page_cb (CtkNotebook     *notebook,
                               CtkWidget       *page,
                               guint            page_num,
                               gpointer         data)
{
	LapizNotebook *nb = LAPIZ_NOTEBOOK (notebook);
	CtkWidget *child;
	LapizView *view;

	child = ctk_notebook_get_nth_page (notebook, page_num);

	/* Remove the old page, we dont want to grow unnecessarily
	 * the list */
	if (nb->priv->focused_pages)
	{
		nb->priv->focused_pages =
			g_list_remove (nb->priv->focused_pages, child);
	}

	nb->priv->focused_pages = g_list_append (nb->priv->focused_pages,
						 child);

	/* give focus to the view */
	view = lapiz_tab_get_view (LAPIZ_TAB (child));
	ctk_widget_grab_focus (CTK_WIDGET (view));
}

/*
 * update_tabs_visibility: Hide tabs if there is only one tab
 * and the pref is not set.
 */
static void
update_tabs_visibility (LapizNotebook *nb)
{
	gboolean   show_tabs;
	guint      num;

	num = ctk_notebook_get_n_pages (CTK_NOTEBOOK (nb));

	show_tabs = (g_settings_get_boolean (lapiz_prefs_manager->settings, "show-single-tab") || num > 1);

	if (g_settings_get_boolean (lapiz_prefs_manager->settings, "show-tabs-with-side-pane"))
		ctk_notebook_set_show_tabs (CTK_NOTEBOOK (nb), show_tabs);
	else
	{
		if (lapiz_prefs_manager_get_side_pane_visible ())
			ctk_notebook_set_show_tabs (CTK_NOTEBOOK (nb), FALSE);
		else
			ctk_notebook_set_show_tabs (CTK_NOTEBOOK (nb), show_tabs);
	}
}

static void
lapiz_notebook_init (LapizNotebook *notebook)
{
	notebook->priv = lapiz_notebook_get_instance_private (notebook);

	notebook->priv->close_buttons_sensitive = TRUE;
	notebook->priv->tab_drag_and_drop_enabled = TRUE;

	ctk_notebook_set_scrollable (CTK_NOTEBOOK (notebook), TRUE);
	ctk_notebook_set_show_border (CTK_NOTEBOOK (notebook), FALSE);
	ctk_notebook_set_show_tabs (CTK_NOTEBOOK (notebook), FALSE);

	g_signal_connect (notebook,
			  "button-press-event",
			  (GCallback)button_press_cb,
			  NULL);

	g_signal_connect (notebook,
			  "button-release-event",
			  (GCallback)button_release_cb,
			  NULL);

	g_signal_connect (notebook,
			  "grab-focus",
			  (GCallback)grab_focus_cb,
			  NULL);

	g_signal_connect (notebook,
			  "focus-in-event",
			  (GCallback)focus_in_cb,
			  NULL);

	g_signal_connect (notebook,
			  "focus-out-event",
			  (GCallback)focus_out_cb,
			  NULL);

	ctk_widget_add_events (CTK_WIDGET (notebook),
			       CDK_BUTTON1_MOTION_MASK);

	g_signal_connect_after (G_OBJECT (notebook),
				"switch_page",
                                G_CALLBACK (lapiz_notebook_switch_page_cb),
                                NULL);
}

static void
lapiz_notebook_finalize (GObject *object)
{
	LapizNotebook *notebook = LAPIZ_NOTEBOOK (object);

	g_list_free (notebook->priv->focused_pages);

	G_OBJECT_CLASS (lapiz_notebook_parent_class)->finalize (object);
}

/*
 * We need to override this because when we don't show the tabs, like in
 * fullscreen we need to have wrap around too
 */
static gboolean
lapiz_notebook_change_current_page (CtkNotebook *notebook,
				    gint         offset)
{
	gboolean wrap_around;
	gint current;

	current = ctk_notebook_get_current_page (notebook);

	if (current != -1)
	{
		current = current + offset;

		g_object_get (ctk_widget_get_settings (CTK_WIDGET (notebook)),
			      "ctk-keynav-wrap-around", &wrap_around,
			      NULL);

		if (wrap_around)
		{
			if (current < 0)
			{
				current = ctk_notebook_get_n_pages (notebook) - 1;
			}
			else if (current >= ctk_notebook_get_n_pages (notebook))
			{
				current = 0;
			}
		}

		ctk_notebook_set_current_page (notebook, current);
	}
	else
	{
		ctk_widget_error_bell (CTK_WIDGET (notebook));
	}

	return TRUE;
}

static void
close_button_clicked_cb (LapizTabLabel *tab_label, LapizNotebook *notebook)
{
	LapizTab *tab;

	tab = lapiz_tab_label_get_tab (tab_label);
	g_signal_emit (notebook, signals[TAB_CLOSE_REQUEST], 0, tab);
}

static CtkWidget *
create_tab_label (LapizNotebook *nb,
		  LapizTab      *tab)
{
	CtkWidget *tab_label;

	tab_label = lapiz_tab_label_new (tab);

	g_signal_connect (tab_label,
			  "close-clicked",
			  G_CALLBACK (close_button_clicked_cb),
			  nb);

	g_object_set_data (G_OBJECT (tab), "tab-label", tab_label);

	return tab_label;
}

static CtkWidget *
get_tab_label (LapizTab *tab)
{
	CtkWidget *tab_label;

	tab_label = CTK_WIDGET (g_object_get_data (G_OBJECT (tab), "tab-label"));
	g_return_val_if_fail (tab_label != NULL, NULL);

	return tab_label;
}

static void
remove_tab_label (LapizNotebook *nb,
		  LapizTab      *tab)
{
	CtkWidget *tab_label;

	tab_label = get_tab_label (tab);

	g_signal_handlers_disconnect_by_func (tab_label,
					      G_CALLBACK (close_button_clicked_cb),
					      nb);

	g_object_set_data (G_OBJECT (tab), "tab-label", NULL);
}

/**
 * lapiz_notebook_add_tab:
 * @nb: a #LapizNotebook
 * @tab: a #LapizTab
 * @position: the position where the @tab should be added
 * @jump_to: %TRUE to set the @tab as active
 *
 * Adds the specified @tab to the @nb.
 */
void
lapiz_notebook_add_tab (LapizNotebook *nb,
		        LapizTab      *tab,
		        gint           position,
		        gboolean       jump_to)
{
	CtkWidget *tab_label;

	g_return_if_fail (LAPIZ_IS_NOTEBOOK (nb));
	g_return_if_fail (LAPIZ_IS_TAB (tab));

	tab_label = create_tab_label (nb, tab);
	ctk_notebook_insert_page (CTK_NOTEBOOK (nb),
				  CTK_WIDGET (tab),
				  tab_label,
				  position);
	update_tabs_visibility (nb);

	g_signal_emit (G_OBJECT (nb), signals[TAB_ADDED], 0, tab);

	/* The signal handler may have reordered the tabs */
	position = ctk_notebook_page_num (CTK_NOTEBOOK (nb),
					  CTK_WIDGET (tab));

	if (jump_to)
	{
		LapizView *view;

		ctk_notebook_set_current_page (CTK_NOTEBOOK (nb), position);
		g_object_set_data (G_OBJECT (tab),
				   "jump_to",
				   GINT_TO_POINTER (jump_to));
		view = lapiz_tab_get_view (tab);

		ctk_widget_grab_focus (CTK_WIDGET (view));
	}
}

static void
smart_tab_switching_on_closure (LapizNotebook *nb,
				LapizTab      *tab)
{
	gboolean jump_to;

	jump_to = GPOINTER_TO_INT (g_object_get_data
				   (G_OBJECT (tab), "jump_to"));

	if (!jump_to || !nb->priv->focused_pages)
	{
		ctk_notebook_next_page (CTK_NOTEBOOK (nb));
	}
	else
	{
		GList *l;
		CtkWidget *child;
		int page_num;

		/* activate the last focused tab */
		l = g_list_last (nb->priv->focused_pages);
		child = CTK_WIDGET (l->data);
		page_num = ctk_notebook_page_num (CTK_NOTEBOOK (nb),
						  child);
		ctk_notebook_set_current_page (CTK_NOTEBOOK (nb),
					       page_num);
	}
}

static void
remove_tab (LapizTab      *tab,
	    LapizNotebook *nb)
{
	gint position;

	position = ctk_notebook_page_num (CTK_NOTEBOOK (nb), CTK_WIDGET (tab));

	/* we ref the tab so that it's still alive while the tabs_removed
	 * signal is processed.
	 */
	g_object_ref (tab);

	remove_tab_label (nb, tab);
	ctk_notebook_remove_page (CTK_NOTEBOOK (nb), position);
	update_tabs_visibility (nb);

	g_signal_emit (G_OBJECT (nb), signals[TAB_REMOVED], 0, tab);

	g_object_unref (tab);
}

/**
 * lapiz_notebook_remove_tab:
 * @nb: a #LapizNotebook
 * @tab: a #LapizTab
 *
 * Removes @tab from @nb.
 */
void
lapiz_notebook_remove_tab (LapizNotebook *nb,
			   LapizTab      *tab)
{
	gint position, curr;

	g_return_if_fail (LAPIZ_IS_NOTEBOOK (nb));
	g_return_if_fail (LAPIZ_IS_TAB (tab));

	/* Remove the page from the focused pages list */
	nb->priv->focused_pages =  g_list_remove (nb->priv->focused_pages,
						  tab);

	position = ctk_notebook_page_num (CTK_NOTEBOOK (nb), CTK_WIDGET (tab));
	curr = ctk_notebook_get_current_page (CTK_NOTEBOOK (nb));

	if (position == curr)
	{
		smart_tab_switching_on_closure (nb, tab);
	}

	remove_tab (tab, nb);
}

/**
 * lapiz_notebook_remove_all_tabs:
 * @nb: a #LapizNotebook
 *
 * Removes all #LapizTab from @nb.
 */
void
lapiz_notebook_remove_all_tabs (LapizNotebook *nb)
{
	g_return_if_fail (LAPIZ_IS_NOTEBOOK (nb));

	g_list_free (nb->priv->focused_pages);
	nb->priv->focused_pages = NULL;

	ctk_container_foreach (CTK_CONTAINER (nb),
			       (CtkCallback)remove_tab,
			       nb);
}

static void
set_close_buttons_sensitivity (LapizTab      *tab,
                               LapizNotebook *nb)
{
	CtkWidget *tab_label;

	tab_label = get_tab_label (tab);

	lapiz_tab_label_set_close_button_sensitive (LAPIZ_TAB_LABEL (tab_label),
						    nb->priv->close_buttons_sensitive);
}

/**
 * lapiz_notebook_set_close_buttons_sensitive:
 * @nb: a #LapizNotebook
 * @sensitive: %TRUE to make the buttons sensitive
 *
 * Sets whether the close buttons in the tabs of @nb are sensitive.
 */
void
lapiz_notebook_set_close_buttons_sensitive (LapizNotebook *nb,
					    gboolean       sensitive)
{
	g_return_if_fail (LAPIZ_IS_NOTEBOOK (nb));

	sensitive = (sensitive != FALSE);

	if (sensitive == nb->priv->close_buttons_sensitive)
		return;

	nb->priv->close_buttons_sensitive = sensitive;

	ctk_container_foreach (CTK_CONTAINER (nb),
			       (CtkCallback)set_close_buttons_sensitivity,
			       nb);
}

/**
 * lapiz_notebook_get_close_buttons_sensitive:
 * @nb: a #LapizNotebook
 *
 * Whether the close buttons are sensitive.
 *
 * Returns: %TRUE if the close buttons are sensitive
 */
gboolean
lapiz_notebook_get_close_buttons_sensitive (LapizNotebook *nb)
{
	g_return_val_if_fail (LAPIZ_IS_NOTEBOOK (nb), TRUE);

	return nb->priv->close_buttons_sensitive;
}

/**
 * lapiz_notebook_set_tab_drag_and_drop_enabled:
 * @nb: a #LapizNotebook
 * @enable: %TRUE to enable the drag and drop
 *
 * Sets whether drag and drop of tabs in the @nb is enabled.
 */
void
lapiz_notebook_set_tab_drag_and_drop_enabled (LapizNotebook *nb,
					      gboolean       enable)
{
	g_return_if_fail (LAPIZ_IS_NOTEBOOK (nb));

	enable = (enable != FALSE);

	if (enable == nb->priv->tab_drag_and_drop_enabled)
		return;

	nb->priv->tab_drag_and_drop_enabled = enable;
}

/**
 * lapiz_notebook_get_tab_drag_and_drop_enabled:
 * @nb: a #LapizNotebook
 *
 * Whether the drag and drop is enabled in the @nb.
 *
 * Returns: %TRUE if the drag and drop is enabled.
 */
gboolean
lapiz_notebook_get_tab_drag_and_drop_enabled (LapizNotebook *nb)
{
	g_return_val_if_fail (LAPIZ_IS_NOTEBOOK (nb), TRUE);

	return nb->priv->tab_drag_and_drop_enabled;
}

