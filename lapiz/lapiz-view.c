/*
 * lapiz-view.c
 * This file is part of lapiz
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2002 Chema Celorio, Paolo Maggi
 * Copyright (C) 2003-2005 Paolo Maggi
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

#include <string.h>
#include <stdlib.h>

#include <ctk/ctk.h>
#include <cdk/cdkkeysyms.h>

#include <glib/gi18n.h>

#include "lapiz-view.h"
#include "lapiz-debug.h"
#include "lapiz-prefs-manager.h"
#include "lapiz-prefs-manager-app.h"
#include "lapiz-prefs-manager-private.h"
#include "lapiz-marshal.h"
#include "lapiz-utils.h"

#define LAPIZ_VIEW_SCROLL_MARGIN 0.02
#define LAPIZ_VIEW_SEARCH_DIALOG_TIMEOUT (30*1000) /* 30 seconds */

#define MIN_SEARCH_COMPLETION_KEY_LEN	3

/* Local variables */
static gboolean middle_or_right_down = FALSE;

typedef enum
{
	GOTO_LINE,
	SEARCH
} SearchMode;

enum
{
	TARGET_URI_LIST = 100
};

struct _LapizViewPrivate
{
	SearchMode   search_mode;

	CtkTextIter  start_search_iter;

	/* used to restore the search state if an
	 * incremental search is cancelled
	 */
 	gchar       *old_search_text;
	guint        old_search_flags;

	/* used to remeber the state of the last
	 * incremental search (the document search
	 * state may be changed by the search dialog)
	 */
	guint        search_flags;
	gboolean     wrap_around;

	CtkWidget   *search_window;
	CtkWidget   *search_entry;

	guint        typeselect_flush_timeout;
	guint        search_entry_changed_id;

	gboolean     disable_popdown;

	CtkTextBuffer *current_buffer;
};

/* The search entry completion is shared among all the views */
CtkListStore *search_completion_model = NULL;

static void	lapiz_view_dispose		(GObject          *object);
static void	lapiz_view_finalize		(GObject          *object);
static gint     lapiz_view_focus_out		(CtkWidget        *widget,
						 GdkEventFocus    *event);
static gboolean lapiz_view_scroll_event         (CtkWidget        *widget,
                                                 GdkEventScroll   *event);
static gboolean lapiz_view_drag_motion		(CtkWidget        *widget,
						 GdkDragContext   *context,
						 gint              x,
						 gint              y,
						 guint             timestamp);
static void     lapiz_view_drag_data_received   (CtkWidget        *widget,
						 GdkDragContext   *context,
						 gint              x,
						 gint              y,
						 CtkSelectionData *selection_data,
						 guint             info,
						 guint             timestamp);
static gboolean lapiz_view_drag_drop		(CtkWidget        *widget,
	      					 GdkDragContext   *context,
	      					 gint              x,
	      					 gint              y,
	      					 guint             timestamp);

static gboolean	lapiz_view_button_press_event	(CtkWidget        *widget,
						 GdkEventButton   *event);
static gboolean	lapiz_view_button_release_event	(CtkWidget        *widget,
						 GdkEventButton   *event);
static void	lapiz_view_populate_popup	(CtkTextView      *text_view,
						 CtkWidget        *widget);

static gboolean start_interactive_search	(LapizView        *view);
static gboolean start_interactive_goto_line	(LapizView        *view);
static gboolean reset_searched_text		(LapizView        *view);

static void	hide_search_window 		(LapizView        *view,
						 gboolean          cancel);

static gboolean	lapiz_view_draw		 	(CtkWidget        *widget,
						 cairo_t          *cr);
static void 	search_highlight_updated_cb	(LapizDocument    *doc,
						 CtkTextIter      *start,
						 CtkTextIter      *end,
						 LapizView        *view);

static void	lapiz_view_delete_from_cursor 	(CtkTextView     *text_view,
						 CtkDeleteType    type,
						 gint             count);

G_DEFINE_TYPE_WITH_PRIVATE (LapizView, lapiz_view, CTK_SOURCE_TYPE_VIEW)

/* Signals */
enum
{
	START_INTERACTIVE_SEARCH,
	START_INTERACTIVE_GOTO_LINE,
	RESET_SEARCHED_TEXT,
	DROP_URIS,
	LAST_SIGNAL
};

static guint view_signals [LAST_SIGNAL] = { 0 };

typedef enum
{
	LAPIZ_SEARCH_ENTRY_NORMAL,
	LAPIZ_SEARCH_ENTRY_NOT_FOUND
} LapizSearchEntryState;

static void
document_read_only_notify_handler (LapizDocument *document,
			           GParamSpec    *pspec,
				   LapizView     *view)
{
	lapiz_debug (DEBUG_VIEW);

	ctk_text_view_set_editable (CTK_TEXT_VIEW (view),
				    !lapiz_document_get_readonly (document));
}

static gboolean
lapiz_view_scroll_event (CtkWidget      *widget,
                         GdkEventScroll *event)
{
	if (event->direction == GDK_SCROLL_UP)
	{
		event->delta_x = 0;
		event->delta_y = -1;
	}
	else if (event->direction == GDK_SCROLL_DOWN)
	{
		event->delta_x = 0;
		event->delta_y = 1;
	}
	else if (event->direction == GDK_SCROLL_LEFT)
	{
		event->delta_x = -1;
		event->delta_y = 0;
	}
	else if (event->direction == GDK_SCROLL_RIGHT)
	{
		event->delta_x = 1;
		event->delta_y = 0;
	}

	event->direction = GDK_SCROLL_SMOOTH;

	return FALSE;
}

static void
lapiz_view_class_init (LapizViewClass *klass)
{
	GObjectClass     *object_class = G_OBJECT_CLASS (klass);
	CtkWidgetClass   *widget_class = CTK_WIDGET_CLASS (klass);
	CtkTextViewClass *text_view_class = CTK_TEXT_VIEW_CLASS (klass);

	CtkBindingSet    *binding_set;

	object_class->dispose = lapiz_view_dispose;
	object_class->finalize = lapiz_view_finalize;

	widget_class->focus_out_event = lapiz_view_focus_out;
	widget_class->draw = lapiz_view_draw;
	widget_class->scroll_event = lapiz_view_scroll_event;

	/*
	 * Override the ctk_text_view_drag_motion and drag_drop
	 * functions to get URIs
	 *
	 * If the mime type is text/uri-list, then we will accept
	 * the potential drop, or request the data (depending on the
	 * function).
	 *
	 * If the drag context has any other mime type, then pass the
	 * information onto the CtkTextView's standard handlers.
	 * (widget_class->function_name).
	 *
	 * See bug #89881 for details
	 */
	widget_class->drag_motion = lapiz_view_drag_motion;
	widget_class->drag_data_received = lapiz_view_drag_data_received;
	widget_class->drag_drop = lapiz_view_drag_drop;
	widget_class->button_press_event = lapiz_view_button_press_event;
	widget_class->button_release_event = lapiz_view_button_release_event;
	text_view_class->populate_popup = lapiz_view_populate_popup;
	klass->start_interactive_search = start_interactive_search;
	klass->start_interactive_goto_line = start_interactive_goto_line;
	klass->reset_searched_text = reset_searched_text;

	text_view_class->delete_from_cursor = lapiz_view_delete_from_cursor;

	view_signals[START_INTERACTIVE_SEARCH] =
    		g_signal_new ("start_interactive_search",
		  	      G_TYPE_FROM_CLASS (object_class),
		  	      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  	      G_STRUCT_OFFSET (LapizViewClass, start_interactive_search),
			      NULL, NULL,
			      lapiz_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN, 0);

	view_signals[START_INTERACTIVE_GOTO_LINE] =
    		g_signal_new ("start_interactive_goto_line",
		  	      G_TYPE_FROM_CLASS (object_class),
		  	      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  	      G_STRUCT_OFFSET (LapizViewClass, start_interactive_goto_line),
			      NULL, NULL,
			      lapiz_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN, 0);

	view_signals[RESET_SEARCHED_TEXT] =
    		g_signal_new ("reset_searched_text",
		  	      G_TYPE_FROM_CLASS (object_class),
		  	      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  	      G_STRUCT_OFFSET (LapizViewClass, reset_searched_text),
			      NULL, NULL,
			      lapiz_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN, 0);

	/* A new signal DROP_URIS has been added to allow plugins to intercept
	 * the default dnd behaviour of 'text/uri-list'. LapizView now handles
	 * dnd in the default handlers of drag_drop, drag_motion and
	 * drag_data_received. The view emits drop_uris from drag_data_received
	 * if valid uris have been dropped. Plugins should connect to
	 * drag_motion, drag_drop and drag_data_received to change this
	 * default behaviour. They should _NOT_ use this signal because this
	 * will not prevent lapiz from loading the uri
	 */
	view_signals[DROP_URIS] =
    		g_signal_new ("drop_uris",
		  	      G_TYPE_FROM_CLASS (object_class),
		  	      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  	      G_STRUCT_OFFSET (LapizViewClass, drop_uris),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE, 1, G_TYPE_STRV);

	binding_set = ctk_binding_set_by_class (klass);

	ctk_binding_entry_add_signal (binding_set,
				      GDK_KEY_k,
				      GDK_CONTROL_MASK,
				      "start_interactive_search", 0);

	ctk_binding_entry_add_signal (binding_set,
				      GDK_KEY_i,
				      GDK_CONTROL_MASK,
				      "start_interactive_goto_line", 0);

	ctk_binding_entry_add_signal (binding_set,
				      GDK_KEY_k,
				      GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				      "reset_searched_text", 0);

	ctk_binding_entry_add_signal (binding_set,
				      GDK_KEY_d,
				      GDK_CONTROL_MASK,
				      "delete_from_cursor", 2,
				      G_TYPE_ENUM, CTK_DELETE_PARAGRAPHS,
				      G_TYPE_INT, 1);
}

static void
current_buffer_removed (LapizView *view)
{
	if (view->priv->current_buffer)
	{
		g_signal_handlers_disconnect_by_func (view->priv->current_buffer,
						      document_read_only_notify_handler,
						      view);
		g_signal_handlers_disconnect_by_func (view->priv->current_buffer,
						      search_highlight_updated_cb,
						      view);

		g_object_unref (view->priv->current_buffer);
		view->priv->current_buffer = NULL;
	}
}

static void
on_notify_buffer_cb (LapizView  *view,
		     GParamSpec *arg1,
		     gpointer    userdata)
{
	CtkTextBuffer *buffer;

	current_buffer_removed (view);
	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));

	if (buffer == NULL || !LAPIZ_IS_DOCUMENT (buffer))
		return;

	view->priv->current_buffer = g_object_ref (buffer);
	g_signal_connect (buffer,
			  "notify::read-only",
			  G_CALLBACK (document_read_only_notify_handler),
			  view);

	ctk_text_view_set_editable (CTK_TEXT_VIEW (view),
				    !lapiz_document_get_readonly (LAPIZ_DOCUMENT (buffer)));

	g_signal_connect (buffer,
			  "search_highlight_updated",
			  G_CALLBACK (search_highlight_updated_cb),
			  view);
}

#ifdef CTK_SOURCE_VERSION_3_24
void
lapiz_set_source_space_drawer_by_level (CtkSourceView *view,
                                        gint level,
                                        CtkSourceSpaceTypeFlags type)
{
	CtkSourceSpaceLocationFlags locs[] = {CTK_SOURCE_SPACE_LOCATION_LEADING,
	                                      CTK_SOURCE_SPACE_LOCATION_INSIDE_TEXT,
	                                      CTK_SOURCE_SPACE_LOCATION_TRAILING};
	/* this array links the level to the location */
	CtkSourceSpaceLocationFlags levels[] = {
	                                        0,
	                                        CTK_SOURCE_SPACE_LOCATION_TRAILING,
	                                        CTK_SOURCE_SPACE_LOCATION_INSIDE_TEXT |
	                                        	CTK_SOURCE_SPACE_LOCATION_TRAILING |
	                                        	CTK_SOURCE_SPACE_LOCATION_LEADING
	};

	gint i;

	CtkSourceSpaceDrawer *drawer = ctk_source_view_get_space_drawer (view);

	if (level >= G_N_ELEMENTS(levels) || level < 0)
		level = 0;

	for (i = 0 ; i < G_N_ELEMENTS(locs) ; i++ ) {
		CtkSourceSpaceTypeFlags f;
		f = ctk_source_space_drawer_get_types_for_locations (drawer,
		                                                     locs[i]);
		if (locs[i] & levels[level])
			f |= type;
		else
			f &= ~type;
		ctk_source_space_drawer_set_types_for_locations (drawer, locs[i], f);
	}
}
#endif

#ifdef CTK_SOURCE_VERSION_3_24
static void
lapiz_set_source_space_drawer (CtkSourceView *view)
{
	lapiz_set_source_space_drawer_by_level (view,
	                                        lapiz_prefs_manager_get_draw_spaces (),
	                                        CTK_SOURCE_SPACE_TYPE_SPACE);
	lapiz_set_source_space_drawer_by_level (view,
	                                        lapiz_prefs_manager_get_draw_tabs (),
	                                        CTK_SOURCE_SPACE_TYPE_TAB);
	lapiz_set_source_space_drawer_by_level (view,
	                                        lapiz_prefs_manager_get_draw_newlines () ? 2 : 0,
	                                        CTK_SOURCE_SPACE_TYPE_NEWLINE);
	lapiz_set_source_space_drawer_by_level (view,
	                                        lapiz_prefs_manager_get_draw_nbsp (),
	                                        CTK_SOURCE_SPACE_TYPE_NBSP);
	ctk_source_space_drawer_set_enable_matrix (ctk_source_view_get_space_drawer (view),
	                                           TRUE);

}
#else
static void
lapiz_set_source_space_drawer (CtkSourceView *view)
{
	CtkSourceDrawSpacesFlags flags = 0;

	if (lapiz_prefs_manager_get_draw_spaces () > 0)
		flags |= CTK_SOURCE_DRAW_SPACES_SPACE;
	if (lapiz_prefs_manager_get_draw_tabs () > 0)
		flags |= CTK_SOURCE_DRAW_SPACES_TAB;
	if (lapiz_prefs_manager_get_draw_newlines ())
		flags |= CTK_SOURCE_DRAW_SPACES_NEWLINE;
	if (lapiz_prefs_manager_get_draw_nbsp () > 0)
		flags |= CTK_SOURCE_DRAW_SPACES_NBSP;

	flags |= CTK_SOURCE_DRAW_SPACES_TRAILING |
		CTK_SOURCE_DRAW_SPACES_TEXT |
		CTK_SOURCE_DRAW_SPACES_LEADING;

	ctk_source_view_set_draw_spaces (view, flags);
}
#endif

static void
lapiz_view_init (LapizView *view)
{
	CtkTargetList *tl;

	lapiz_debug (DEBUG_VIEW);

	view->priv = lapiz_view_get_instance_private (view);

	/*
	 *  Set tab, fonts, wrap mode, colors, etc. according
	 *  to preferences
	 */
	if (!lapiz_prefs_manager_get_use_default_font ())
	{
		gchar *editor_font;

		editor_font = lapiz_prefs_manager_get_editor_font ();

		lapiz_view_set_font (view, FALSE, editor_font);

		g_free (editor_font);
	}
	else
	{
		lapiz_view_set_font (view, TRUE, NULL);
	}

	g_object_set (G_OBJECT (view),
		      "wrap_mode", lapiz_prefs_manager_get_wrap_mode (),
		      "show_line_numbers", lapiz_prefs_manager_get_display_line_numbers (),
		      "auto_indent", lapiz_prefs_manager_get_auto_indent (),
		      "tab_width", lapiz_prefs_manager_get_tabs_size (),
		      "insert_spaces_instead_of_tabs", lapiz_prefs_manager_get_insert_spaces (),
		      "show_right_margin", lapiz_prefs_manager_get_display_right_margin (),
		      "right_margin_position", lapiz_prefs_manager_get_right_margin_position (),
		      "highlight_current_line", lapiz_prefs_manager_get_highlight_current_line (),
		      "smart_home_end", lapiz_prefs_manager_get_smart_home_end (),
		      "indent_on_tab", TRUE,
		      NULL);

	lapiz_set_source_space_drawer (CTK_SOURCE_VIEW (view));

	view->priv->typeselect_flush_timeout = 0;
	view->priv->wrap_around = TRUE;

	/* Drag and drop support */
	tl = ctk_drag_dest_get_target_list (CTK_WIDGET (view));

	if (tl != NULL)
		ctk_target_list_add_uri_targets (tl, TARGET_URI_LIST);

	/* Act on buffer change */
	g_signal_connect (view,
			  "notify::buffer",
			  G_CALLBACK (on_notify_buffer_cb),
			  NULL);
}

static void
lapiz_view_dispose (GObject *object)
{
	LapizView *view;

	view = LAPIZ_VIEW (object);

	if (view->priv->search_window != NULL)
	{
		ctk_widget_destroy (view->priv->search_window);
		view->priv->search_window = NULL;
		view->priv->search_entry = NULL;

		if (view->priv->typeselect_flush_timeout != 0)
		{
			g_source_remove (view->priv->typeselect_flush_timeout);
			view->priv->typeselect_flush_timeout = 0;
		}
	}

	/* Disconnect notify buffer because the destroy of the textview will
	   set the buffer to NULL, and we call get_buffer in the notify which
	   would reinstate a CtkTextBuffer which we don't want */
	current_buffer_removed (view);
	g_signal_handlers_disconnect_by_func (view, on_notify_buffer_cb, NULL);

	(* G_OBJECT_CLASS (lapiz_view_parent_class)->dispose) (object);
}

static void
lapiz_view_finalize (GObject *object)
{
	LapizView *view;

	view = LAPIZ_VIEW (object);

	current_buffer_removed (view);

	g_free (view->priv->old_search_text);

	(* G_OBJECT_CLASS (lapiz_view_parent_class)->finalize) (object);
}

static gint
lapiz_view_focus_out (CtkWidget *widget, GdkEventFocus *event)
{
	LapizView *view = LAPIZ_VIEW (widget);

	ctk_widget_queue_draw (widget);

	/* hide interactive search dialog */
	if (view->priv->search_window != NULL)
		hide_search_window (view, FALSE);

	(* CTK_WIDGET_CLASS (lapiz_view_parent_class)->focus_out_event) (widget, event);

	return FALSE;
}

/**
 * lapiz_view_new:
 * @doc: a #LapizDocument
 *
 * Creates a new #LapizView object displaying the @doc document.
 * @doc cannot be %NULL.
 *
 * Return value: a new #LapizView
 **/
CtkWidget *
lapiz_view_new (LapizDocument *doc)
{
	CtkWidget *view;

	lapiz_debug_message (DEBUG_VIEW, "START");

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);

	view = CTK_WIDGET (g_object_new (LAPIZ_TYPE_VIEW, "buffer", doc, NULL));

	lapiz_debug_message (DEBUG_VIEW, "END: %d", G_OBJECT (view)->ref_count);

	ctk_widget_show_all (view);

	return view;
}

void
lapiz_view_cut_clipboard (LapizView *view)
{
	CtkTextBuffer *buffer;
	CtkClipboard *clipboard;

	lapiz_debug (DEBUG_VIEW);

	g_return_if_fail (LAPIZ_IS_VIEW (view));

	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	clipboard = ctk_widget_get_clipboard (CTK_WIDGET (view),
					      GDK_SELECTION_CLIPBOARD);

	/* FIXME: what is default editability of a buffer? */
  	ctk_text_buffer_cut_clipboard (buffer,
  				       clipboard,
				       !lapiz_document_get_readonly (
				       		LAPIZ_DOCUMENT (buffer)));

	ctk_text_view_scroll_to_mark (CTK_TEXT_VIEW (view),
				      ctk_text_buffer_get_insert (buffer),
				      LAPIZ_VIEW_SCROLL_MARGIN,
				      FALSE,
				      0.0,
				      0.0);
}

void
lapiz_view_copy_clipboard (LapizView *view)
{
	CtkTextBuffer *buffer;
	CtkClipboard *clipboard;

	lapiz_debug (DEBUG_VIEW);

	g_return_if_fail (LAPIZ_IS_VIEW (view));

	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	clipboard = ctk_widget_get_clipboard (CTK_WIDGET (view),
					      GDK_SELECTION_CLIPBOARD);

  	ctk_text_buffer_copy_clipboard (buffer, clipboard);

	/* on copy do not scroll, we are already on screen */
}

void
lapiz_view_paste_clipboard (LapizView *view)
{
  	CtkTextBuffer *buffer;
	CtkClipboard *clipboard;

	lapiz_debug (DEBUG_VIEW);

	g_return_if_fail (LAPIZ_IS_VIEW (view));

	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	clipboard = ctk_widget_get_clipboard (CTK_WIDGET (view),
					      GDK_SELECTION_CLIPBOARD);

	/* FIXME: what is default editability of a buffer? */
  	ctk_text_buffer_paste_clipboard (buffer,
					 clipboard,
					 NULL,
					 !lapiz_document_get_readonly (
						LAPIZ_DOCUMENT (buffer)));

	ctk_text_view_scroll_to_mark (CTK_TEXT_VIEW (view),
				      ctk_text_buffer_get_insert (buffer),
				      LAPIZ_VIEW_SCROLL_MARGIN,
				      FALSE,
				      0.0,
				      0.0);
}

/**
 * lapiz_view_delete_selection:
 * @view: a #LapizView
 *
 * Deletes the text currently selected in the #CtkTextBuffer associated
 * to the view and scroll to the cursor position.
 **/
void
lapiz_view_delete_selection (LapizView *view)
{
  	CtkTextBuffer *buffer = NULL;

	lapiz_debug (DEBUG_VIEW);

	g_return_if_fail (LAPIZ_IS_VIEW (view));

	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	/* FIXME: what is default editability of a buffer? */
	ctk_text_buffer_delete_selection (buffer,
					  TRUE,
					  !lapiz_document_get_readonly (
						LAPIZ_DOCUMENT (buffer)));

	ctk_text_view_scroll_to_mark (CTK_TEXT_VIEW (view),
				      ctk_text_buffer_get_insert (buffer),
				      LAPIZ_VIEW_SCROLL_MARGIN,
				      FALSE,
				      0.0,
				      0.0);
}

/**
 * lapiz_view_select_all:
 * @view: a #LapizView
 *
 * Selects all the text displayed in the @view.
 **/
void
lapiz_view_select_all (LapizView *view)
{
	CtkTextBuffer *buffer = NULL;
	CtkTextIter start, end;

	lapiz_debug (DEBUG_VIEW);

	g_return_if_fail (LAPIZ_IS_VIEW (view));

	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	ctk_text_buffer_get_bounds (buffer, &start, &end);
	ctk_text_buffer_select_range (buffer, &start, &end);
}

/**
 * lapiz_view_scroll_to_cursor:
 * @view: a #LapizView
 *
 * Scrolls the @view to the cursor position.
 **/
void
lapiz_view_scroll_to_cursor (LapizView *view)
{
	CtkTextBuffer* buffer = NULL;

	lapiz_debug (DEBUG_VIEW);

	g_return_if_fail (LAPIZ_IS_VIEW (view));

	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	ctk_text_view_scroll_to_mark (CTK_TEXT_VIEW (view),
				      ctk_text_buffer_get_insert (buffer),
				      0.25,
				      FALSE,
				      0.0,
				      0.0);
}

static PangoFontDescription* get_system_font (void)
{
	PangoFontDescription *desc = NULL;
	char *name;

	name = g_settings_get_string (lapiz_prefs_manager->interface_settings, "font-name");

	if (name)
	{
		desc = pango_font_description_from_string (name);
		g_free (name);
	}

	return desc;
}

static void
system_font_changed_cb (GSettings *settings,
			gchar     *key,
			gpointer   user_data)
{
	PangoFontDescription *sys_font_desc = NULL;
	sys_font_desc = get_system_font ();
	if (sys_font_desc)
	{
		lapiz_override_font ("label", NULL, sys_font_desc);
		pango_font_description_free (sys_font_desc);
	}
}

void
lapiz_override_font (const gchar          *item,
		     CtkWidget            *widget,
		     PangoFontDescription *font)
{
	static CtkCssProvider *provider = NULL; /*We need to keep this as long as Lapiz is running*/
	gchar          *prov_str;
	gchar          *css;
	gchar          *family;
	gchar          *weight;
	const gchar    *style;
	gchar          *size;

	family = g_strdup_printf ("font-family: %s;", pango_font_description_get_family (font));

	weight = g_strdup_printf ("font-weight: %d;", pango_font_description_get_weight (font));

	if (pango_font_description_get_style (font) == PANGO_STYLE_NORMAL)
		style = "font-style: normal;";
	else if (pango_font_description_get_style (font) == PANGO_STYLE_ITALIC)
		style = "font-style: italic;";
	else
		style = "font-style: oblique;";

	size = g_strdup_printf ("font-size: %d%s;",
				pango_font_description_get_size (font) / PANGO_SCALE,
				pango_font_description_get_size_is_absolute (font) ? "px" : "pt");

	if (provider == NULL)
	{
		provider = ctk_css_provider_new ();

		g_signal_connect (lapiz_prefs_manager->interface_settings,
				  "changed::" "font-name",
				  G_CALLBACK (system_font_changed_cb), NULL);

		ctk_style_context_add_provider_for_screen (ctk_widget_get_screen (widget),
							   CTK_STYLE_PROVIDER (provider),
							   CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	prov_str = ctk_css_provider_to_string (provider);

	if (g_str_has_prefix (prov_str, "textview") && g_str_has_prefix (item, "label"))
	{
		if (strstr (prov_str, "label"))
		{
			g_strdelimit (prov_str, "}", '\0');
			gchar *prov_new_str = g_strdup_printf ("%s}", prov_str);
			css = g_strdup_printf ("%s %s { %s %s %s %s }", prov_new_str, item, family, weight, style, size);
			g_free (prov_new_str);
		}
		else
			css = g_strdup_printf ("%s %s { %s %s %s %s }", prov_str, item, family, weight, style, size);
	}
	else
		css = g_strdup_printf ("%s { %s %s %s %s }", item, family, weight, style, size);

	ctk_css_provider_load_from_data (provider, css, -1, NULL);

	g_free (css);
	g_free (family);
	g_free (weight);
	g_free (size);
	g_free (prov_str);
}

/* FIXME this is an issue for introspection */
/**
 * lapiz_view_set_font:
 * @view: a #LapizView
 * @def: whether to reset the default font
 * @font_name: the name of the font to use
 *
 * If @def is #TRUE, resets the font of the @view to the default font
 * otherwise sets it to @font_name.
 **/
void
lapiz_view_set_font (LapizView   *view,
		     gboolean     def,
		     const gchar *font_name)
{
	PangoFontDescription *font_desc = NULL;
	PangoFontDescription *sys_font_desc = NULL;

	lapiz_debug (DEBUG_VIEW);

	g_return_if_fail (LAPIZ_IS_VIEW (view));

	if (def)
	{
		gchar *font;

		font = lapiz_prefs_manager_get_system_font ();
		font_desc = pango_font_description_from_string (font);
		g_free (font);
	}
	else
	{
		g_return_if_fail (font_name != NULL);

		font_desc = pango_font_description_from_string (font_name);
	}

	g_return_if_fail (font_desc != NULL);

	lapiz_override_font ("textview", CTK_WIDGET (view), font_desc);

	sys_font_desc = get_system_font ();
	if (sys_font_desc) {
		lapiz_override_font ("label", CTK_WIDGET (view), sys_font_desc);
		pango_font_description_free (sys_font_desc);
	}

	pango_font_description_free (font_desc);
}

static void
add_search_completion_entry (const gchar *str)
{
	gchar        *text;
	gboolean      valid;
	CtkTreeModel *model;
	CtkTreeIter   iter;

	if (str == NULL)
		return;

	text = lapiz_utils_unescape_search_text (str);

	if (g_utf8_strlen (text, -1) < MIN_SEARCH_COMPLETION_KEY_LEN)
	{
		g_free (text);
		return;
	}

	g_return_if_fail (CTK_IS_TREE_MODEL (search_completion_model));

	model = CTK_TREE_MODEL (search_completion_model);

	/* Get the first iter in the list */
	valid = ctk_tree_model_get_iter_first (model, &iter);

	while (valid)
	{
		/* Walk through the list, reading each row */
     		gchar *str_data;

		ctk_tree_model_get (model,
				    &iter,
                          	    0,
                          	    &str_data,
                          	    -1);

		if (strcmp (text, str_data) == 0)
		{
			g_free (text);
			g_free (str_data);
			ctk_list_store_move_after (CTK_LIST_STORE (model),
						   &iter,
						   NULL);

			return;
		}

		g_free (str_data);

		valid = ctk_tree_model_iter_next (model, &iter);
    	}

	ctk_list_store_prepend (CTK_LIST_STORE (model), &iter);
	ctk_list_store_set (CTK_LIST_STORE (model),
			    &iter,
			    0,
			    text,
			    -1);

	g_free (text);
}

static void
set_entry_state (CtkWidget             *entry,
                 LapizSearchEntryState  state)
{
	CtkStyleContext *context = ctk_widget_get_style_context (CTK_WIDGET (entry));

	if (state == LAPIZ_SEARCH_ENTRY_NOT_FOUND)
	{
		ctk_style_context_add_class (context, CTK_STYLE_CLASS_ERROR);
	}
	else /* reset */
	{
		ctk_style_context_remove_class (context, CTK_STYLE_CLASS_ERROR);
	}
}

static gboolean
run_search (LapizView        *view,
            const gchar      *entry_text,
	    gboolean          search_backward,
	    gboolean          wrap_around,
            gboolean          typing)
{
	CtkTextIter    start_iter;
	CtkTextIter    match_start;
	CtkTextIter    match_end;
	gboolean       found = FALSE;
	LapizDocument *doc;

	g_return_val_if_fail (view->priv->search_mode == SEARCH, FALSE);

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (view)));

	start_iter = view->priv->start_search_iter;

	if (*entry_text != '\0')
	{
		if (!search_backward)
		{
			if (!typing)
			{
				/* forward and _NOT_ typing */
				ctk_text_buffer_get_selection_bounds (CTK_TEXT_BUFFER (doc),
							      &start_iter,
							      &match_end);

				ctk_text_iter_order (&match_end, &start_iter);
			}

			/* run search */
			found = lapiz_document_search_forward (doc,
							       &start_iter,
							       NULL,
							       &match_start,
							       &match_end);
		}
		else if (!typing)
		{
			/* backward and not typing */
			ctk_text_buffer_get_selection_bounds (CTK_TEXT_BUFFER (doc),
							      &start_iter,
							      &match_end);

			/* run search */
			found = lapiz_document_search_backward (doc,
							        NULL,
							        &start_iter,
							        &match_start,
							        &match_end);
		}
		else
		{
			/* backward (while typing) */
			g_return_val_if_reached (FALSE);

		}

		if (!found && wrap_around)
		{
			if (!search_backward)
				found = lapiz_document_search_forward (doc,
								       NULL,
								       NULL, /* FIXME: set the end_inter */
								       &match_start,
								       &match_end);
			else
				found = lapiz_document_search_backward (doc,
								        NULL, /* FIXME: set the start_inter */
								        NULL,
								        &match_start,
								        &match_end);
		}
	}
	else
	{
		ctk_text_buffer_get_selection_bounds (CTK_TEXT_BUFFER (doc),
						      &start_iter,
						      NULL);
	}

	if (found)
	{
		ctk_text_buffer_place_cursor (CTK_TEXT_BUFFER (doc),
					&match_start);

		ctk_text_buffer_move_mark_by_name (CTK_TEXT_BUFFER (doc),
					"selection_bound", &match_end);
	}
	else
	{
		if (typing)
		{
			ctk_text_buffer_place_cursor (CTK_TEXT_BUFFER (doc),
						      &view->priv->start_search_iter);
		}
	}

	if (found || (*entry_text == '\0'))
	{
		lapiz_view_scroll_to_cursor (view);

		set_entry_state (view->priv->search_entry,
		                 LAPIZ_SEARCH_ENTRY_NORMAL);
	}
	else
	{
		set_entry_state (view->priv->search_entry,
		                 LAPIZ_SEARCH_ENTRY_NOT_FOUND);
	}

	return found;
}

/* Cut and paste from ctkwindow.c */
static void
send_focus_change (CtkWidget *widget,
		   gboolean   in)
{
	GdkEvent *fevent = cdk_event_new (GDK_FOCUS_CHANGE);

	g_object_ref (widget);

	fevent->focus_change.type = GDK_FOCUS_CHANGE;
	fevent->focus_change.window = g_object_ref (ctk_widget_get_window (widget));
	fevent->focus_change.in = in;

	ctk_widget_event (widget, fevent);

	g_object_notify (G_OBJECT (widget), "has-focus");

	g_object_unref (widget);
	cdk_event_free (fevent);
}

static void
hide_search_window (LapizView *view, gboolean cancel)
{
	if (view->priv->disable_popdown)
		return;

	if (view->priv->search_entry_changed_id != 0)
	{
		g_signal_handler_disconnect (view->priv->search_entry,
					     view->priv->search_entry_changed_id);
		view->priv->search_entry_changed_id = 0;
    	}

	if (view->priv->typeselect_flush_timeout != 0)
	{
		g_source_remove (view->priv->typeselect_flush_timeout);
		view->priv->typeselect_flush_timeout = 0;
	}

	/* send focus-in event */
	send_focus_change (CTK_WIDGET (view->priv->search_entry), FALSE);
	ctk_text_view_set_cursor_visible (CTK_TEXT_VIEW (view), TRUE);
	ctk_widget_hide (view->priv->search_window);

	if (cancel)
	{
		CtkTextBuffer *buffer;

		buffer = CTK_TEXT_BUFFER (ctk_text_view_get_buffer (CTK_TEXT_VIEW (view)));
		ctk_text_buffer_place_cursor (buffer, &view->priv->start_search_iter);

		lapiz_view_scroll_to_cursor (view);
	}

	/* make sure a focus event is sent for the edit area */
	send_focus_change (CTK_WIDGET (view), TRUE);
}

static gboolean
search_entry_flush_timeout (LapizView *view)
{
  	view->priv->typeselect_flush_timeout = 0;
	hide_search_window (view, FALSE);

	return FALSE;
}

static void
update_search_window_position (LapizView *view)
{
	gint x, y;
	gint view_x, view_y;
	GdkWindow *view_window = ctk_widget_get_window (CTK_WIDGET (view));

	ctk_widget_realize (view->priv->search_window);

	cdk_window_get_origin (view_window, &view_x, &view_y);

	x = MAX (12, view_x + 12);
	y = MAX (12, view_y - 12);

	ctk_window_move (CTK_WINDOW (view->priv->search_window), x, y);
}

static gboolean
search_window_delete_event (CtkWidget   *widget,
			    GdkEventAny *event,
			    LapizView   *view)
{
	hide_search_window (view, FALSE);

	return TRUE;
}

static gboolean
search_window_button_press_event (CtkWidget      *widget,
				  GdkEventButton *event,
				  LapizView      *view)
{
	hide_search_window (view, FALSE);

	ctk_propagate_event (CTK_WIDGET (view), (GdkEvent *)event);

	return FALSE;
}

static void
search_again (LapizView *view,
	      gboolean   search_backward)
{
	const gchar *entry_text;

	g_return_if_fail (view->priv->search_mode == SEARCH);

	/* SEARCH mode */
	/* renew the flush timeout */
	if (view->priv->typeselect_flush_timeout != 0)
	{
		g_source_remove (view->priv->typeselect_flush_timeout);
		view->priv->typeselect_flush_timeout =
			g_timeout_add (LAPIZ_VIEW_SEARCH_DIALOG_TIMEOUT,
		       		       (GSourceFunc)search_entry_flush_timeout,
		       		       view);
	}

	entry_text = ctk_entry_get_text (CTK_ENTRY (view->priv->search_entry));

	add_search_completion_entry (entry_text);

	run_search (view,
		    entry_text,
		    search_backward,
		    view->priv->wrap_around,
		    FALSE);
}

static gboolean
search_window_scroll_event (CtkWidget      *widget,
			    GdkEventScroll *event,
			    LapizView      *view)
{
	gboolean retval = FALSE;

	if (view->priv->search_mode == GOTO_LINE)
		return retval;

	/* SEARCH mode */
	if (event->direction == GDK_SCROLL_UP)
	{
		search_again (view, TRUE);
		retval = TRUE;
	}
	else if (event->direction == GDK_SCROLL_DOWN)
	{
      		search_again (view, FALSE);
      		retval = TRUE;
	}

	return retval;
}

static gboolean
search_window_key_press_event (CtkWidget   *widget,
			       GdkEventKey *event,
			       LapizView   *view)
{
	gboolean retval = FALSE;
	guint modifiers;

	modifiers = ctk_accelerator_get_default_mod_mask ();

	/* Close window */
	if (event->keyval == GDK_KEY_Tab)
	{
		hide_search_window (view, FALSE);
		retval = TRUE;
	}

	/* Close window and cancel the search */
	if (event->keyval == GDK_KEY_Escape)
	{
		if (view->priv->search_mode == SEARCH)
		{
			LapizDocument *doc;

			/* restore document search so that Find Next does the right thing */
			doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (view)));
			lapiz_document_set_search_text (doc,
							view->priv->old_search_text,
							view->priv->old_search_flags);

		}

		hide_search_window (view, TRUE);
		retval = TRUE;
	}

	if (view->priv->search_mode == GOTO_LINE)
		return retval;

	/* SEARCH mode */

	/* select previous matching iter */
	if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up)
	{
		search_again (view, TRUE);
		retval = TRUE;
	}

	if (((event->state & modifiers) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) &&
	    (event->keyval == GDK_KEY_g || event->keyval == GDK_KEY_G))
	{
		search_again (view, TRUE);
		retval = TRUE;
	}

	/* select next matching iter */
	if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down)
	{
		search_again (view, FALSE);
		retval = TRUE;
	}

	if (((event->state & modifiers) == GDK_CONTROL_MASK) &&
	    (event->keyval == GDK_KEY_g || event->keyval == GDK_KEY_G))
	{
		search_again (view, FALSE);
		retval = TRUE;
	}

	return retval;
}

static void
search_entry_activate (CtkEntry  *entry,
		       LapizView *view)
{
	hide_search_window (view, FALSE);
}

static void
wrap_around_menu_item_toggled (CtkCheckMenuItem *checkmenuitem,
			       LapizView        *view)
{
	view->priv->wrap_around = ctk_check_menu_item_get_active (checkmenuitem);
}

static void
match_entire_word_menu_item_toggled (CtkCheckMenuItem *checkmenuitem,
				     LapizView        *view)
{
	LAPIZ_SEARCH_SET_ENTIRE_WORD (view->priv->search_flags,
				      ctk_check_menu_item_get_active (checkmenuitem));
}

static void
match_case_menu_item_toggled (CtkCheckMenuItem *checkmenuitem,
			      LapizView        *view)
{
	LAPIZ_SEARCH_SET_CASE_SENSITIVE (view->priv->search_flags,
					 ctk_check_menu_item_get_active (checkmenuitem));
}

static void
parse_escapes_menu_item_toggled (CtkCheckMenuItem *checkmenuitem,
			         LapizView        *view)
{
	LAPIZ_SEARCH_SET_PARSE_ESCAPES (view->priv->search_flags,
					ctk_check_menu_item_get_active (checkmenuitem));
}

static gboolean
real_search_enable_popdown (gpointer data)
{
	LapizView *view = (LapizView *)data;

	view->priv->disable_popdown = FALSE;

	return FALSE;
}

static void
search_enable_popdown (CtkWidget *widget,
		       LapizView *view)
{
	g_timeout_add (200, real_search_enable_popdown, view);

	/* renew the flush timeout */
	if (view->priv->typeselect_flush_timeout != 0)
		g_source_remove (view->priv->typeselect_flush_timeout);

	view->priv->typeselect_flush_timeout =
		g_timeout_add (LAPIZ_VIEW_SEARCH_DIALOG_TIMEOUT,
	       		       (GSourceFunc)search_entry_flush_timeout,
	       		       view);
}

static void
search_entry_populate_popup (CtkEntry  *entry,
			     CtkMenu   *menu,
			     LapizView *view)
{
	CtkWidget *menu_item;

	view->priv->disable_popdown = TRUE;
	g_signal_connect (menu, "hide",
		    	  G_CALLBACK (search_enable_popdown), view);

	if (view->priv->search_mode == GOTO_LINE)
		return;

	/* separator */
	menu_item = ctk_menu_item_new ();
	ctk_menu_shell_prepend (CTK_MENU_SHELL (menu), menu_item);
	ctk_widget_show (menu_item);

	/* create "Wrap Around" menu item. */
	menu_item = ctk_check_menu_item_new_with_mnemonic (_("_Wrap Around"));
	g_signal_connect (G_OBJECT (menu_item), "toggled",
			  G_CALLBACK (wrap_around_menu_item_toggled),
			  view);
	ctk_menu_shell_prepend (CTK_MENU_SHELL (menu), menu_item);
	ctk_check_menu_item_set_active (CTK_CHECK_MENU_ITEM (menu_item),
					view->priv->wrap_around);
	ctk_widget_show (menu_item);

	/* create "Match Entire Word Only" menu item. */
	menu_item = ctk_check_menu_item_new_with_mnemonic (_("Match _Entire Word Only"));
	g_signal_connect (G_OBJECT (menu_item), "toggled",
			  G_CALLBACK (match_entire_word_menu_item_toggled),
			  view);
	ctk_menu_shell_prepend (CTK_MENU_SHELL (menu), menu_item);
	ctk_check_menu_item_set_active (CTK_CHECK_MENU_ITEM (menu_item),
					LAPIZ_SEARCH_IS_ENTIRE_WORD (view->priv->search_flags));
	ctk_widget_show (menu_item);

	/* create "Match Case" menu item. */
	menu_item = ctk_check_menu_item_new_with_mnemonic (_("_Match Case"));
	g_signal_connect (G_OBJECT (menu_item), "toggled",
			  G_CALLBACK (match_case_menu_item_toggled),
			  view);
	ctk_menu_shell_prepend (CTK_MENU_SHELL (menu), menu_item);
	ctk_check_menu_item_set_active (CTK_CHECK_MENU_ITEM (menu_item),
					LAPIZ_SEARCH_IS_CASE_SENSITIVE (view->priv->search_flags));
	ctk_widget_show (menu_item);

	/* create "Parse escapes" menu item. */
	menu_item = ctk_check_menu_item_new_with_mnemonic (_("_Parse escape sequences (e.g. \n)"));
	g_signal_connect (G_OBJECT (menu_item), "toggled",
			  G_CALLBACK (parse_escapes_menu_item_toggled),
			  view);
	ctk_menu_shell_prepend (CTK_MENU_SHELL (menu), menu_item);
	ctk_check_menu_item_set_active (CTK_CHECK_MENU_ITEM (menu_item),
					LAPIZ_SEARCH_IS_PARSE_ESCAPES (view->priv->search_flags));
	ctk_widget_show (menu_item);
}

static void
search_entry_insert_text (CtkEditable *editable,
			  const gchar *text,
			  gint         length,
			  gint        *position,
			  LapizView   *view)
{
	if (view->priv->search_mode == GOTO_LINE)
	{
		gunichar c;
		const gchar *p;
	 	const gchar *end;
	 	const gchar *next;

		p = text;
		end = text + length;

		if (p == end)
			return;

		c = g_utf8_get_char (p);

		if (((c == '-' || c == '+') && *position == 0) ||
		    (c == ':' && *position != 0))
		{
			gchar *s = NULL;

			if (c == ':')
			{
				s = ctk_editable_get_chars (editable, 0, -1);
				s = g_utf8_strchr (s, -1, ':');
			}

			if (s == NULL || s == p)
			{
				next = g_utf8_next_char (p);
				p = next;
			}

			g_free (s);
		}

		while (p != end)
		{
			next = g_utf8_next_char (p);

			c = g_utf8_get_char (p);

			if (!g_unichar_isdigit (c)) {
				g_signal_stop_emission_by_name (editable, "insert_text");
				ctk_widget_error_bell (view->priv->search_entry);
				break;
			}

			p = next;
		}
	}
	else
	{
		/* SEARCH mode */
		static gboolean  insert_text = FALSE;
		gchar           *escaped_text;
		gint             new_len;

		lapiz_debug_message (DEBUG_SEARCH, "Text: %s", text);

		/* To avoid recursive behavior */
		if (insert_text)
			return;

		escaped_text = lapiz_utils_escape_search_text (text);

		lapiz_debug_message (DEBUG_SEARCH, "Escaped Text: %s", escaped_text);

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
}

static void
customize_for_search_mode (LapizView *view)
{
	if (view->priv->search_mode == SEARCH)
	{
		ctk_entry_set_icon_from_icon_name (CTK_ENTRY (view->priv->search_entry),
					       CTK_ENTRY_ICON_PRIMARY,
					       "edit-find");

		ctk_widget_set_tooltip_text (view->priv->search_entry,
					     _("String you want to search for"));
	}
	else
	{
		ctk_entry_set_icon_from_icon_name (CTK_ENTRY (view->priv->search_entry),
					       CTK_ENTRY_ICON_PRIMARY,
					       "go-jump");

		ctk_widget_set_tooltip_text (view->priv->search_entry,
					     _("Line you want to move the cursor to"));
	}
}

static gboolean
completion_func (CtkEntryCompletion *completion,
                 const char         *key,
		 CtkTreeIter        *iter,
		 gpointer            data)
{
	gchar *item = NULL;
	gboolean ret = FALSE;
	CtkTreeModel *model;
	LapizViewPrivate *priv = (LapizViewPrivate *)data;
	const gchar *real_key;

	if (priv->search_mode == GOTO_LINE)
		return FALSE;

	real_key = ctk_entry_get_text (CTK_ENTRY (ctk_entry_completion_get_entry (completion)));

	if (g_utf8_strlen (real_key, -1) <= MIN_SEARCH_COMPLETION_KEY_LEN)
		return FALSE;

	model = ctk_entry_completion_get_model (completion);
	g_return_val_if_fail (ctk_tree_model_get_column_type (model, 0) == G_TYPE_STRING,
			      FALSE);

	ctk_tree_model_get (model,
			    iter,
			    0,
			    &item,
			    -1);

	if (item == NULL)
		return FALSE;

	if (LAPIZ_SEARCH_IS_CASE_SENSITIVE (priv->search_flags))
	{
		if (!strncmp (real_key, item, strlen (real_key)))
			ret = TRUE;
	}
	else
	{
		gchar *normalized_string;
		gchar *case_normalized_string;

		normalized_string = g_utf8_normalize (item, -1, G_NORMALIZE_ALL);
		case_normalized_string = g_utf8_casefold (normalized_string, -1);

      		if (!strncmp (key, case_normalized_string, strlen (key)))
			ret = TRUE;

		g_free (normalized_string);
		g_free (case_normalized_string);

	}

	g_free (item);

	return ret;
}

static void
ensure_search_window (LapizView *view)
{
	CtkWidget          *frame;
	CtkWidget          *vbox;
	CtkWidget          *toplevel;
	CtkEntryCompletion *completion;
	CtkWindowGroup     *group;
	CtkWindowGroup     *search_group;

	toplevel = ctk_widget_get_toplevel (CTK_WIDGET (view));
	group = ctk_window_get_group (CTK_WINDOW (toplevel));
	if (view->priv->search_window != NULL)
		search_group = ctk_window_get_group (CTK_WINDOW (view->priv->search_window));

	if (view->priv->search_window != NULL)
	{
		if (group)
			ctk_window_group_add_window (group,
						     CTK_WINDOW (view->priv->search_window));
		else if (search_group)
	 		ctk_window_group_remove_window (search_group,
					 		CTK_WINDOW (view->priv->search_window));

		customize_for_search_mode (view);

		return;
	}

	view->priv->search_window = ctk_window_new (CTK_WINDOW_POPUP);

	if (group)
		ctk_window_group_add_window (group,
					     CTK_WINDOW (view->priv->search_window));

	ctk_window_set_modal (CTK_WINDOW (view->priv->search_window), TRUE);

	g_signal_connect (view->priv->search_window, "delete_event",
			  G_CALLBACK (search_window_delete_event),
			  view);
	g_signal_connect (view->priv->search_window, "key_press_event",
			  G_CALLBACK (search_window_key_press_event),
			  view);
	g_signal_connect (view->priv->search_window, "button_press_event",
			  G_CALLBACK (search_window_button_press_event),
			  view);
	g_signal_connect (view->priv->search_window, "scroll_event",
			  G_CALLBACK (search_window_scroll_event),
			  view);

	frame = ctk_frame_new (NULL);
	ctk_frame_set_shadow_type (CTK_FRAME (frame), CTK_SHADOW_ETCHED_IN);
	ctk_widget_show (frame);
	ctk_container_add (CTK_CONTAINER (view->priv->search_window), frame);

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
	ctk_widget_show (vbox);
	ctk_container_add (CTK_CONTAINER (frame), vbox);
	ctk_container_set_border_width (CTK_CONTAINER (vbox), 3);

	/* add entry */
	view->priv->search_entry = ctk_entry_new ();
	ctk_widget_show (view->priv->search_entry);

	g_signal_connect (view->priv->search_entry, "populate_popup",
			  G_CALLBACK (search_entry_populate_popup),
			  view);
	g_signal_connect (view->priv->search_entry, "activate",
			  G_CALLBACK (search_entry_activate),
			  view);
	g_signal_connect (view->priv->search_entry,
			  "insert_text",
			  G_CALLBACK (search_entry_insert_text),
			  view);

	ctk_container_add (CTK_CONTAINER (vbox),
			   view->priv->search_entry);

	if (search_completion_model == NULL)
	{
		/* Create a tree model and use it as the completion model */
		search_completion_model = ctk_list_store_new (1, G_TYPE_STRING);
	}

	/* Create the completion object for the search entry */
	completion = ctk_entry_completion_new ();
	ctk_entry_completion_set_model (completion,
					CTK_TREE_MODEL (search_completion_model));

	/* Use model column 0 as the text column */
	ctk_entry_completion_set_text_column (completion, 0);

	ctk_entry_completion_set_minimum_key_length (completion,
						     MIN_SEARCH_COMPLETION_KEY_LEN);

	ctk_entry_completion_set_popup_completion (completion, FALSE);
	ctk_entry_completion_set_inline_completion (completion, TRUE);

	ctk_entry_completion_set_match_func (completion,
					     completion_func,
					     view->priv,
					     NULL);

	/* Assign the completion to the entry */
	ctk_entry_set_completion (CTK_ENTRY (view->priv->search_entry),
				  completion);
	g_object_unref (completion);

	ctk_widget_realize (view->priv->search_entry);

	customize_for_search_mode (view);
}

static gboolean
get_selected_text (CtkTextBuffer *doc, gchar **selected_text, gint *len)
{
	CtkTextIter start, end;

	g_return_val_if_fail (selected_text != NULL, FALSE);
	g_return_val_if_fail (*selected_text == NULL, FALSE);

	if (!ctk_text_buffer_get_selection_bounds (doc, &start, &end))
	{
		if (len != NULL)
			len = 0;

		return FALSE;
	}

	*selected_text = ctk_text_buffer_get_slice (doc, &start, &end, TRUE);

	if (len != NULL)
		*len = g_utf8_strlen (*selected_text, -1);

	return TRUE;
}

static void
init_search_entry (LapizView *view)
{
	CtkTextBuffer *buffer;

	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));

	if (view->priv->search_mode == GOTO_LINE)
	{
		gint   line;
		gchar *line_str;

		line = ctk_text_iter_get_line (&view->priv->start_search_iter);

		line_str = g_strdup_printf ("%d", line + 1);

		ctk_entry_set_text (CTK_ENTRY (view->priv->search_entry),
				    line_str);

		g_free (line_str);

		return;
	}
	else
	{
		/* SEARCH mode */
		gboolean  selection_exists;
		gchar    *find_text = NULL;
		gchar    *old_find_text;
		guint     old_find_flags = 0;
		gint      sel_len = 0;

		old_find_text = lapiz_document_get_search_text (LAPIZ_DOCUMENT (buffer),
								&old_find_flags);
		if (old_find_text != NULL)
		{
			g_free (view->priv->old_search_text);
			view->priv->old_search_text = old_find_text;
			add_search_completion_entry (old_find_text);
		}

		if (old_find_flags != 0)
		{
			view->priv->old_search_flags = old_find_flags;
		}

		selection_exists = get_selected_text (buffer,
						      &find_text,
						      &sel_len);

		if (selection_exists  && (find_text != NULL) && (sel_len <= 160))
		{
			ctk_entry_set_text (CTK_ENTRY (view->priv->search_entry),
					    find_text);
		}
		else
		{
			ctk_entry_set_text (CTK_ENTRY (view->priv->search_entry),
					    "");
		}

		g_free (find_text);
	}
}

static void
search_init (CtkWidget *entry,
	     LapizView *view)
{
	LapizDocument *doc;
	const gchar *entry_text;

	/* renew the flush timeout */
	if (view->priv->typeselect_flush_timeout != 0)
	{
		g_source_remove (view->priv->typeselect_flush_timeout);
		view->priv->typeselect_flush_timeout =
			g_timeout_add (LAPIZ_VIEW_SEARCH_DIALOG_TIMEOUT,
		       		       (GSourceFunc)search_entry_flush_timeout,
		       		       view);
	}

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (view)));

	entry_text = ctk_entry_get_text (CTK_ENTRY (entry));

	if (view->priv->search_mode == SEARCH)
	{
		gchar *search_text;
		guint  search_flags;

		search_text = lapiz_document_get_search_text (doc, &search_flags);

		if ((search_text == NULL) ||
		    (strcmp (search_text, entry_text) != 0) ||
		     search_flags != view->priv->search_flags)
		{
			lapiz_document_set_search_text (doc,
							entry_text,
							view->priv->search_flags);
		}

		g_free (search_text);

		run_search (view,
			    entry_text,
			    FALSE,
			    view->priv->wrap_around,
			    TRUE);
	}
	else
	{
		if (*entry_text != '\0')
		{
			gboolean moved, moved_offset;
			gint line;
			gint offset_line = 0;
			gint line_offset = 0;
			gchar **split_text = NULL;
			const gchar *text;

			split_text = g_strsplit (entry_text, ":", -1);

			if (g_strv_length (split_text) > 1)
			{
				text = split_text[0];
			}
			else
			{
				text = entry_text;
			}

			if (*text == '-')
			{
				gint cur_line = ctk_text_iter_get_line (&view->priv->start_search_iter);

				if (*(text + 1) != '\0')
					offset_line = MAX (atoi (text + 1), 0);

				line = MAX (cur_line - offset_line, 0);
			}
			else if (*entry_text == '+')
			{
				gint cur_line = ctk_text_iter_get_line (&view->priv->start_search_iter);

				if (*(text + 1) != '\0')
					offset_line = MAX (atoi (text + 1), 0);

				line = cur_line + offset_line;
			}
			else
			{
				line = MAX (atoi (text) - 1, 0);
			}

			if (split_text[1] != NULL)
			{
				line_offset = atoi (split_text[1]);
			}

			g_strfreev (split_text);

			moved = lapiz_document_goto_line (doc, line);
			moved_offset = lapiz_document_goto_line_offset (doc, line,
									line_offset);

			lapiz_view_scroll_to_cursor (view);

			if (!moved || !moved_offset)
				set_entry_state (view->priv->search_entry,
				                 LAPIZ_SEARCH_ENTRY_NOT_FOUND);
			else
				set_entry_state (view->priv->search_entry,
				                 LAPIZ_SEARCH_ENTRY_NORMAL);
		}
	}
}

static gboolean
start_interactive_search_real (LapizView *view)
{
	CtkTextBuffer *buffer;

	if ((view->priv->search_window != NULL) &&
	    ctk_widget_get_visible (view->priv->search_window))
		return TRUE;

	if (!ctk_widget_has_focus (CTK_WIDGET (view)))
		return FALSE;

	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));

	if (view->priv->search_mode == SEARCH)
		ctk_text_buffer_get_selection_bounds (buffer, &view->priv->start_search_iter, NULL);
	else
		ctk_text_buffer_get_iter_at_mark (buffer,
						  &view->priv->start_search_iter,
						  ctk_text_buffer_get_insert (buffer));

	ensure_search_window (view);

	/* done, show it */
	update_search_window_position (view);
	ctk_widget_show (view->priv->search_window);

	if (view->priv->search_entry_changed_id == 0)
	{
      		view->priv->search_entry_changed_id =
			g_signal_connect (view->priv->search_entry,
					  "changed",
					  G_CALLBACK (search_init),
					  view);
	}

	init_search_entry (view);

	view->priv->typeselect_flush_timeout =
		g_timeout_add (LAPIZ_VIEW_SEARCH_DIALOG_TIMEOUT,
		   	       (GSourceFunc) search_entry_flush_timeout,
		   	       view);

	ctk_text_view_set_cursor_visible (CTK_TEXT_VIEW (view), FALSE);
	ctk_widget_grab_focus (view->priv->search_entry);

	send_focus_change (view->priv->search_entry, TRUE);

	return TRUE;
}

static gboolean
reset_searched_text (LapizView *view)
{
	LapizDocument *doc;

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (view)));

	lapiz_document_set_search_text (doc, "", LAPIZ_SEARCH_DONT_SET_FLAGS);

	return TRUE;
}

static gboolean
start_interactive_search (LapizView *view)
{
	view->priv->search_mode = SEARCH;

	return start_interactive_search_real (view);
}

static gboolean
start_interactive_goto_line (LapizView *view)
{
	view->priv->search_mode = GOTO_LINE;

	return start_interactive_search_real (view);
}

static gboolean
lapiz_view_draw (CtkWidget      *widget,
                 cairo_t        *cr)
{
	CtkTextView *text_view;
	LapizDocument *doc;
	GdkWindow *window;

	text_view = CTK_TEXT_VIEW (widget);

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (text_view));
	window = ctk_text_view_get_window (text_view, CTK_TEXT_WINDOW_TEXT);
	if (ctk_cairo_should_draw_window (cr, window) &&
	    lapiz_document_get_enable_search_highlighting (doc))
	{
		GdkRectangle visible_rect;
		CtkTextIter iter1, iter2;

		ctk_text_view_get_visible_rect (text_view, &visible_rect);
		ctk_text_view_get_line_at_y (text_view, &iter1,
					     visible_rect.y, NULL);
		ctk_text_view_get_line_at_y (text_view, &iter2,
					     visible_rect.y
					     + visible_rect.height, NULL);
		ctk_text_iter_forward_line (&iter2);

		_lapiz_document_search_region (doc,
					       &iter1,
					       &iter2);
	}

	return CTK_WIDGET_CLASS (lapiz_view_parent_class)->draw (widget, cr);
}

static GdkAtom
drag_get_uri_target (CtkWidget      *widget,
		     GdkDragContext *context)
{
	GdkAtom target;
	CtkTargetList *tl;

	tl = ctk_target_list_new (NULL, 0);
	ctk_target_list_add_uri_targets (tl, 0);

	target = ctk_drag_dest_find_target (widget, context, tl);
	ctk_target_list_unref (tl);

	return target;
}

static gboolean
lapiz_view_drag_motion (CtkWidget      *widget,
			GdkDragContext *context,
			gint            x,
			gint            y,
			guint           timestamp)
{
	gboolean result;

	/* Chain up to allow textview to scroll and position dnd mark, note
	 * that this needs to be checked if ctksourceview or ctktextview
	 * changes drag_motion behaviour */
	result = CTK_WIDGET_CLASS (lapiz_view_parent_class)->drag_motion (widget, context, x, y, timestamp);

	/* If this is a URL, deal with it here */
	if (drag_get_uri_target (widget, context) != GDK_NONE)
	{
		cdk_drag_status (context,
				 cdk_drag_context_get_suggested_action (context),
				 timestamp);
		result = TRUE;
	}

	return result;
}

static void
lapiz_view_drag_data_received (CtkWidget        *widget,
		       	       GdkDragContext   *context,
			       gint              x,
			       gint              y,
			       CtkSelectionData *selection_data,
			       guint             info,
			       guint             timestamp)
{
	gchar **uri_list;

	/* If this is an URL emit DROP_URIS, otherwise chain up the signal */
	if (info == TARGET_URI_LIST)
	{
		uri_list = lapiz_utils_drop_get_uris (selection_data);

		if (uri_list != NULL)
		{
			g_signal_emit (widget, view_signals[DROP_URIS], 0, uri_list);
			g_strfreev (uri_list);

			ctk_drag_finish (context, TRUE, FALSE, timestamp);
		}
	}
	else
	{
		CTK_WIDGET_CLASS (lapiz_view_parent_class)->drag_data_received (widget, context, x, y, selection_data, info, timestamp);
	}
}

static gboolean
lapiz_view_drag_drop (CtkWidget      *widget,
		      GdkDragContext *context,
		      gint            x,
		      gint            y,
		      guint           timestamp)
{
	gboolean result;
	GdkAtom target;

	/* If this is a URL, just get the drag data */
	target = drag_get_uri_target (widget, context);

	if (target != GDK_NONE)
	{
		ctk_drag_get_data (widget, context, target, timestamp);
		result = TRUE;
	}
	else
	{
		/* Chain up */
		result = CTK_WIDGET_CLASS (lapiz_view_parent_class)->drag_drop (widget, context, x, y, timestamp);
	}

	return result;
}

static void
show_line_numbers_toggled (CtkMenu   *menu,
			   LapizView *view)
{
	gboolean show;

	show = ctk_check_menu_item_get_active (CTK_CHECK_MENU_ITEM (menu));

	lapiz_prefs_manager_set_display_line_numbers (show);
}

static CtkWidget *
create_line_numbers_menu (CtkWidget *view)
{
	CtkWidget *menu;
	CtkWidget *item;

	menu = ctk_menu_new ();

	item = ctk_check_menu_item_new_with_mnemonic (_("_Display line numbers"));
	ctk_check_menu_item_set_active (CTK_CHECK_MENU_ITEM (item),
					ctk_source_view_get_show_line_numbers (CTK_SOURCE_VIEW (view)));
	g_signal_connect (item, "toggled",
			  G_CALLBACK (show_line_numbers_toggled), view);
	ctk_menu_shell_append (CTK_MENU_SHELL (menu), item);

	ctk_widget_show_all (menu);

	return menu;
}

static void
show_line_numbers_menu (CtkWidget      *view,
			GdkEventButton *event)
{
	CtkWidget *menu;

	menu = create_line_numbers_menu (view);

	ctk_menu_popup_at_pointer (CTK_MENU (menu), NULL);
}

static gboolean
lapiz_view_button_press_event (CtkWidget *widget, GdkEventButton *event)
{
	static gchar  *primtxt = "";

	gchar *txt_clip = ctk_clipboard_wait_for_text (ctk_clipboard_get (GDK_SELECTION_PRIMARY));

	if (txt_clip)
	{
		primtxt = g_strdup (txt_clip);
		g_free (txt_clip);
	}
	else
		ctk_clipboard_set_text (ctk_clipboard_get (GDK_SELECTION_PRIMARY), primtxt, strlen (primtxt));

	if ((event->type == GDK_BUTTON_PRESS) &&
	    (event->window == ctk_text_view_get_window (CTK_TEXT_VIEW (widget),
						        CTK_TEXT_WINDOW_LEFT)))
	{
		if (event->button == 3)
			show_line_numbers_menu (widget, event);

		return TRUE;
	}

	if ((event->button == 2) || (event->button == 3))
	{
		if (middle_or_right_down)
		{
			middle_or_right_down = FALSE;
			return TRUE;
		}
		else
			middle_or_right_down = TRUE;
	}

	if ((event->type == GDK_2BUTTON_PRESS) && (event->button == 1) &&
	    (event->window == ctk_text_view_get_window (CTK_TEXT_VIEW (widget), CTK_TEXT_WINDOW_TEXT)))
		return TRUE;

	return CTK_WIDGET_CLASS (lapiz_view_parent_class)->button_press_event (widget, event);
}

static gboolean
lapiz_view_button_release_event (CtkWidget *widget, GdkEventButton *event)
{
	if (event->button == 2)
		middle_or_right_down = FALSE;

	return CTK_WIDGET_CLASS (lapiz_view_parent_class)->button_release_event (widget, event);
}

static void
lapiz_view_populate_popup (CtkTextView *text_view, CtkWidget *widget)
{
	middle_or_right_down = FALSE;
}

static void
search_highlight_updated_cb (LapizDocument *doc,
			     CtkTextIter   *start,
			     CtkTextIter   *end,
			     LapizView     *view)
{
	GdkRectangle visible_rect;
	GdkRectangle updated_rect;
	GdkRectangle redraw_rect;
	gint y;
	gint height;
	CtkTextView *text_view;

	text_view = CTK_TEXT_VIEW (view);

	g_return_if_fail (lapiz_document_get_enable_search_highlighting (
				LAPIZ_DOCUMENT (ctk_text_view_get_buffer (text_view))));

	/* get visible area */
	ctk_text_view_get_visible_rect (text_view, &visible_rect);

	/* get updated rectangle */
	ctk_text_view_get_line_yrange (text_view, start, &y, &height);
	updated_rect.y = y;
	ctk_text_view_get_line_yrange (text_view, end, &y, &height);
	updated_rect.height = y + height - updated_rect.y;
	updated_rect.x = visible_rect.x;
	updated_rect.width = visible_rect.width;

	/* intersect both rectangles to see whether we need to queue a redraw */
	if (cdk_rectangle_intersect (&updated_rect, &visible_rect, &redraw_rect))
	{
		GdkRectangle widget_rect;

		ctk_text_view_buffer_to_window_coords (text_view,
						       CTK_TEXT_WINDOW_WIDGET,
						       redraw_rect.x,
						       redraw_rect.y,
						       &widget_rect.x,
						       &widget_rect.y);

		widget_rect.width = redraw_rect.width;
		widget_rect.height = redraw_rect.height;

		ctk_widget_queue_draw_area (CTK_WIDGET (text_view),
					    widget_rect.x,
					    widget_rect.y,
					    widget_rect.width,
					    widget_rect.height);
	}
}

static void
delete_line (CtkTextView *text_view,
	     gint         count)
{
	CtkTextIter start;
	CtkTextIter end;
	CtkTextBuffer *buffer;

	buffer = ctk_text_view_get_buffer (text_view);

	ctk_text_view_reset_im_context (text_view);

	/* If there is a selection delete the selected lines and
	 * ignore count */
	if (ctk_text_buffer_get_selection_bounds (buffer, &start, &end))
	{
		ctk_text_iter_order (&start, &end);

		if (ctk_text_iter_starts_line (&end))
		{
			/* Do no delete the line with the cursor if the cursor
			 * is at the beginning of the line */
			count = 0;
		}
		else
			count = 1;
	}

	ctk_text_iter_set_line_offset (&start, 0);

	if (count > 0)
	{
		ctk_text_iter_forward_lines (&end, count);

		if (ctk_text_iter_is_end (&end))
		{
			if (ctk_text_iter_backward_line (&start) && !ctk_text_iter_ends_line (&start))
				ctk_text_iter_forward_to_line_end (&start);
		}
	}
	else if (count < 0)
	{
		if (!ctk_text_iter_ends_line (&end))
			ctk_text_iter_forward_to_line_end (&end);

		while (count < 0)
		{
			if (!ctk_text_iter_backward_line (&start))
				break;

			++count;
		}

		if (count == 0)
		{
			if (!ctk_text_iter_ends_line (&start))
				ctk_text_iter_forward_to_line_end (&start);
		}
		else
			ctk_text_iter_forward_line (&end);
	}

	if (!ctk_text_iter_equal (&start, &end))
	{
		CtkTextIter cur = start;
		ctk_text_iter_set_line_offset (&cur, 0);

		ctk_text_buffer_begin_user_action (buffer);

		ctk_text_buffer_place_cursor (buffer, &cur);

		ctk_text_buffer_delete_interactive (buffer,
						    &start,
						    &end,
						    ctk_text_view_get_editable (text_view));

		ctk_text_buffer_end_user_action (buffer);

		ctk_text_view_scroll_mark_onscreen (text_view,
						    ctk_text_buffer_get_insert (buffer));
	}
	else
	{
		ctk_widget_error_bell (CTK_WIDGET (text_view));
	}
}

static void
lapiz_view_delete_from_cursor (CtkTextView   *text_view,
			       CtkDeleteType  type,
			       gint           count)
{
	/* We override the standard handler for delete_from_cursor since
	   the CTK_DELETE_PARAGRAPHS case is not implemented as we like (i.e. it
	   does not remove the carriage return in the previous line)
	 */
	switch (type)
	{
		case CTK_DELETE_PARAGRAPHS:
			delete_line (text_view, count);
			break;
		default:
			CTK_TEXT_VIEW_CLASS (lapiz_view_parent_class)->delete_from_cursor(text_view, type, count);
			break;
	}
}
