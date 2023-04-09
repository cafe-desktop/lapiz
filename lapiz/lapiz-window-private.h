/*
 * lapiz-window-private.h
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

#ifndef __LAPIZ_WINDOW_PRIVATE_H__
#define __LAPIZ_WINDOW_PRIVATE_H__

#include <libpeas/peas-extension-set.h>

#include "lapiz/lapiz-window.h"
#include "lapiz-prefs-manager.h"
#include "lapiz-message-bus.h"

G_BEGIN_DECLS

/* WindowPrivate is in a separate .h so that we can access it from lapiz-commands */

struct _LapizWindowPrivate
{
	CtkWidget      *notebook;

	CtkWidget      *side_panel;
	CtkWidget      *bottom_panel;

	CtkWidget      *hpaned;
	CtkWidget      *vpaned;

	CtkWidget      *tab_width_combo;
	CtkWidget      *language_combo;

	LapizMessageBus *message_bus;
	PeasExtensionSet *extensions;

	/* Widgets for fullscreen mode */
	CtkWidget      *fullscreen_controls;
	guint           fullscreen_animation_timeout_id;
	gboolean        fullscreen_animation_enter;

	/* statusbar and context ids for statusbar messages */
	CtkWidget      *statusbar;
	guint           generic_message_cid;
	guint           tip_message_cid;
	guint 		tab_width_id;
	guint 		spaces_instead_of_tabs_id;
	guint 		language_changed_id;

	/* Menus & Toolbars */
	CtkUIManager   *manager;
	CtkActionGroup *action_group;
	CtkActionGroup *always_sensitive_action_group;
	CtkActionGroup *close_action_group;
	CtkActionGroup *quit_action_group;
	CtkActionGroup *panes_action_group;
	CtkActionGroup *languages_action_group;
	CtkActionGroup *documents_list_action_group;
	guint           documents_list_menu_ui_id;
	CtkWidget      *toolbar;
	CtkWidget      *toolbar_recent_menu;
	CtkWidget      *menubar;
	LapizToolbarSetting toolbar_style;

	/* recent files */
	CtkActionGroup *recents_action_group;
	guint           recents_menu_ui_id;
	gulong          recents_handler_id;

	LapizTab       *active_tab;
	gint            num_tabs;

	gint            num_tabs_with_error;

	gint            width;
	gint            height;
	CdkWindowState  window_state;

	gint            side_panel_size;
	gint            bottom_panel_size;

	LapizWindowState state;

	gint            bottom_panel_item_removed_handler_id;

	CtkWindowGroup *window_group;

	GFile          *default_location;

	gboolean        removing_tabs : 1;
	gboolean        dispose_has_run : 1;
};

G_END_DECLS

#endif  /* __LAPIZ_WINDOW_PRIVATE_H__  */
