/*
 * lapiz-window.h
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
 * MERCHANWINDOWILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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

#ifndef __LAPIZ_WINDOW_H__
#define __LAPIZ_WINDOW_H__

#include <gio/gio.h>
#include <ctk/ctk.h>

#include <lapiz/lapiz-tab.h>
#include <lapiz/lapiz-panel.h>
#include <lapiz/lapiz-message-bus.h>

G_BEGIN_DECLS

typedef enum
{
	LAPIZ_WINDOW_STATE_NORMAL		= 0,
	LAPIZ_WINDOW_STATE_SAVING		= 1 << 1,
	LAPIZ_WINDOW_STATE_PRINTING		= 1 << 2,
	LAPIZ_WINDOW_STATE_LOADING		= 1 << 3,
	LAPIZ_WINDOW_STATE_ERROR		= 1 << 4,
	LAPIZ_WINDOW_STATE_SAVING_SESSION	= 1 << 5
} LapizWindowState;

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_WINDOW              (lapiz_window_get_type())
#define LAPIZ_WINDOW(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_WINDOW, LapizWindow))
#define LAPIZ_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_WINDOW, LapizWindowClass))
#define LAPIZ_IS_WINDOW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_WINDOW))
#define LAPIZ_IS_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_WINDOW))
#define LAPIZ_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_WINDOW, LapizWindowClass))

/* Private structure type */
typedef struct _LapizWindowPrivate LapizWindowPrivate;

/*
 * Main object structure
 */
typedef struct _LapizWindow LapizWindow;

struct _LapizWindow
{
	GtkWindow window;

	/*< private > */
	LapizWindowPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizWindowClass LapizWindowClass;

struct _LapizWindowClass
{
	GtkWindowClass parent_class;

	/* Signals */
	void	 (* tab_added)      	(LapizWindow *window,
				     	 LapizTab    *tab);
	void	 (* tab_removed)    	(LapizWindow *window,
				     	 LapizTab    *tab);
	void	 (* tabs_reordered) 	(LapizWindow *window);
	void	 (* active_tab_changed)	(LapizWindow *window,
				     	 LapizTab    *tab);
	void	 (* active_tab_state_changed)
					(LapizWindow *window);
};

/*
 * Public methods
 */
GType 		 lapiz_window_get_type 			(void) G_GNUC_CONST;

LapizTab	*lapiz_window_create_tab		(LapizWindow         *window,
							 gboolean             jump_to);

LapizTab	*lapiz_window_create_tab_from_uri	(LapizWindow         *window,
							 const gchar         *uri,
							 const LapizEncoding *encoding,
							 gint                 line_pos,
							 gboolean             create,
							 gboolean             jump_to);

void		 lapiz_window_close_tab			(LapizWindow         *window,
							 LapizTab            *tab);

void		 lapiz_window_close_all_tabs		(LapizWindow         *window);

void		 lapiz_window_close_tabs		(LapizWindow         *window,
							 const GList         *tabs);

LapizTab	*lapiz_window_get_active_tab		(LapizWindow         *window);

void		 lapiz_window_set_active_tab		(LapizWindow         *window,
							 LapizTab            *tab);

/* Helper functions */
LapizView	*lapiz_window_get_active_view		(LapizWindow         *window);
LapizDocument	*lapiz_window_get_active_document	(LapizWindow         *window);

/* Returns a newly allocated list with all the documents in the window */
GList		*lapiz_window_get_documents		(LapizWindow         *window);

/* Returns a newly allocated list with all the documents that need to be
   saved before closing the window */
GList		*lapiz_window_get_unsaved_documents 	(LapizWindow         *window);

/* Returns a newly allocated list with all the views in the window */
GList		*lapiz_window_get_views			(LapizWindow         *window);

GtkWindowGroup  *lapiz_window_get_group			(LapizWindow         *window);

LapizPanel	*lapiz_window_get_side_panel		(LapizWindow         *window);

LapizPanel	*lapiz_window_get_bottom_panel		(LapizWindow         *window);

GtkWidget	*lapiz_window_get_statusbar		(LapizWindow         *window);

GtkUIManager	*lapiz_window_get_ui_manager		(LapizWindow         *window);

LapizWindowState lapiz_window_get_state 		(LapizWindow         *window);

LapizTab        *lapiz_window_get_tab_from_location	(LapizWindow         *window,
							 GFile               *location);

/* Message bus */
LapizMessageBus	*lapiz_window_get_message_bus		(LapizWindow         *window);

/*
 * Non exported functions
 */
GtkWidget	*_lapiz_window_get_notebook		(LapizWindow         *window);

LapizWindow	*_lapiz_window_move_tab_to_new_window	(LapizWindow         *window,
							 LapizTab            *tab);
gboolean	 _lapiz_window_is_removing_tabs		(LapizWindow         *window);

GFile		*_lapiz_window_get_default_location 	(LapizWindow         *window);

void		 _lapiz_window_set_default_location 	(LapizWindow         *window,
							 GFile               *location);

void		 _lapiz_window_set_saving_session_state	(LapizWindow         *window,
							 gboolean             saving_session);

void		 _lapiz_window_fullscreen		(LapizWindow         *window);

void		 _lapiz_window_unfullscreen		(LapizWindow         *window);

gboolean	 _lapiz_window_is_fullscreen		(LapizWindow         *window);

/* these are in lapiz-window because of screen safety */
void		 _lapiz_recent_add			(LapizWindow	     *window,
							 const gchar         *uri,
							 const gchar         *mime);
void		 _lapiz_recent_remove			(LapizWindow         *window,
							 const gchar         *uri);

G_END_DECLS

#endif  /* __LAPIZ_WINDOW_H__  */
