/*
 * lapiz-window.c
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

#include <time.h>
#include <sys/types.h>
#include <string.h>

#include <cdk/cdk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <ctk/ctk.h>
#include <ctksourceview/ctksource.h>
#include <libbean/bean-activatable.h>
#include <libbean/bean-extension-set.h>

#include "lapiz-ui.h"
#include "lapiz-window.h"
#include "lapiz-window-private.h"
#include "lapiz-app.h"
#include "lapiz-notebook.h"
#include "lapiz-statusbar.h"
#include "lapiz-utils.h"
#include "lapiz-commands.h"
#include "lapiz-debug.h"
#include "lapiz-language-manager.h"
#include "lapiz-prefs-manager-app.h"
#include "lapiz-prefs-manager-private.h"
#include "lapiz-panel.h"
#include "lapiz-documents-panel.h"
#include "lapiz-plugins-engine.h"
#include "lapiz-enum-types.h"
#include "lapiz-dirs.h"
#include "lapiz-status-combo-box.h"

#define LANGUAGE_NONE (const gchar *)"LangNone"
#define LAPIZ_UIFILE "lapiz-ui.xml"
#define TAB_WIDTH_DATA "LapizWindowTabWidthData"
#define LANGUAGE_DATA "LapizWindowLanguageData"
#define FULLSCREEN_ANIMATION_SPEED 4

/* Local variables */
static gboolean cansave = TRUE;

/* Signals */
enum
{
	TAB_ADDED,
	TAB_REMOVED,
	TABS_REORDERED,
	ACTIVE_TAB_CHANGED,
	ACTIVE_TAB_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
	PROP_0,
	PROP_STATE
};

enum
{
	TARGET_URI_LIST = 100
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizWindow, lapiz_window, CTK_TYPE_WINDOW)

static void	recent_manager_changed	(CtkRecentManager *manager,
					 LapizWindow *window);

static void
lapiz_window_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	LapizWindow *window = LAPIZ_WINDOW (object);

	switch (prop_id)
	{
		case PROP_STATE:
			g_value_set_enum (value,
					  lapiz_window_get_state (window));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
save_panes_state (LapizWindow *window)
{
	gint pane_page;

	lapiz_debug (DEBUG_WINDOW);

	if (lapiz_prefs_manager_window_size_can_set ())
		lapiz_prefs_manager_set_window_size (window->priv->width,
						     window->priv->height);

	if (lapiz_prefs_manager_window_state_can_set ())
		lapiz_prefs_manager_set_window_state (window->priv->window_state);

	if ((window->priv->side_panel_size > 0) &&
	    lapiz_prefs_manager_side_panel_size_can_set ())
		lapiz_prefs_manager_set_side_panel_size	(
					window->priv->side_panel_size);

	pane_page = _lapiz_panel_get_active_item_id (LAPIZ_PANEL (window->priv->side_panel));
	if (pane_page != 0 &&
	    lapiz_prefs_manager_side_panel_active_page_can_set ())
		lapiz_prefs_manager_set_side_panel_active_page (pane_page);

	if ((window->priv->bottom_panel_size > 0) &&
	    lapiz_prefs_manager_bottom_panel_size_can_set ())
		lapiz_prefs_manager_set_bottom_panel_size (
					window->priv->bottom_panel_size);

	pane_page = _lapiz_panel_get_active_item_id (LAPIZ_PANEL (window->priv->bottom_panel));
	if (pane_page != 0 &&
	    lapiz_prefs_manager_bottom_panel_active_page_can_set ())
		lapiz_prefs_manager_set_bottom_panel_active_page (pane_page);
}

static void
lapiz_window_dispose (GObject *object)
{
	LapizWindow *window;

	lapiz_debug (DEBUG_WINDOW);

	window = LAPIZ_WINDOW (object);

	/* Stop tracking removal of panes otherwise we always
	 * end up with thinking we had no pane active, since they
	 * should all be removed below */
	if (window->priv->bottom_panel_item_removed_handler_id != 0)
	{
		g_signal_handler_disconnect (window->priv->bottom_panel,
					     window->priv->bottom_panel_item_removed_handler_id);
		window->priv->bottom_panel_item_removed_handler_id = 0;
	}

	/* First of all, force collection so that plugins
	 * really drop some of the references.
	 */
	bean_engine_garbage_collect (PEAS_ENGINE (lapiz_plugins_engine_get_default ()));

	/* save the panes position and make sure to deactivate plugins
	 * for this window, but only once */
	if (!window->priv->dispose_has_run)
	{
		save_panes_state (window);

		/* Note that unreffing the extensions will automatically remove
		   all extensions which in turn will deactivate the extension */
		g_object_unref (window->priv->extensions);

		bean_engine_garbage_collect (PEAS_ENGINE (lapiz_plugins_engine_get_default ()));

		window->priv->dispose_has_run = TRUE;
	}

	if (window->priv->fullscreen_animation_timeout_id != 0)
	{
		g_source_remove (window->priv->fullscreen_animation_timeout_id);
		window->priv->fullscreen_animation_timeout_id = 0;
	}

	if (window->priv->fullscreen_controls != NULL)
	{
		ctk_widget_destroy (window->priv->fullscreen_controls);

		window->priv->fullscreen_controls = NULL;
	}

	if (window->priv->recents_handler_id != 0)
	{
		CtkRecentManager *recent_manager;

		recent_manager =  ctk_recent_manager_get_default ();
		g_signal_handler_disconnect (recent_manager,
					     window->priv->recents_handler_id);
		window->priv->recents_handler_id = 0;
	}

	if (window->priv->manager != NULL)
	{
		g_object_unref (window->priv->manager);
		window->priv->manager = NULL;
	}

	if (window->priv->message_bus != NULL)
	{
		g_object_unref (window->priv->message_bus);
		window->priv->message_bus = NULL;
	}

	if (window->priv->window_group != NULL)
	{
		g_object_unref (window->priv->window_group);
		window->priv->window_group = NULL;
	}

	/* Now that there have broken some reference loops,
	 * force collection again.
	 */
	bean_engine_garbage_collect (PEAS_ENGINE (lapiz_plugins_engine_get_default ()));

	G_OBJECT_CLASS (lapiz_window_parent_class)->dispose (object);
}

static void
lapiz_window_finalize (GObject *object)
{
	LapizWindow *window;

	lapiz_debug (DEBUG_WINDOW);

	window = LAPIZ_WINDOW (object);

	if (window->priv->default_location != NULL)
		g_object_unref (window->priv->default_location);

	G_OBJECT_CLASS (lapiz_window_parent_class)->finalize (object);
}

static gboolean
lapiz_window_window_state_event (CtkWidget           *widget,
				 CdkEventWindowState *event)
{
	LapizWindow *window = LAPIZ_WINDOW (widget);

	window->priv->window_state = event->new_window_state;

	return CTK_WIDGET_CLASS (lapiz_window_parent_class)->window_state_event (widget, event);
}

static gboolean
lapiz_window_configure_event (CtkWidget         *widget,
			      CdkEventConfigure *event)
{
	LapizWindow *window = LAPIZ_WINDOW (widget);

	window->priv->width = event->width;
	window->priv->height = event->height;

	return CTK_WIDGET_CLASS (lapiz_window_parent_class)->configure_event (widget, event);
}

/*
 * CtkWindow catches keybindings for the menu items _before_ passing them to
 * the focused widget. This is unfortunate and means that pressing ctrl+V
 * in an entry on a panel ends up pasting text in the TextView.
 * Here we override CtkWindow's handler to do the same things that it
 * does, but in the opposite order and then we chain up to the grand
 * parent handler, skipping ctk_window_key_press_event.
 */
static gboolean
lapiz_window_key_press_event (CtkWidget   *widget,
			      CdkEventKey *event)
{
	static gpointer grand_parent_class = NULL;
	CtkWindow *window = CTK_WINDOW (widget);
	gboolean handled = FALSE;

	if (event->state & CDK_CONTROL_MASK)
	{
		gchar     *font;
		gchar     *tempsize;
		gint       nsize;

		font = g_settings_get_string (lapiz_prefs_manager->settings, "editor-font");
		tempsize = g_strdup (font);

		g_strreverse (tempsize);
		g_strcanon (tempsize, "1234567890", '\0');
		g_strreverse (tempsize);

		gchar tempfont [strlen (font)];
		strcpy (tempfont, font);
		tempfont [strlen (font) - strlen (tempsize)] = 0;

		sscanf (tempsize, "%d", &nsize);

		if ((event->keyval == CDK_KEY_plus) || (event->keyval == CDK_KEY_KP_Add))
		{
			nsize = nsize + 1;
			sprintf (tempsize, "%d", nsize);

			if (!g_settings_get_boolean (lapiz_prefs_manager->settings, "use-default-font") && (nsize < 73))
			{
				gchar *tmp = g_strconcat (tempfont, tempsize, NULL);
				g_settings_set_string (lapiz_prefs_manager->settings, "editor-font", tmp);
				g_free (tmp);
			}
		}
		else if ((event->keyval == CDK_KEY_minus) || (event->keyval == CDK_KEY_KP_Subtract))
		{
			nsize = nsize - 1;
			sprintf (tempsize, "%d", nsize);

			if (!g_settings_get_boolean (lapiz_prefs_manager->settings, "use-default-font") && (nsize > 5))
			{
				gchar *tmp = g_strconcat (tempfont, tempsize, NULL);
				g_settings_set_string (lapiz_prefs_manager->settings, "editor-font", tmp);
				g_free (tmp);
			}
		}

		if (g_settings_get_boolean (lapiz_prefs_manager->settings, "ctrl-tab-switch-tabs"))
		{
			CtkNotebook *notebook = CTK_NOTEBOOK (_lapiz_window_get_notebook (LAPIZ_WINDOW (window)));

			int pages = ctk_notebook_get_n_pages (notebook);
			int page_num = ctk_notebook_get_current_page (notebook);

			if (event->keyval == CDK_KEY_ISO_Left_Tab)
			{
				if (page_num != 0)
					ctk_notebook_prev_page (notebook);
				else
					ctk_notebook_set_current_page (notebook, (pages - 1));
				handled = TRUE;
			}

			if (event->keyval == CDK_KEY_Tab)
			{
				if (page_num != (pages -1))
					ctk_notebook_next_page (notebook);
				else
					ctk_notebook_set_current_page (notebook, 0);
				handled = TRUE;
			}
		}
		g_free (font);
		g_free (tempsize);
	}

	if (grand_parent_class == NULL)
		grand_parent_class = g_type_class_peek_parent (lapiz_window_parent_class);

	/* handle focus widget key events */
	if (!handled)
		handled = ctk_window_propagate_key_event (window, event);

	/* handle mnemonics and accelerators */
	if (!handled)
		handled = ctk_window_activate_key (window, event);

	/* Chain up, invokes binding set */
	if (!handled)
		handled = CTK_WIDGET_CLASS (grand_parent_class)->key_press_event (widget, event);

	return handled;
}

static void
lapiz_window_tab_removed (LapizWindow *window,
			  LapizTab    *tab)
{
	bean_engine_garbage_collect (PEAS_ENGINE (lapiz_plugins_engine_get_default ()));
}

static void
lapiz_window_class_init (LapizWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);

	klass->tab_removed = lapiz_window_tab_removed;

	object_class->dispose = lapiz_window_dispose;
	object_class->finalize = lapiz_window_finalize;
	object_class->get_property = lapiz_window_get_property;

	widget_class->window_state_event = lapiz_window_window_state_event;
	widget_class->configure_event = lapiz_window_configure_event;
	widget_class->key_press_event = lapiz_window_key_press_event;

	signals[TAB_ADDED] =
		g_signal_new ("tab_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizWindowClass, tab_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      LAPIZ_TYPE_TAB);
	signals[TAB_REMOVED] =
		g_signal_new ("tab_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizWindowClass, tab_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      LAPIZ_TYPE_TAB);
	signals[TABS_REORDERED] =
		g_signal_new ("tabs_reordered",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizWindowClass, tabs_reordered),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[ACTIVE_TAB_CHANGED] =
		g_signal_new ("active_tab_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizWindowClass, active_tab_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      LAPIZ_TYPE_TAB);
	signals[ACTIVE_TAB_STATE_CHANGED] =
		g_signal_new ("active_tab_state_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizWindowClass, active_tab_state_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_flags ("state",
							     "State",
							     "The window's state",
							     LAPIZ_TYPE_WINDOW_STATE,
							     LAPIZ_WINDOW_STATE_NORMAL,
							     G_PARAM_READABLE |
							     G_PARAM_STATIC_STRINGS));
}

static void
menu_item_select_cb (CtkMenuItem *proxy,
		     LapizWindow *window)
{
	CtkAction *action;
	char *message;

	action = ctk_activatable_get_related_action (CTK_ACTIVATABLE (proxy));
	g_return_if_fail (action != NULL);

	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message)
	{
		ctk_statusbar_push (CTK_STATUSBAR (window->priv->statusbar),
				    window->priv->tip_message_cid, message);
		g_free (message);
	}
}

static void
menu_item_deselect_cb (CtkMenuItem *proxy,
                       LapizWindow *window)
{
	ctk_statusbar_pop (CTK_STATUSBAR (window->priv->statusbar),
			   window->priv->tip_message_cid);
}

static void
connect_proxy_cb (CtkUIManager *manager,
                  CtkAction *action,
                  CtkWidget *proxy,
                  LapizWindow *window)
{
	if (CTK_IS_MENU_ITEM (proxy))
	{
		g_signal_connect (proxy, "select",
				  G_CALLBACK (menu_item_select_cb), window);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (menu_item_deselect_cb), window);
	}
}

static void
disconnect_proxy_cb (CtkUIManager *manager,
                     CtkAction *action,
                     CtkWidget *proxy,
                     LapizWindow *window)
{
	if (CTK_IS_MENU_ITEM (proxy))
	{
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_select_cb), window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_deselect_cb), window);
	}
}

static void
apply_toolbar_style (LapizWindow *window,
		     CtkWidget *toolbar)
{
	switch (window->priv->toolbar_style)
	{
		case LAPIZ_TOOLBAR_SYSTEM:
			lapiz_debug_message (DEBUG_WINDOW, "LAPIZ: SYSTEM");
			ctk_toolbar_unset_style (
					CTK_TOOLBAR (toolbar));
			break;

		case LAPIZ_TOOLBAR_ICONS:
			lapiz_debug_message (DEBUG_WINDOW, "LAPIZ: ICONS");
			ctk_toolbar_set_style (
					CTK_TOOLBAR (toolbar),
					CTK_TOOLBAR_ICONS);
			break;

		case LAPIZ_TOOLBAR_ICONS_AND_TEXT:
			lapiz_debug_message (DEBUG_WINDOW, "LAPIZ: ICONS_AND_TEXT");
			ctk_toolbar_set_style (
					CTK_TOOLBAR (toolbar),
					CTK_TOOLBAR_BOTH);
			break;

		case LAPIZ_TOOLBAR_ICONS_BOTH_HORIZ:
			lapiz_debug_message (DEBUG_WINDOW, "LAPIZ: ICONS_BOTH_HORIZ");
			ctk_toolbar_set_style (
					CTK_TOOLBAR (toolbar),
					CTK_TOOLBAR_BOTH_HORIZ);
			break;
	}
}

/* Returns TRUE if toolbar is visible */
static gboolean
set_toolbar_style (LapizWindow *window,
		   LapizWindow *origin)
{
	gboolean visible;
	LapizToolbarSetting style;
	CtkAction *action;

	if (origin == NULL)
		visible = lapiz_prefs_manager_get_toolbar_visible ();
	else
		visible = ctk_widget_get_visible (origin->priv->toolbar);

	/* Set visibility */
	if (visible)
		ctk_widget_show (window->priv->toolbar);
	else
		ctk_widget_hide (window->priv->toolbar);

	action = ctk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewToolbar");

	if (ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action)) != visible)
		ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), visible);

	/* Set style */
	if (origin == NULL)
		style = lapiz_prefs_manager_get_toolbar_buttons_style ();
	else
		style = origin->priv->toolbar_style;

	window->priv->toolbar_style = style;

	apply_toolbar_style (window, window->priv->toolbar);

	return visible;
}

static void
update_next_prev_doc_sensitivity (LapizWindow *window,
				  LapizTab    *tab)
{
	gint	     tab_number;
	CtkNotebook *notebook;
	CtkAction   *action;

	lapiz_debug (DEBUG_WINDOW);

	notebook = CTK_NOTEBOOK (_lapiz_window_get_notebook (window));

	tab_number = ctk_notebook_page_num (notebook, CTK_WIDGET (tab));
	g_return_if_fail (tab_number >= 0);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "DocumentsPreviousDocument");
	ctk_action_set_sensitive (action, tab_number != 0);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "DocumentsNextDocument");
	ctk_action_set_sensitive (action,
				  tab_number < ctk_notebook_get_n_pages (notebook) - 1);
}

static void
update_next_prev_doc_sensitivity_per_window (LapizWindow *window)
{
	LapizTab  *tab;
	CtkAction *action;

	lapiz_debug (DEBUG_WINDOW);

	tab = lapiz_window_get_active_tab (window);
	if (tab != NULL)
	{
		update_next_prev_doc_sensitivity (window, tab);

		return;
	}

	action = ctk_action_group_get_action (window->priv->action_group,
					      "DocumentsPreviousDocument");
	ctk_action_set_sensitive (action, FALSE);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "DocumentsNextDocument");
	ctk_action_set_sensitive (action, FALSE);

}

static void
received_clipboard_contents (CtkClipboard     *clipboard,
			     CtkSelectionData *selection_data,
			     LapizWindow      *window)
{
	gboolean sens;
	CtkAction *action;

	/* getting clipboard contents is async, so we need to
	 * get the current tab and its state */

	if (window->priv->active_tab != NULL)
	{
		LapizTabState state;
		gboolean state_normal;

		state = lapiz_tab_get_state (window->priv->active_tab);
		state_normal = (state == LAPIZ_TAB_STATE_NORMAL);

		sens = state_normal &&
		       ctk_selection_data_targets_include_text (selection_data);
	}
	else
	{
		sens = FALSE;
	}

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditPaste");

	ctk_action_set_sensitive (action, sens);

	g_object_unref (window);
}

static void
set_paste_sensitivity_according_to_clipboard (LapizWindow  *window,
					      CtkClipboard *clipboard)
{
	CdkDisplay *display;

	display = ctk_clipboard_get_display (clipboard);

	if (cdk_display_supports_selection_notification (display))
	{
		ctk_clipboard_request_contents (clipboard,
						cdk_atom_intern_static_string ("TARGETS"),
						(CtkClipboardReceivedFunc) received_clipboard_contents,
						g_object_ref (window));
	}
	else
	{
		CtkAction *action;

		action = ctk_action_group_get_action (window->priv->action_group,
						      "EditPaste");

		/* XFIXES extension not availbale, make
		 * Paste always sensitive */
		ctk_action_set_sensitive (action, TRUE);
	}
}

static void
set_sensitivity_according_to_tab (LapizWindow *window,
				  LapizTab    *tab)
{
	LapizDocument *doc;
	LapizView     *view;
	CtkAction     *action;
	gboolean       b;
	gboolean       state_normal;
	gboolean       editable;
	LapizTabState  state;
	CtkClipboard  *clipboard;
	LapizLockdownMask lockdown;

	g_return_if_fail (LAPIZ_TAB (tab));

	lapiz_debug (DEBUG_WINDOW);

	lockdown = lapiz_app_get_lockdown (lapiz_app_get_default ());

	state = lapiz_tab_get_state (tab);
	state_normal = (state == LAPIZ_TAB_STATE_NORMAL);

	view = lapiz_tab_get_view (tab);
	editable = ctk_text_view_get_editable (CTK_TEXT_VIEW (view));

	doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (view)));

	clipboard = ctk_widget_get_clipboard (CTK_WIDGET (window),
					      CDK_SELECTION_CLIPBOARD);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "FileSave");

	if (state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) {
		ctk_text_buffer_set_modified (CTK_TEXT_BUFFER (doc), TRUE);
	}

	ctk_action_set_sensitive (action,
				  (state_normal ||
				   (state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) ||
				   (state == LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW)) &&
				  !lapiz_document_get_readonly (doc) &&
				  !(lockdown & LAPIZ_LOCKDOWN_SAVE_TO_DISK) &&
				   (cansave) &&
				   (editable));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "FileSaveAs");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   (state == LAPIZ_TAB_STATE_SAVING_ERROR) ||
				   (state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) ||
				   (state == LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW)) &&
				  !(lockdown & LAPIZ_LOCKDOWN_SAVE_TO_DISK));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "FileRevert");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   (state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)) &&
				  !lapiz_document_is_untitled (doc));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "FilePrintPreview");
	ctk_action_set_sensitive (action,
				  state_normal &&
				  !(lockdown & LAPIZ_LOCKDOWN_PRINTING));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "FilePrint");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				  (state == LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW)) &&
				  !(lockdown & LAPIZ_LOCKDOWN_PRINTING));

	action = ctk_action_group_get_action (window->priv->close_action_group,
					      "FileClose");

	ctk_action_set_sensitive (action,
				  (state != LAPIZ_TAB_STATE_CLOSING) &&
				  (state != LAPIZ_TAB_STATE_SAVING) &&
				  (state != LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW) &&
				  (state != LAPIZ_TAB_STATE_PRINTING) &&
				  (state != LAPIZ_TAB_STATE_PRINT_PREVIEWING) &&
				  (state != LAPIZ_TAB_STATE_SAVING_ERROR));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditUndo");
	ctk_action_set_sensitive (action,
				  state_normal &&
				  ctk_source_buffer_can_undo (CTK_SOURCE_BUFFER (doc)));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditRedo");
	ctk_action_set_sensitive (action,
				  state_normal &&
				  ctk_source_buffer_can_redo (CTK_SOURCE_BUFFER (doc)));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditCut");
	ctk_action_set_sensitive (action,
				  state_normal &&
				  editable &&
				  ctk_text_buffer_get_has_selection (CTK_TEXT_BUFFER (doc)));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditCopy");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) &&
				  ctk_text_buffer_get_has_selection (CTK_TEXT_BUFFER (doc)));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditPaste");
	if (state_normal && editable)
	{
		set_paste_sensitivity_according_to_clipboard (window,
							      clipboard);
	}
	else
	{
		ctk_action_set_sensitive (action, FALSE);
	}

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditDelete");
	ctk_action_set_sensitive (action,
				  state_normal &&
				  editable &&
				  ctk_text_buffer_get_has_selection (CTK_TEXT_BUFFER (doc)));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchFind");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchIncrementalSearch");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchReplace");
	ctk_action_set_sensitive (action,
				  state_normal &&
				  editable);

	b = lapiz_document_get_can_search_again (doc);
	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchFindNext");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) && b);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchFindPrevious");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) && b);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchClearHighlight");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) && b);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchGoToLine");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "ViewHighlightMode");
	ctk_action_set_sensitive (action,
				  (state != LAPIZ_TAB_STATE_CLOSING) &&
				  lapiz_prefs_manager_get_enable_syntax_highlighting ());

	update_next_prev_doc_sensitivity (window, tab);

	bean_extension_set_call (window->priv->extensions, "update_state", window);
}

static void
language_toggled (CtkToggleAction *action,
		  LapizWindow     *window)
{
	LapizDocument *doc;
	CtkSourceLanguage *lang;
	const gchar *lang_id;

	if (ctk_toggle_action_get_active (action) == FALSE)
		return;

	doc = lapiz_window_get_active_document (window);
	if (doc == NULL)
		return;

	lang_id = ctk_action_get_name (CTK_ACTION (action));

	if (strcmp (lang_id, LANGUAGE_NONE) == 0)
	{
		/* Normal (no highlighting) */
		lang = NULL;
	}
	else
	{
		lang = ctk_source_language_manager_get_language (
				lapiz_get_language_manager (),
				lang_id);
		if (lang == NULL)
		{
			g_warning ("Could not get language %s\n", lang_id);
		}
	}

	lapiz_document_set_language (doc, lang);
}

static gchar *
escape_section_name (const gchar *name)
{
	gchar *ret;

	ret = g_markup_escape_text (name, -1);

	/* Replace '/' with '-' to avoid problems in xml paths */
	g_strdelimit (ret, "/", '-');

	return ret;
}

static void
create_language_menu_item (CtkSourceLanguage *lang,
			   gint               index,
			   guint              ui_id,
			   LapizWindow       *window)
{
	CtkAction *section_action;
	CtkRadioAction *action;
	CtkAction *normal_action;
	GSList *group;
	const gchar *section;
	gchar *escaped_section;
	const gchar *lang_id;
	const gchar *lang_name;
	gchar *escaped_lang_name;
	gchar *tip;
	gchar *path;

	section = ctk_source_language_get_section (lang);
	escaped_section = escape_section_name (section);

	/* check if the section submenu exists or create it */
	section_action = ctk_action_group_get_action (window->priv->languages_action_group,
						      escaped_section);

	if (section_action == NULL)
	{
		gchar *section_name;

		section_name = lapiz_utils_escape_underscores (section, -1);

		section_action = ctk_action_new (escaped_section,
						 section_name,
						 NULL,
						 NULL);

		g_free (section_name);

		ctk_action_group_add_action (window->priv->languages_action_group,
					     section_action);
		g_object_unref (section_action);

		ctk_ui_manager_add_ui (window->priv->manager,
				       ui_id,
				       "/MenuBar/ViewMenu/ViewHighlightModeMenu/LanguagesMenuPlaceholder",
				       escaped_section,
				       escaped_section,
				       CTK_UI_MANAGER_MENU,
				       FALSE);
	}

	/* now add the language item to the section */
	lang_name = ctk_source_language_get_name (lang);
	lang_id = ctk_source_language_get_id (lang);

	escaped_lang_name = lapiz_utils_escape_underscores (lang_name, -1);

	tip = g_strdup_printf (_("Use %s highlight mode"), lang_name);
	path = g_strdup_printf ("/MenuBar/ViewMenu/ViewHighlightModeMenu/LanguagesMenuPlaceholder/%s",
				escaped_section);

	action = ctk_radio_action_new (lang_id,
				       escaped_lang_name,
				       tip,
				       NULL,
				       index);

	g_free (escaped_lang_name);

	/* Action is added with a NULL accel to make the accel overridable */
	ctk_action_group_add_action_with_accel (window->priv->languages_action_group,
						CTK_ACTION (action),
						NULL);
	g_object_unref (action);

	/* add the action to the same radio group of the "Normal" action */
	normal_action = ctk_action_group_get_action (window->priv->languages_action_group,
						     LANGUAGE_NONE);
	group = ctk_radio_action_get_group (CTK_RADIO_ACTION (normal_action));
	ctk_radio_action_set_group (action, group);

	g_signal_connect (action,
			  "activate",
			  G_CALLBACK (language_toggled),
			  window);

	ctk_ui_manager_add_ui (window->priv->manager,
			       ui_id,
			       path,
			       lang_id,
			       lang_id,
			       CTK_UI_MANAGER_MENUITEM,
			       FALSE);

	g_free (path);
	g_free (tip);
	g_free (escaped_section);
}

static void
create_languages_menu (LapizWindow *window)
{
	CtkRadioAction *action_none;
	GSList *languages;
	GSList *l;
	guint id;
	gint i;

	lapiz_debug (DEBUG_WINDOW);

	/* add the "Plain Text" item before all the others */

	/* Translators: "Plain Text" means that no highlight mode is selected in the
	 * "View->Highlight Mode" submenu and so syntax highlighting is disabled */
	action_none = ctk_radio_action_new (LANGUAGE_NONE, _("Plain Text"),
					    _("Disable syntax highlighting"),
					    NULL,
					    -1);

	ctk_action_group_add_action (window->priv->languages_action_group,
				     CTK_ACTION (action_none));
	g_object_unref (action_none);

	g_signal_connect (action_none,
			  "activate",
			  G_CALLBACK (language_toggled),
			  window);

	id = ctk_ui_manager_new_merge_id (window->priv->manager);

	ctk_ui_manager_add_ui (window->priv->manager,
			       id,
			       "/MenuBar/ViewMenu/ViewHighlightModeMenu/LanguagesMenuPlaceholder",
			       LANGUAGE_NONE,
			       LANGUAGE_NONE,
			       CTK_UI_MANAGER_MENUITEM,
			       TRUE);

	ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action_none), TRUE);

	/* now add all the known languages */
	languages = lapiz_language_manager_list_languages_sorted (
						lapiz_get_language_manager (),
						FALSE);

	for (l = languages, i = 0; l != NULL; l = l->next, ++i)
	{
		create_language_menu_item (l->data,
					   i,
					   id,
					   window);
	}

	g_slist_free (languages);
}

static void
update_languages_menu (LapizWindow *window)
{
	LapizDocument *doc;
	GList *actions;
	GList *l;
	CtkAction *action;
	CtkSourceLanguage *lang;
	const gchar *lang_id;

	doc = lapiz_window_get_active_document (window);
	if (doc == NULL)
		return;

	lang = lapiz_document_get_language (doc);
	if (lang != NULL)
		lang_id = ctk_source_language_get_id (lang);
	else
		lang_id = LANGUAGE_NONE;

	actions = ctk_action_group_list_actions (window->priv->languages_action_group);

	/* prevent recursion */
	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_block_by_func (CTK_ACTION (l->data),
						 G_CALLBACK (language_toggled),
						 window);
	}

	action = ctk_action_group_get_action (window->priv->languages_action_group,
					      lang_id);

	ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), TRUE);

	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_unblock_by_func (CTK_ACTION (l->data),
						   G_CALLBACK (language_toggled),
						   window);
	}

	g_list_free (actions);
}

void
_lapiz_recent_add (LapizWindow *window,
		   const gchar *uri,
		   const gchar *mime)
{
	CtkRecentManager *recent_manager;
	CtkRecentData recent_data;

	static gchar *groups[2] = {
		"lapiz",
		NULL
	};

	recent_manager =  ctk_recent_manager_get_default ();

	recent_data.display_name = NULL;
	recent_data.description = NULL;
	recent_data.mime_type = (gchar *) mime;
	recent_data.app_name = (gchar *) g_get_application_name ();
	recent_data.app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
	recent_data.groups = groups;
	recent_data.is_private = FALSE;

	ctk_recent_manager_add_full (recent_manager,
				     uri,
				     &recent_data);

	g_free (recent_data.app_exec);
}

void
_lapiz_recent_remove (LapizWindow *window,
		      const gchar *uri)
{
	CtkRecentManager *recent_manager;

	recent_manager =  ctk_recent_manager_get_default ();

	ctk_recent_manager_remove_item (recent_manager, uri, NULL);
}

static void
open_recent_file (const gchar *uri,
		  LapizWindow *window)
{
	GSList *uris = NULL;

	uris = g_slist_prepend (uris, (gpointer) uri);

	if (lapiz_commands_load_uris (window, uris, NULL, 0) != 1)
	{
		_lapiz_recent_remove (window, uri);
	}

	g_slist_free (uris);
}

static void
recent_chooser_item_activated (CtkRecentChooser *chooser,
			       LapizWindow      *window)
{
	gchar *uri;

	uri = ctk_recent_chooser_get_current_uri (chooser);

	open_recent_file (uri, window);

	g_free (uri);
}

static void
recents_menu_activate (CtkAction   *action,
		       LapizWindow *window)
{
	CtkRecentInfo *info;
	const gchar *uri;

	info = g_object_get_data (G_OBJECT (action), "ctk-recent-info");
	g_return_if_fail (info != NULL);

	uri = ctk_recent_info_get_uri (info);

	open_recent_file (uri, window);
}

static gint
sort_recents_mru (CtkRecentInfo *a, CtkRecentInfo *b)
{
	return (ctk_recent_info_get_modified (b) - ctk_recent_info_get_modified (a));
}

static void	update_recent_files_menu (LapizWindow *window);

static void
recent_manager_changed (CtkRecentManager *manager,
			LapizWindow      *window)
{
	/* regenerate the menu when the model changes */
	update_recent_files_menu (window);
}

/*
 * Manually construct the inline recents list in the File menu.
 * Hopefully ctk 2.12 will add support for it.
 */
static void
update_recent_files_menu (LapizWindow *window)
{
	LapizWindowPrivate *p = window->priv;
	CtkRecentManager *recent_manager;
	gint max_recents;
	GList *actions, *l, *items;
	GList *filtered_items = NULL;
	gint i;

	lapiz_debug (DEBUG_WINDOW);

	max_recents = lapiz_prefs_manager_get_max_recents ();

	g_return_if_fail (p->recents_action_group != NULL);

	if (p->recents_menu_ui_id != 0)
		ctk_ui_manager_remove_ui (p->manager,
					  p->recents_menu_ui_id);

	actions = ctk_action_group_list_actions (p->recents_action_group);
	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_disconnect_by_func (CTK_ACTION (l->data),
						      G_CALLBACK (recents_menu_activate),
						      window);
 		ctk_action_group_remove_action (p->recents_action_group,
						CTK_ACTION (l->data));
	}
	g_list_free (actions);

	p->recents_menu_ui_id = ctk_ui_manager_new_merge_id (p->manager);

	recent_manager =  ctk_recent_manager_get_default ();
	items = ctk_recent_manager_get_items (recent_manager);

	/* filter */
	for (l = items; l != NULL; l = l->next)
	{
		CtkRecentInfo *info = l->data;

		if (!ctk_recent_info_has_group (info, "lapiz"))
			continue;

		filtered_items = g_list_prepend (filtered_items, info);
	}

	/* sort */
	filtered_items = g_list_sort (filtered_items,
				      (GCompareFunc) sort_recents_mru);

	i = 0;
	for (l = filtered_items; l != NULL; l = l->next)
	{
		gchar *action_name;
		const gchar *display_name;
		gchar *escaped;
		gchar *label;
		gchar *uri;
		gchar *ruri;
		gchar *tip;
		CtkAction *action;
		CtkRecentInfo *info = l->data;

		/* clamp */
		if (i >= max_recents)
			break;

		i++;

		action_name = g_strdup_printf ("recent-info-%d", i);

		display_name = ctk_recent_info_get_display_name (info);
		escaped = lapiz_utils_escape_underscores (display_name, -1);
		if (i >= 10)
			label = g_strdup_printf ("%d.  %s",
						 i,
						 escaped);
		else
			label = g_strdup_printf ("_%d.  %s",
						 i,
						 escaped);
		g_free (escaped);

		/* ctk_recent_info_get_uri_display (info) is buggy and
		 * works only for local files */
		uri = lapiz_utils_uri_for_display (ctk_recent_info_get_uri (info));
		ruri = lapiz_utils_replace_home_dir_with_tilde (uri);
		g_free (uri);

		/* Translators: %s is a URI */
		tip = g_strdup_printf (_("Open '%s'"), ruri);
		g_free (ruri);

		action = ctk_action_new (action_name,
					 label,
					 tip,
					 NULL);

		g_object_set_data_full (G_OBJECT (action),
					"ctk-recent-info",
					ctk_recent_info_ref (info),
					(GDestroyNotify) ctk_recent_info_unref);

		g_signal_connect (action,
				  "activate",
				  G_CALLBACK (recents_menu_activate),
				  window);

		ctk_action_group_add_action (p->recents_action_group,
					     action);
		g_object_unref (action);

		ctk_ui_manager_add_ui (p->manager,
				       p->recents_menu_ui_id,
				       "/MenuBar/FileMenu/FileRecentsPlaceholder",
				       action_name,
				       action_name,
				       CTK_UI_MANAGER_MENUITEM,
				       FALSE);

		g_free (action_name);
		g_free (label);
		g_free (tip);
	}

	g_list_free (filtered_items);

	g_list_foreach (items, (GFunc) ctk_recent_info_unref, NULL);
	g_list_free (items);
}

static void
set_non_homogeneus (CtkWidget *widget, gpointer data)
{
	ctk_tool_item_set_homogeneous (CTK_TOOL_ITEM (widget), FALSE);
}

static void
toolbar_visibility_changed (CtkWidget   *toolbar,
			    LapizWindow *window)
{
	gboolean visible;
	CtkAction *action;

	visible = ctk_widget_get_visible (toolbar);

	if (lapiz_prefs_manager_toolbar_visible_can_set ())
		lapiz_prefs_manager_set_toolbar_visible (visible);

	action = ctk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewToolbar");

	if (ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action)) != visible)
		ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), visible);
}

static CtkWidget *
setup_toolbar_open_button (LapizWindow *window,
			   CtkWidget *toolbar)
{
	CtkRecentManager *recent_manager;
	CtkRecentFilter *filter;
	CtkWidget *toolbar_recent_menu;
	CtkToolItem *open_button;
	CtkAction *action;

	recent_manager = ctk_recent_manager_get_default ();

	/* recent files menu tool button */
	toolbar_recent_menu = ctk_recent_chooser_menu_new_for_manager (recent_manager);

	ctk_recent_chooser_set_local_only (CTK_RECENT_CHOOSER (toolbar_recent_menu),
					   FALSE);
	ctk_recent_chooser_set_sort_type (CTK_RECENT_CHOOSER (toolbar_recent_menu),
					  CTK_RECENT_SORT_MRU);
	ctk_recent_chooser_set_limit (CTK_RECENT_CHOOSER (toolbar_recent_menu),
				      lapiz_prefs_manager_get_max_recents ());

	filter = ctk_recent_filter_new ();
	ctk_recent_filter_add_group (filter, "lapiz");
	ctk_recent_chooser_set_filter (CTK_RECENT_CHOOSER (toolbar_recent_menu),
				       filter);

	g_signal_connect (toolbar_recent_menu,
			  "item_activated",
			  G_CALLBACK (recent_chooser_item_activated),
			  window);

	/* add the custom Open button to the toolbar */
	open_button = ctk_menu_tool_button_new (ctk_image_new_from_icon_name ("document-open",
									      CTK_ICON_SIZE_MENU),
						_("Open a file"));

	ctk_menu_tool_button_set_menu (CTK_MENU_TOOL_BUTTON (open_button),
				       toolbar_recent_menu);

	ctk_menu_tool_button_set_arrow_tooltip_text (CTK_MENU_TOOL_BUTTON (open_button),
						     _("Open a recently used file"));

	action = ctk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "FileOpen");
	g_object_set (action,
		      "is_important", TRUE,
		      "short_label", _("Open"),
		      NULL);
	ctk_activatable_set_related_action (CTK_ACTIVATABLE (open_button),
					    action);

	ctk_toolbar_insert (CTK_TOOLBAR (toolbar),
			    open_button,
			    1);

	return toolbar_recent_menu;
}

static void
create_menu_bar_and_toolbar (LapizWindow *window,
			     CtkWidget   *main_box)
{
	CtkActionGroup *action_group;
	CtkAction *action;
	CtkUIManager *manager;
	CtkRecentManager *recent_manager;
	GError *error = NULL;
	gchar *ui_file;

	lapiz_debug (DEBUG_WINDOW);

	manager = ctk_ui_manager_new ();
	window->priv->manager = manager;

	ctk_window_add_accel_group (CTK_WINDOW (window),
				    ctk_ui_manager_get_accel_group (manager));

	action_group = ctk_action_group_new ("LapizWindowAlwaysSensitiveActions");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      lapiz_always_sensitive_menu_entries,
				      G_N_ELEMENTS (lapiz_always_sensitive_menu_entries),
				      window);
	ctk_action_group_add_toggle_actions (action_group,
					     lapiz_always_sensitive_toggle_menu_entries,
					     G_N_ELEMENTS (lapiz_always_sensitive_toggle_menu_entries),
					     window);

	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->always_sensitive_action_group = action_group;

	action_group = ctk_action_group_new ("LapizWindowActions");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      lapiz_menu_entries,
				      G_N_ELEMENTS (lapiz_menu_entries),
				      window);
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->action_group = action_group;

	/* set short labels to use in the toolbar */
	action = ctk_action_group_get_action (action_group, "FileSave");
	g_object_set (action, "short_label", _("Save"), NULL);
	action = ctk_action_group_get_action (action_group, "FilePrint");
	g_object_set (action, "short_label", _("Print"), NULL);
	action = ctk_action_group_get_action (action_group, "SearchFind");
	g_object_set (action, "short_label", _("Find"), NULL);
	action = ctk_action_group_get_action (action_group, "SearchReplace");
	g_object_set (action, "short_label", _("Replace"), NULL);

	/* set which actions should have priority on the toolbar */
	action = ctk_action_group_get_action (action_group, "FileSave");
	g_object_set (action, "is_important", TRUE, NULL);
	action = ctk_action_group_get_action (action_group, "EditUndo");
	g_object_set (action, "is_important", TRUE, NULL);

	action_group = ctk_action_group_new ("LapizQuitWindowActions");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      lapiz_quit_menu_entries,
				      G_N_ELEMENTS (lapiz_quit_menu_entries),
				      window);

	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->quit_action_group = action_group;

	action_group = ctk_action_group_new ("LapizCloseWindowActions");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
	                              lapiz_close_menu_entries,
	                              G_N_ELEMENTS (lapiz_close_menu_entries),
	                              window);

	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->close_action_group = action_group;

	action_group = ctk_action_group_new ("LapizWindowPanesActions");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_toggle_actions (action_group,
					     lapiz_panes_toggle_menu_entries,
					     G_N_ELEMENTS (lapiz_panes_toggle_menu_entries),
					     window);

	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->panes_action_group = action_group;

	/* now load the UI definition */
	ui_file = lapiz_dirs_get_ui_file (LAPIZ_UIFILE);
	ctk_ui_manager_add_ui_from_file (manager, ui_file, &error);
	if (error != NULL)
	{
		g_warning ("Could not merge %s: %s", ui_file, error->message);
		g_error_free (error);
	}
	g_free (ui_file);

	/* show tooltips in the statusbar */
	g_signal_connect (manager,
			  "connect_proxy",
			  G_CALLBACK (connect_proxy_cb),
			  window);
	g_signal_connect (manager,
			  "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb),
			  window);

	/* recent files menu */
	action_group = ctk_action_group_new ("RecentFilesActions");
	ctk_action_group_set_translation_domain (action_group, NULL);
	window->priv->recents_action_group = action_group;
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);

	recent_manager = ctk_recent_manager_get_default ();
	window->priv->recents_handler_id = g_signal_connect (recent_manager,
							     "changed",
							     G_CALLBACK (recent_manager_changed),
							     window);
	update_recent_files_menu (window);

	/* languages menu */
	action_group = ctk_action_group_new ("LanguagesActions");
	ctk_action_group_set_translation_domain (action_group, NULL);
	window->priv->languages_action_group = action_group;
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	create_languages_menu (window);

	/* list of open documents menu */
	action_group = ctk_action_group_new ("DocumentsListActions");
	ctk_action_group_set_translation_domain (action_group, NULL);
	window->priv->documents_list_action_group = action_group;
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);

	window->priv->menubar = ctk_ui_manager_get_widget (manager, "/MenuBar");
	ctk_box_pack_start (CTK_BOX (main_box),
			    window->priv->menubar,
			    FALSE,
			    FALSE,
			    0);

	window->priv->toolbar = ctk_ui_manager_get_widget (manager, "/ToolBar");
	ctk_style_context_add_class (ctk_widget_get_style_context (window->priv->toolbar),
		CTK_STYLE_CLASS_PRIMARY_TOOLBAR);
	ctk_box_pack_start (CTK_BOX (main_box),
			    window->priv->toolbar,
			    FALSE,
			    FALSE,
			    0);

	set_toolbar_style (window, NULL);

	window->priv->toolbar_recent_menu = setup_toolbar_open_button (window,
								       window->priv->toolbar);

	ctk_container_foreach (CTK_CONTAINER (window->priv->toolbar),
			       (CtkCallback)set_non_homogeneus,
			       NULL);

	g_signal_connect_after (G_OBJECT (window->priv->toolbar),
				"show",
				G_CALLBACK (toolbar_visibility_changed),
				window);
	g_signal_connect_after (G_OBJECT (window->priv->toolbar),
				"hide",
				G_CALLBACK (toolbar_visibility_changed),
				window);
}

static void
documents_list_menu_activate (CtkToggleAction *action,
			      LapizWindow     *window)
{
	gint n;

	if (ctk_toggle_action_get_active (action) == FALSE)
		return;

	n = ctk_radio_action_get_current_value (CTK_RADIO_ACTION (action));
	ctk_notebook_set_current_page (CTK_NOTEBOOK (window->priv->notebook), n);
}

static gchar *
get_menu_tip_for_tab (LapizTab *tab)
{
	LapizDocument *doc;
	gchar *uri;
	gchar *ruri;
	gchar *tip;

	doc = lapiz_tab_get_document (tab);

	uri = lapiz_document_get_uri_for_display (doc);
	ruri = lapiz_utils_replace_home_dir_with_tilde (uri);
	g_free (uri);

	/* Translators: %s is a URI */
	tip =  g_strdup_printf (_("Activate '%s'"), ruri);
	g_free (ruri);

	return tip;
}

static void
update_documents_list_menu (LapizWindow *window)
{
	LapizWindowPrivate *p = window->priv;
	GList *actions, *l;
	gint n, i;
	guint id;
	GSList *group = NULL;

	lapiz_debug (DEBUG_WINDOW);

	g_return_if_fail (p->documents_list_action_group != NULL);

	if (p->documents_list_menu_ui_id != 0)
		ctk_ui_manager_remove_ui (p->manager,
					  p->documents_list_menu_ui_id);

	actions = ctk_action_group_list_actions (p->documents_list_action_group);
	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_disconnect_by_func (CTK_ACTION (l->data),
						      G_CALLBACK (documents_list_menu_activate),
						      window);
 		ctk_action_group_remove_action (p->documents_list_action_group,
						CTK_ACTION (l->data));
	}
	g_list_free (actions);

	n = ctk_notebook_get_n_pages (CTK_NOTEBOOK (p->notebook));

	id = (n > 0) ? ctk_ui_manager_new_merge_id (p->manager) : 0;

	for (i = 0; i < n; i++)
	{
		CtkWidget *tab;
		CtkRadioAction *action;
		gchar *action_name;
		gchar *tab_name;
		gchar *name;
		gchar *tip;
		gchar *accel;

		tab = ctk_notebook_get_nth_page (CTK_NOTEBOOK (p->notebook), i);

		/* NOTE: the action is associated to the position of the tab in
		 * the notebook not to the tab itself! This is needed to work
		 * around the ctk+ bug #170727: ctk leaves around the accels
		 * of the action. Since the accel depends on the tab position
		 * the problem is worked around, action with the same name always
		 * get the same accel.
		 */
		action_name = g_strdup_printf ("Tab_%d", i);
		tab_name = _lapiz_tab_get_name (LAPIZ_TAB (tab));
		name = lapiz_utils_escape_underscores (tab_name, -1);
		tip =  get_menu_tip_for_tab (LAPIZ_TAB (tab));

		/* alt + 1, 2, 3... 0 to switch to the first ten tabs */
		accel = (i < 10) ? g_strdup_printf ("<alt>%d", (i + 1) % 10) : NULL;

		action = ctk_radio_action_new (action_name,
					       name,
					       tip,
					       NULL,
					       i);

		if (group != NULL)
			ctk_radio_action_set_group (action, group);

		/* note that group changes each time we add an action, so it must be updated */
		group = ctk_radio_action_get_group (action);

		ctk_action_group_add_action_with_accel (p->documents_list_action_group,
							CTK_ACTION (action),
							accel);

		g_signal_connect (action,
				  "activate",
				  G_CALLBACK (documents_list_menu_activate),
				  window);

		ctk_ui_manager_add_ui (p->manager,
				       id,
				       "/MenuBar/DocumentsMenu/DocumentsListPlaceholder",
				       action_name, action_name,
				       CTK_UI_MANAGER_MENUITEM,
				       FALSE);

		if (LAPIZ_TAB (tab) == p->active_tab)
			ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), TRUE);

		g_object_unref (action);

		g_free (action_name);
		g_free (tab_name);
		g_free (name);
		g_free (tip);
		g_free (accel);
	}

	p->documents_list_menu_ui_id = id;
}

/* Returns TRUE if status bar is visible */
static gboolean
set_statusbar_style (LapizWindow *window,
		     LapizWindow *origin)
{
	CtkAction *action;

	gboolean visible;

	if (origin == NULL)
		visible = lapiz_prefs_manager_get_statusbar_visible ();
	else
		visible = ctk_widget_get_visible (origin->priv->statusbar);

	if (visible)
		ctk_widget_show (window->priv->statusbar);
	else
		ctk_widget_hide (window->priv->statusbar);

	action = ctk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewStatusbar");

	if (ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action)) != visible)
		ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), visible);

	return visible;
}

static void
statusbar_visibility_changed (CtkWidget   *statusbar,
			      LapizWindow *window)
{
	gboolean visible;
	CtkAction *action;

	visible = ctk_widget_get_visible (statusbar);

	if (lapiz_prefs_manager_statusbar_visible_can_set ())
		lapiz_prefs_manager_set_statusbar_visible (visible);

	action = ctk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewStatusbar");

	if (ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action)) != visible)
		ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), visible);
}

static void
tab_width_combo_changed (LapizStatusComboBox *combo,
			 CtkMenuItem         *item,
			 LapizWindow         *window)
{
	LapizView *view;
	guint width_data = 0;

	view = lapiz_window_get_active_view (window);

	if (!view)
		return;

	width_data = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), TAB_WIDTH_DATA));

	if (width_data == 0)
		return;

	g_signal_handler_block (view, window->priv->tab_width_id);
	ctk_source_view_set_tab_width (CTK_SOURCE_VIEW (view), width_data);
	g_signal_handler_unblock (view, window->priv->tab_width_id);
}

static void
use_spaces_toggled (CtkCheckMenuItem *item,
		    LapizWindow      *window)
{
	LapizView *view;

	view = lapiz_window_get_active_view (window);

	g_signal_handler_block (view, window->priv->spaces_instead_of_tabs_id);
	ctk_source_view_set_insert_spaces_instead_of_tabs (
			CTK_SOURCE_VIEW (view),
			ctk_check_menu_item_get_active (item));
	g_signal_handler_unblock (view, window->priv->spaces_instead_of_tabs_id);
}

static void
language_combo_changed (LapizStatusComboBox *combo,
			CtkMenuItem         *item,
			LapizWindow         *window)
{
	LapizDocument *doc;
	CtkSourceLanguage *language;

	doc = lapiz_window_get_active_document (window);

	if (!doc)
		return;

	language = CTK_SOURCE_LANGUAGE (g_object_get_data (G_OBJECT (item), LANGUAGE_DATA));

	g_signal_handler_block (doc, window->priv->language_changed_id);
	lapiz_document_set_language (doc, language);
	g_signal_handler_unblock (doc, window->priv->language_changed_id);
}

typedef struct
{
	const gchar *label;
	guint width;
} TabWidthDefinition;

static void
fill_tab_width_combo (LapizWindow *window)
{
	static TabWidthDefinition defs[] = {
		{"2", 2},
		{"4", 4},
		{"8", 8},
		{"", 0}, /* custom size */
		{NULL, 0}
	};

	LapizStatusComboBox *combo = LAPIZ_STATUS_COMBO_BOX (window->priv->tab_width_combo);
	guint i = 0;
	CtkWidget *item;

	while (defs[i].label != NULL)
	{
		item = ctk_menu_item_new_with_label (defs[i].label);
		g_object_set_data (G_OBJECT (item), TAB_WIDTH_DATA, GINT_TO_POINTER (defs[i].width));

		lapiz_status_combo_box_add_item (combo,
						 CTK_MENU_ITEM (item),
						 defs[i].label);

		if (defs[i].width != 0)
			ctk_widget_show (item);

		++i;
	}

	item = ctk_separator_menu_item_new ();
	lapiz_status_combo_box_add_item (combo, CTK_MENU_ITEM (item), NULL);
	ctk_widget_show (item);

	item = ctk_check_menu_item_new_with_label (_("Use Spaces"));
	lapiz_status_combo_box_add_item (combo, CTK_MENU_ITEM (item), NULL);
	ctk_widget_show (item);

	g_signal_connect (item,
			  "toggled",
			  G_CALLBACK (use_spaces_toggled),
			  window);
}

static void
fill_language_combo (LapizWindow *window)
{
	CtkSourceLanguageManager *manager;
	GSList *languages;
	GSList *item;
	CtkWidget *menu_item;
	const gchar *name;

	manager = lapiz_get_language_manager ();
	languages = lapiz_language_manager_list_languages_sorted (manager, FALSE);

	name = _("Plain Text");
	menu_item = ctk_menu_item_new_with_label (name);
	ctk_widget_show (menu_item);

	g_object_set_data (G_OBJECT (menu_item), LANGUAGE_DATA, NULL);
	lapiz_status_combo_box_add_item (LAPIZ_STATUS_COMBO_BOX (window->priv->language_combo),
					 CTK_MENU_ITEM (menu_item),
					 name);

	for (item = languages; item; item = item->next)
	{
		CtkSourceLanguage *lang = CTK_SOURCE_LANGUAGE (item->data);

		name = ctk_source_language_get_name (lang);
		menu_item = ctk_menu_item_new_with_label (name);
		ctk_widget_show (menu_item);

		g_object_set_data_full (G_OBJECT (menu_item),
				        LANGUAGE_DATA,
					g_object_ref (lang),
					(GDestroyNotify)g_object_unref);

		lapiz_status_combo_box_add_item (LAPIZ_STATUS_COMBO_BOX (window->priv->language_combo),
						 CTK_MENU_ITEM (menu_item),
						 name);
	}

	g_slist_free (languages);
}

static void
create_statusbar (LapizWindow *window,
		  CtkWidget   *main_box)
{
	lapiz_debug (DEBUG_WINDOW);

	window->priv->statusbar = lapiz_statusbar_new ();

	window->priv->generic_message_cid = ctk_statusbar_get_context_id
		(CTK_STATUSBAR (window->priv->statusbar), "generic_message");
	window->priv->tip_message_cid = ctk_statusbar_get_context_id
		(CTK_STATUSBAR (window->priv->statusbar), "tip_message");

	ctk_box_pack_end (CTK_BOX (main_box),
			  window->priv->statusbar,
			  FALSE,
			  TRUE,
			  0);

	window->priv->tab_width_combo = lapiz_status_combo_box_new (_("Tab Width"));
	ctk_widget_show (window->priv->tab_width_combo);
	ctk_box_pack_end (CTK_BOX (window->priv->statusbar),
			  window->priv->tab_width_combo,
			  FALSE,
			  TRUE,
			  0);

	fill_tab_width_combo (window);

	g_signal_connect (G_OBJECT (window->priv->tab_width_combo),
			  "changed",
			  G_CALLBACK (tab_width_combo_changed),
			  window);

	window->priv->language_combo = lapiz_status_combo_box_new (NULL);
	ctk_widget_show (window->priv->language_combo);
	ctk_box_pack_end (CTK_BOX (window->priv->statusbar),
			  window->priv->language_combo,
			  FALSE,
			  TRUE,
			  0);

	fill_language_combo (window);

	g_signal_connect (G_OBJECT (window->priv->language_combo),
			  "changed",
			  G_CALLBACK (language_combo_changed),
			  window);

	g_signal_connect_after (G_OBJECT (window->priv->statusbar),
				"show",
				G_CALLBACK (statusbar_visibility_changed),
				window);
	g_signal_connect_after (G_OBJECT (window->priv->statusbar),
				"hide",
				G_CALLBACK (statusbar_visibility_changed),
				window);

	set_statusbar_style (window, NULL);
}

static LapizWindow *
clone_window (LapizWindow *origin)
{
	LapizWindow *window;
	CdkScreen *screen;
	LapizApp  *app;
	gint panel_page;

	lapiz_debug (DEBUG_WINDOW);

	app = lapiz_app_get_default ();

	screen = ctk_window_get_screen (CTK_WINDOW (origin));
	window = lapiz_app_create_window (app, screen);

	if ((origin->priv->window_state & CDK_WINDOW_STATE_MAXIMIZED) != 0)
	{
		gint w, h;

		lapiz_prefs_manager_get_default_window_size (&w, &h);
		ctk_window_set_default_size (CTK_WINDOW (window), w, h);
		ctk_window_maximize (CTK_WINDOW (window));
	}
	else
	{
		ctk_window_set_default_size (CTK_WINDOW (window),
					     origin->priv->width,
					     origin->priv->height);

		ctk_window_unmaximize (CTK_WINDOW (window));
	}

	if ((origin->priv->window_state & CDK_WINDOW_STATE_STICKY ) != 0)
		ctk_window_stick (CTK_WINDOW (window));
	else
		ctk_window_unstick (CTK_WINDOW (window));

	/* set the panes size, the paned position will be set when
	 * they are mapped */
	window->priv->side_panel_size = origin->priv->side_panel_size;
	window->priv->bottom_panel_size = origin->priv->bottom_panel_size;

	panel_page = _lapiz_panel_get_active_item_id (LAPIZ_PANEL (origin->priv->side_panel));
	_lapiz_panel_set_active_item_by_id (LAPIZ_PANEL (window->priv->side_panel),
					    panel_page);

	panel_page = _lapiz_panel_get_active_item_id (LAPIZ_PANEL (origin->priv->bottom_panel));
	_lapiz_panel_set_active_item_by_id (LAPIZ_PANEL (window->priv->bottom_panel),
					    panel_page);

	if (ctk_widget_get_visible (origin->priv->side_panel))
		ctk_widget_show (window->priv->side_panel);
	else
		ctk_widget_hide (window->priv->side_panel);

	if (ctk_widget_get_visible (origin->priv->bottom_panel))
		ctk_widget_show (window->priv->bottom_panel);
	else
		ctk_widget_hide (window->priv->bottom_panel);

	set_statusbar_style (window, origin);
	set_toolbar_style (window, origin);

	return window;
}

static void
update_cursor_position_statusbar (CtkTextBuffer *buffer,
				  LapizWindow   *window)
{
	gint row, col;
	CtkTextIter iter;
	CtkTextIter start;
	guint tab_size;
	LapizView *view;

	lapiz_debug (DEBUG_WINDOW);

 	if (buffer != CTK_TEXT_BUFFER (lapiz_window_get_active_document (window)))
 		return;

 	view = lapiz_window_get_active_view (window);

	ctk_text_buffer_get_iter_at_mark (buffer,
					  &iter,
					  ctk_text_buffer_get_insert (buffer));

	row = ctk_text_iter_get_line (&iter);

	start = iter;
	ctk_text_iter_set_line_offset (&start, 0);
	col = 0;

	tab_size = ctk_source_view_get_tab_width (CTK_SOURCE_VIEW (view));

	while (!ctk_text_iter_equal (&start, &iter))
	{
		/* FIXME: Are we Unicode compliant here? */
		if (ctk_text_iter_get_char (&start) == '\t')

			col += (tab_size - (col  % tab_size));
		else
			++col;

		ctk_text_iter_forward_char (&start);
	}

	lapiz_statusbar_set_cursor_position (
				LAPIZ_STATUSBAR (window->priv->statusbar),
				row + 1,
				col + 1);
}

static void
update_overwrite_mode_statusbar (CtkTextView *view,
				 LapizWindow *window)
{
	if (view != CTK_TEXT_VIEW (lapiz_window_get_active_view (window)))
		return;

	/* Note that we have to use !ctk_text_view_get_overwrite since we
	   are in the in the signal handler of "toggle overwrite" that is
	   G_SIGNAL_RUN_LAST
	*/
	lapiz_statusbar_set_overwrite (
			LAPIZ_STATUSBAR (window->priv->statusbar),
			!ctk_text_view_get_overwrite (view));
}

#define MAX_TITLE_LENGTH 100

static void
set_title (LapizWindow *window)
{
	LapizDocument *doc = NULL;
	gchar *name;
	gchar *dirname = NULL;
	gchar *title = NULL;
	gint len;
	CtkAction *action;

	if (window->priv->active_tab == NULL)
	{
		ctk_window_set_title (CTK_WINDOW (window), "Lapiz");
		return;
	}

	doc = lapiz_tab_get_document (window->priv->active_tab);
	g_return_if_fail (doc != NULL);

	name = lapiz_document_get_short_name_for_display (doc);

	len = g_utf8_strlen (name, -1);

	/* if the name is awfully long, truncate it and be done with it,
	 * otherwise also show the directory (ellipsized if needed)
	 */
	if (len > MAX_TITLE_LENGTH)
	{
		gchar *tmp;

		tmp = lapiz_utils_str_middle_truncate (name,
						       MAX_TITLE_LENGTH);
		g_free (name);
		name = tmp;
	}
	else
	{
		GFile *file;

		file = lapiz_document_get_location (doc);
		if (file != NULL)
		{
			gchar *str;

			str = lapiz_utils_location_get_dirname_for_display (file);
			g_object_unref (file);

			/* use the remaining space for the dir, but use a min of 20 chars
			 * so that we do not end up with a dirname like "(a...b)".
			 * This means that in the worst case when the filename is long 99
			 * we have a title long 99 + 20, but I think it's a rare enough
			 * case to be acceptable. It's justa darn title afterall :)
			 */
			dirname = lapiz_utils_str_middle_truncate (str,
								   MAX (20, MAX_TITLE_LENGTH - len));
			g_free (str);
		}
	}

	if (ctk_text_buffer_get_modified (CTK_TEXT_BUFFER (doc)))
	{
		gchar *tmp_name;

		tmp_name = g_strdup_printf ("*%s", name);
		g_free (name);

		name = tmp_name;
		cansave = TRUE;
	}
	else
		cansave = FALSE;

	if (lapiz_document_get_readonly (doc))
	{
		if (dirname != NULL)
			title = g_strdup_printf ("%s [%s] (%s) - Lapiz",
						 name,
						 _("Read-Only"),
						 dirname);
		else
			title = g_strdup_printf ("%s [%s] - Lapiz",
						 name,
						 _("Read-Only"));
	}
	else
	{
		if (dirname != NULL)
			title = g_strdup_printf ("%s (%s) - Lapiz",
						 name,
						 dirname);
		else
			title = g_strdup_printf ("%s - Lapiz",
						 name);
	}

	action = ctk_action_group_get_action (window->priv->action_group,
					      "FileSave");
	ctk_action_set_sensitive (action, cansave);

	ctk_window_set_title (CTK_WINDOW (window), title);

	g_free (dirname);
	g_free (name);
	g_free (title);
}

#undef MAX_TITLE_LENGTH

static void
set_tab_width_item_blocked (LapizWindow *window,
			    CtkMenuItem *item)
{
	g_signal_handlers_block_by_func (window->priv->tab_width_combo,
					 tab_width_combo_changed,
					 window);

	lapiz_status_combo_box_set_item (LAPIZ_STATUS_COMBO_BOX (window->priv->tab_width_combo),
					 item);

	g_signal_handlers_unblock_by_func (window->priv->tab_width_combo,
					   tab_width_combo_changed,
					   window);
}

static void
spaces_instead_of_tabs_changed (GObject     *object,
		   		GParamSpec  *pspec,
		 		LapizWindow *window)
{
	LapizView *view = LAPIZ_VIEW (object);
	gboolean active = ctk_source_view_get_insert_spaces_instead_of_tabs (
			CTK_SOURCE_VIEW (view));
	GList *children = lapiz_status_combo_box_get_items (
			LAPIZ_STATUS_COMBO_BOX (window->priv->tab_width_combo));
	CtkCheckMenuItem *item;

	item = CTK_CHECK_MENU_ITEM (g_list_last (children)->data);

	ctk_check_menu_item_set_active (item, active);

	g_list_free (children);
}

static void
tab_width_changed (GObject     *object,
		   GParamSpec  *pspec,
		   LapizWindow *window)
{
	GList *items;
	GList *item;
	LapizStatusComboBox *combo = LAPIZ_STATUS_COMBO_BOX (window->priv->tab_width_combo);
	guint new_tab_width;
	gboolean found = FALSE;

	items = lapiz_status_combo_box_get_items (combo);

	new_tab_width = ctk_source_view_get_tab_width (CTK_SOURCE_VIEW (object));

	for (item = items; item; item = item->next)
	{
		guint tab_width = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item->data), TAB_WIDTH_DATA));

		if (tab_width == new_tab_width)
		{
			set_tab_width_item_blocked (window, CTK_MENU_ITEM (item->data));
			found = TRUE;
		}

		if (CTK_IS_SEPARATOR_MENU_ITEM (item->next->data))
		{
			if (!found)
			{
				/* Set for the last item the custom thing */
				gchar *text;

				text = g_strdup_printf ("%u", new_tab_width);
				lapiz_status_combo_box_set_item_text (combo,
								      CTK_MENU_ITEM (item->data),
								      text);

				ctk_label_set_text (CTK_LABEL (ctk_bin_get_child (CTK_BIN (item->data))),
						    text);

				set_tab_width_item_blocked (window, CTK_MENU_ITEM (item->data));
				ctk_widget_show (CTK_WIDGET (item->data));
			}
			else
			{
				ctk_widget_hide (CTK_WIDGET (item->data));
			}

			break;
		}
	}

	g_list_free (items);
}

static void
language_changed (GObject     *object,
		  GParamSpec  *pspec,
		  LapizWindow *window)
{
	GList *items;
	GList *item;
	LapizStatusComboBox *combo = LAPIZ_STATUS_COMBO_BOX (window->priv->language_combo);
	CtkSourceLanguage *new_language;
	const gchar *new_id;

	items = lapiz_status_combo_box_get_items (combo);

	new_language = ctk_source_buffer_get_language (CTK_SOURCE_BUFFER (object));

	if (new_language)
		new_id = ctk_source_language_get_id (new_language);
	else
		new_id = NULL;

	for (item = items; item; item = item->next)
	{
		CtkSourceLanguage *lang = g_object_get_data (G_OBJECT (item->data), LANGUAGE_DATA);

		if ((new_id == NULL && lang == NULL) ||
		    (new_id != NULL && lang != NULL && strcmp (ctk_source_language_get_id (lang),
		    					       new_id) == 0))
		{
			g_signal_handlers_block_by_func (window->priv->language_combo,
							 language_combo_changed,
					 		 window);

			lapiz_status_combo_box_set_item (LAPIZ_STATUS_COMBO_BOX (window->priv->language_combo),
					 		 CTK_MENU_ITEM (item->data));

			g_signal_handlers_unblock_by_func (window->priv->language_combo,
							   language_combo_changed,
					 		   window);
		}
	}

	g_list_free (items);
}

static void
notebook_switch_page (CtkNotebook     *book,
		      CtkWidget       *pg,
		      gint             page_num,
		      LapizWindow     *window)
{
	LapizView *view;
	LapizTab *tab;
	CtkAction *action;
	gchar *action_name;

	/* CHECK: I don't know why but it seems notebook_switch_page is called
	two times every time the user change the active tab */

	tab = LAPIZ_TAB (ctk_notebook_get_nth_page (book, page_num));
	if (tab == window->priv->active_tab)
		return;

	if (window->priv->active_tab)
	{
		if (window->priv->tab_width_id)
		{
			g_signal_handler_disconnect (lapiz_tab_get_view (window->priv->active_tab),
						     window->priv->tab_width_id);

			window->priv->tab_width_id = 0;
		}

		if (window->priv->spaces_instead_of_tabs_id)
		{
			g_signal_handler_disconnect (lapiz_tab_get_view (window->priv->active_tab),
						     window->priv->spaces_instead_of_tabs_id);

			window->priv->spaces_instead_of_tabs_id = 0;
		}
	}

	/* set the active tab */
	window->priv->active_tab = tab;

	set_title (window);
	set_sensitivity_according_to_tab (window, tab);

	/* activate the right item in the documents menu */
	action_name = g_strdup_printf ("Tab_%d", page_num);
	action = ctk_action_group_get_action (window->priv->documents_list_action_group,
					      action_name);

	/* sometimes the action doesn't exist yet, and the proper action
	 * is set active during the documents list menu creation
	 * CHECK: would it be nicer if active_tab was a property and we monitored the notify signal?
	 */
	if (action != NULL)
		ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), TRUE);

	g_free (action_name);

	/* update the syntax menu */
	update_languages_menu (window);

	view = lapiz_tab_get_view (tab);

	/* sync the statusbar */
	update_cursor_position_statusbar (CTK_TEXT_BUFFER (lapiz_tab_get_document (tab)),
					  window);
	lapiz_statusbar_set_overwrite (LAPIZ_STATUSBAR (window->priv->statusbar),
				       ctk_text_view_get_overwrite (CTK_TEXT_VIEW (view)));

	ctk_widget_show (window->priv->tab_width_combo);
	ctk_widget_show (window->priv->language_combo);

	window->priv->tab_width_id = g_signal_connect (view,
						       "notify::tab-width",
						       G_CALLBACK (tab_width_changed),
						       window);
	window->priv->spaces_instead_of_tabs_id = g_signal_connect (view,
								    "notify::insert-spaces-instead-of-tabs",
								    G_CALLBACK (spaces_instead_of_tabs_changed),
								    window);

	window->priv->language_changed_id = g_signal_connect (lapiz_tab_get_document (tab),
							      "notify::language",
							      G_CALLBACK (language_changed),
							      window);

	/* call it for the first time */
	tab_width_changed (G_OBJECT (view), NULL, window);
	spaces_instead_of_tabs_changed (G_OBJECT (view), NULL, window);
	language_changed (G_OBJECT (lapiz_tab_get_document (tab)), NULL, window);

	g_signal_emit (G_OBJECT (window),
		       signals[ACTIVE_TAB_CHANGED],
		       0,
		       window->priv->active_tab);
}

static void
set_sensitivity_according_to_window_state (LapizWindow *window)
{
	CtkAction *action;
	LapizLockdownMask lockdown;

	lockdown = lapiz_app_get_lockdown (lapiz_app_get_default ());

	/* We disable File->Quit/SaveAll/CloseAll while printing to avoid to have two
	   operations (save and print/print preview) that uses the message area at
	   the same time (may be we can remove this limitation in the future) */
	/* We disable File->Quit/CloseAll if state is saving since saving cannot be
	   cancelled (may be we can remove this limitation in the future) */
	ctk_action_group_set_sensitive (window->priv->quit_action_group,
				  !(window->priv->state & LAPIZ_WINDOW_STATE_SAVING) &&
				  !(window->priv->state & LAPIZ_WINDOW_STATE_PRINTING));

	action = ctk_action_group_get_action (window->priv->action_group,
				              "FileCloseAll");
	ctk_action_set_sensitive (action,
				  !(window->priv->state & LAPIZ_WINDOW_STATE_SAVING) &&
				  !(window->priv->state & LAPIZ_WINDOW_STATE_PRINTING));

	action = ctk_action_group_get_action (window->priv->action_group,
				              "FileSaveAll");
	ctk_action_set_sensitive (action,
				  !(window->priv->state & LAPIZ_WINDOW_STATE_PRINTING) &&
				  !(lockdown & LAPIZ_LOCKDOWN_SAVE_TO_DISK));

	action = ctk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "FileNew");
	ctk_action_set_sensitive (action,
				  !(window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION));

	action = ctk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "FileOpen");
	ctk_action_set_sensitive (action,
				  !(window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION));

	ctk_action_group_set_sensitive (window->priv->recents_action_group,
					!(window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION));

	lapiz_notebook_set_close_buttons_sensitive (LAPIZ_NOTEBOOK (window->priv->notebook),
						    !(window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION));

	lapiz_notebook_set_tab_drag_and_drop_enabled (LAPIZ_NOTEBOOK (window->priv->notebook),
						      !(window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION));

	if ((window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION) != 0)
	{
		/* TODO: If we really care, Find could be active
		 * when in SAVING_SESSION state */

		if (ctk_action_group_get_sensitive (window->priv->action_group))
			ctk_action_group_set_sensitive (window->priv->action_group,
							FALSE);
		if (ctk_action_group_get_sensitive (window->priv->quit_action_group))
			ctk_action_group_set_sensitive (window->priv->quit_action_group,
							FALSE);
		if (ctk_action_group_get_sensitive (window->priv->close_action_group))
			ctk_action_group_set_sensitive (window->priv->close_action_group,
							FALSE);
	}
	else
	{
		if (!ctk_action_group_get_sensitive (window->priv->action_group))
			ctk_action_group_set_sensitive (window->priv->action_group,
							window->priv->num_tabs > 0);
		if (!ctk_action_group_get_sensitive (window->priv->quit_action_group))
			ctk_action_group_set_sensitive (window->priv->quit_action_group,
							window->priv->num_tabs > 0);
		if (!ctk_action_group_get_sensitive (window->priv->close_action_group))
		{
			ctk_action_group_set_sensitive (window->priv->close_action_group,
							window->priv->num_tabs > 0);
		}
	}
}

static void
update_tab_autosave (CtkWidget *widget,
		     gpointer   data)
{
	LapizTab *tab = LAPIZ_TAB (widget);
	gboolean *enabled = (gboolean *) data;

	lapiz_tab_set_auto_save_enabled (tab, *enabled);
}

void
_lapiz_window_set_lockdown (LapizWindow       *window,
			    LapizLockdownMask  lockdown)
{
	LapizTab *tab;
	CtkAction *action;
	gboolean autosave;

	/* start/stop autosave in each existing tab */
	autosave = lapiz_prefs_manager_get_auto_save ();
	ctk_container_foreach (CTK_CONTAINER (window->priv->notebook),
			       update_tab_autosave,
			       &autosave);

	/* update menues wrt the current active tab */
	tab = lapiz_window_get_active_tab (window);

	set_sensitivity_according_to_tab (window, tab);

	action = ctk_action_group_get_action (window->priv->action_group,
				              "FileSaveAll");
	ctk_action_set_sensitive (action,
				  !(window->priv->state & LAPIZ_WINDOW_STATE_PRINTING) &&
				  !(lockdown & LAPIZ_LOCKDOWN_SAVE_TO_DISK));

}

static void
analyze_tab_state (LapizTab    *tab,
		   LapizWindow *window)
{
	LapizTabState ts;

	ts = lapiz_tab_get_state (tab);

	switch (ts)
	{
		case LAPIZ_TAB_STATE_LOADING:
		case LAPIZ_TAB_STATE_REVERTING:
			window->priv->state |= LAPIZ_WINDOW_STATE_LOADING;
			break;

		case LAPIZ_TAB_STATE_SAVING:
			window->priv->state |= LAPIZ_WINDOW_STATE_SAVING;
			break;

		case LAPIZ_TAB_STATE_PRINTING:
		case LAPIZ_TAB_STATE_PRINT_PREVIEWING:
			window->priv->state |= LAPIZ_WINDOW_STATE_PRINTING;
			break;

		case LAPIZ_TAB_STATE_LOADING_ERROR:
		case LAPIZ_TAB_STATE_REVERTING_ERROR:
		case LAPIZ_TAB_STATE_SAVING_ERROR:
		case LAPIZ_TAB_STATE_GENERIC_ERROR:
			window->priv->state |= LAPIZ_WINDOW_STATE_ERROR;
			++window->priv->num_tabs_with_error;
		default:
			/* NOP */
			break;
	}
}

static void
update_window_state (LapizWindow *window)
{
	LapizWindowState old_ws;
	gint old_num_of_errors;

	lapiz_debug_message (DEBUG_WINDOW, "Old state: %x", window->priv->state);

	old_ws = window->priv->state;
	old_num_of_errors = window->priv->num_tabs_with_error;

	window->priv->state = old_ws & LAPIZ_WINDOW_STATE_SAVING_SESSION;

	window->priv->num_tabs_with_error = 0;

	ctk_container_foreach (CTK_CONTAINER (window->priv->notebook),
	       		       (CtkCallback)analyze_tab_state,
	       		       window);

	lapiz_debug_message (DEBUG_WINDOW, "New state: %x", window->priv->state);

	if (old_ws != window->priv->state)
	{
		set_sensitivity_according_to_window_state (window);

		lapiz_statusbar_set_window_state (LAPIZ_STATUSBAR (window->priv->statusbar),
						  window->priv->state,
						  window->priv->num_tabs_with_error);

		g_object_notify (G_OBJECT (window), "state");
	}
	else if (old_num_of_errors != window->priv->num_tabs_with_error)
	{
		lapiz_statusbar_set_window_state (LAPIZ_STATUSBAR (window->priv->statusbar),
						  window->priv->state,
						  window->priv->num_tabs_with_error);
	}
}

static void
sync_state (LapizTab    *tab,
	    GParamSpec  *pspec,
	    LapizWindow *window)
{
	lapiz_debug (DEBUG_WINDOW);

	update_window_state (window);

	if (tab != window->priv->active_tab)
		return;

	set_sensitivity_according_to_tab (window, tab);

	g_signal_emit (G_OBJECT (window), signals[ACTIVE_TAB_STATE_CHANGED], 0);
}

static void
sync_name (LapizTab    *tab,
	   GParamSpec  *pspec,
	   LapizWindow *window)
{
	CtkAction *action;
	gchar *action_name;
	gchar *tab_name;
	gchar *escaped_name;
	gchar *tip;
	gint n;
	LapizDocument *doc;

	if (tab == window->priv->active_tab)
	{
		set_title (window);

		doc = lapiz_tab_get_document (tab);
		action = ctk_action_group_get_action (window->priv->action_group,
						      "FileRevert");
		ctk_action_set_sensitive (action,
					  !lapiz_document_is_untitled (doc));
	}

	/* sync the item in the documents list menu */
	n = ctk_notebook_page_num (CTK_NOTEBOOK (window->priv->notebook),
				   CTK_WIDGET (tab));
	action_name = g_strdup_printf ("Tab_%d", n);
	action = ctk_action_group_get_action (window->priv->documents_list_action_group,
					      action_name);
	g_free (action_name);
	g_return_if_fail (action != NULL);

	tab_name = _lapiz_tab_get_name (tab);
	escaped_name = lapiz_utils_escape_underscores (tab_name, -1);
	tip =  get_menu_tip_for_tab (tab);

	g_object_set (action, "label", escaped_name, NULL);
	g_object_set (action, "tooltip", tip, NULL);

	g_free (tab_name);
	g_free (escaped_name);
	g_free (tip);

	bean_extension_set_call (window->priv->extensions, "update_state", window);
}

static LapizWindow *
get_drop_window (CtkWidget *widget)
{
	CtkWidget *target_window;

	target_window = ctk_widget_get_toplevel (widget);
	g_return_val_if_fail (LAPIZ_IS_WINDOW (target_window), NULL);

	if ((LAPIZ_WINDOW(target_window)->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION) != 0)
		return NULL;

	return LAPIZ_WINDOW (target_window);
}

static void
load_uris_from_drop (LapizWindow  *window,
		     gchar       **uri_list)
{
	GSList *uris = NULL;
	gint i;

	if (uri_list == NULL)
		return;

	for (i = 0; uri_list[i] != NULL; ++i)
	{
		uris = g_slist_prepend (uris, uri_list[i]);
	}

	uris = g_slist_reverse (uris);
	lapiz_commands_load_uris (window,
				  uris,
				  NULL,
				  0);

	g_slist_free (uris);
}

/* Handle drops on the LapizWindow */
static void
drag_data_received_cb (CtkWidget        *widget,
		       CdkDragContext   *context,
		       gint              x,
		       gint              y,
		       CtkSelectionData *selection_data,
		       guint             info,
		       guint             timestamp,
		       gpointer          data)
{
	LapizWindow *window;
	gchar **uri_list;

	window = get_drop_window (widget);

	if (window == NULL)
		return;

	if (info == TARGET_URI_LIST)
	{
		uri_list = lapiz_utils_drop_get_uris(selection_data);
		load_uris_from_drop (window, uri_list);
		g_strfreev (uri_list);
	}
}

/* Handle drops on the LapizView */
static void
drop_uris_cb (CtkWidget    *widget,
	      gchar       **uri_list)
{
	LapizWindow *window;

	window = get_drop_window (widget);

	if (window == NULL)
		return;

	load_uris_from_drop (window, uri_list);
}

static void
fullscreen_controls_show (LapizWindow *window)
{
	CdkScreen *screen;
	CdkDisplay *display;
	CdkRectangle fs_rect;
	gint w, h;

	screen = ctk_window_get_screen (CTK_WINDOW (window));
	display = cdk_screen_get_display (screen);

	cdk_monitor_get_geometry (cdk_display_get_monitor_at_window (display,
								     ctk_widget_get_window (CTK_WIDGET (window))),
				  &fs_rect);

	ctk_window_get_size (CTK_WINDOW (window->priv->fullscreen_controls), &w, &h);

	ctk_window_resize (CTK_WINDOW (window->priv->fullscreen_controls),
			   fs_rect.width, h);

	ctk_window_move (CTK_WINDOW (window->priv->fullscreen_controls),
			 fs_rect.x, fs_rect.y - h + 1);

	ctk_widget_show_all (window->priv->fullscreen_controls);
}

static gboolean
run_fullscreen_animation (gpointer data)
{
	LapizWindow *window = LAPIZ_WINDOW (data);
	CdkScreen *screen;
	CdkDisplay *display;
	CdkRectangle fs_rect;
	gint x, y;

	screen = ctk_window_get_screen (CTK_WINDOW (window));
	display = cdk_screen_get_display (screen);

	cdk_monitor_get_geometry (cdk_display_get_monitor_at_window (display,
								     ctk_widget_get_window (CTK_WIDGET (window))),
				  &fs_rect);

	ctk_window_get_position (CTK_WINDOW (window->priv->fullscreen_controls),
				 &x, &y);

	if (window->priv->fullscreen_animation_enter)
	{
		if (y == fs_rect.y)
		{
			window->priv->fullscreen_animation_timeout_id = 0;
			return FALSE;
		}
		else
		{
			ctk_window_move (CTK_WINDOW (window->priv->fullscreen_controls),
					 x, y + 1);
			return TRUE;
		}
	}
	else
	{
		gint w, h;

		ctk_window_get_size (CTK_WINDOW (window->priv->fullscreen_controls),
				     &w, &h);

		if (y == fs_rect.y - h + 1)
		{
			window->priv->fullscreen_animation_timeout_id = 0;
			return FALSE;
		}
		else
		{
			ctk_window_move (CTK_WINDOW (window->priv->fullscreen_controls),
					 x, y - 1);
			return TRUE;
		}
	}
}

static void
show_hide_fullscreen_toolbar (LapizWindow *window,
			      gboolean     show,
			      gint         height)
{
	CtkSettings *settings;
	gboolean enable_animations;

	settings = ctk_widget_get_settings (CTK_WIDGET (window));
	g_object_get (G_OBJECT (settings),
		      "ctk-enable-animations",
		      &enable_animations,
		      NULL);

	if (enable_animations)
	{
		window->priv->fullscreen_animation_enter = show;

		if (window->priv->fullscreen_animation_timeout_id == 0)
		{
			window->priv->fullscreen_animation_timeout_id =
				g_timeout_add (FULLSCREEN_ANIMATION_SPEED,
					       (GSourceFunc) run_fullscreen_animation,
					       window);
		}
	}
	else
	{
		CdkRectangle fs_rect;
		CdkScreen *screen;
		CdkDisplay *display;

		screen = ctk_window_get_screen (CTK_WINDOW (window));
		display = cdk_screen_get_display (screen);

		cdk_monitor_get_geometry (cdk_display_get_monitor_at_window (display,
									     ctk_widget_get_window (CTK_WIDGET (window))),
					  &fs_rect);

		if (show)
			ctk_window_move (CTK_WINDOW (window->priv->fullscreen_controls),
				 fs_rect.x, fs_rect.y);
		else
			ctk_window_move (CTK_WINDOW (window->priv->fullscreen_controls),
					 fs_rect.x, fs_rect.y - height + 1);
	}

}

static gboolean
on_fullscreen_controls_enter_notify_event (CtkWidget        *widget,
					   CdkEventCrossing *event,
					   LapizWindow      *window)
{
	show_hide_fullscreen_toolbar (window, TRUE, 0);

	return FALSE;
}

static gboolean
on_fullscreen_controls_leave_notify_event (CtkWidget        *widget,
					   CdkEventCrossing *event,
					   LapizWindow      *window)
{
	CdkDevice *device;
	gint w, h;
	gint x, y;

	device = cdk_event_get_device ((CdkEvent *)event);

	ctk_window_get_size (CTK_WINDOW (window->priv->fullscreen_controls), &w, &h);
	cdk_device_get_position (device, NULL, &x, &y);

	/* ctk seems to emit leave notify when clicking on tool items,
	 * work around it by checking the coordinates
	 */
	if (y >= h)
	{
		show_hide_fullscreen_toolbar (window, FALSE, h);
	}

	return FALSE;
}

static void
fullscreen_controls_build (LapizWindow *window)
{
	LapizWindowPrivate *priv = window->priv;
	CtkWidget *toolbar;
	CtkAction *action;

	if (priv->fullscreen_controls != NULL)
		return;

	priv->fullscreen_controls = ctk_window_new (CTK_WINDOW_POPUP);

	ctk_window_set_transient_for (CTK_WINDOW (priv->fullscreen_controls),
				      &window->window);

	/* popup toolbar */
	toolbar = ctk_ui_manager_get_widget (priv->manager, "/FullscreenToolBar");
	ctk_container_add (CTK_CONTAINER (priv->fullscreen_controls),
			   toolbar);

	action = ctk_action_group_get_action (priv->always_sensitive_action_group,
					      "LeaveFullscreen");
	g_object_set (action, "is-important", TRUE, NULL);

	setup_toolbar_open_button (window, toolbar);

	ctk_container_foreach (CTK_CONTAINER (toolbar),
			       (CtkCallback)set_non_homogeneus,
			       NULL);

	/* Set the toolbar style */
	ctk_toolbar_set_style (CTK_TOOLBAR (toolbar),
			       CTK_TOOLBAR_BOTH_HORIZ);

	g_signal_connect (priv->fullscreen_controls, "enter-notify-event",
			  G_CALLBACK (on_fullscreen_controls_enter_notify_event),
			  window);
	g_signal_connect (priv->fullscreen_controls, "leave-notify-event",
			  G_CALLBACK (on_fullscreen_controls_leave_notify_event),
			  window);
}

static void
can_search_again (LapizDocument *doc,
		  GParamSpec    *pspec,
		  LapizWindow   *window)
{
	gboolean sensitive;
	CtkAction *action;

	if (doc != lapiz_window_get_active_document (window))
		return;

	sensitive = lapiz_document_get_can_search_again (doc);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchFindNext");
	ctk_action_set_sensitive (action, sensitive);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchFindPrevious");
	ctk_action_set_sensitive (action, sensitive);

	action = ctk_action_group_get_action (window->priv->action_group,
					      "SearchClearHighlight");
	ctk_action_set_sensitive (action, sensitive);
}

static void
can_undo (LapizDocument *doc,
	  GParamSpec    *pspec,
	  LapizWindow   *window)
{
	CtkAction *action;
	gboolean sensitive;

	sensitive = ctk_source_buffer_can_undo (CTK_SOURCE_BUFFER (doc));

	if (doc != lapiz_window_get_active_document (window))
		return;

	action = ctk_action_group_get_action (window->priv->action_group,
					     "EditUndo");
	ctk_action_set_sensitive (action, sensitive);
}

static void
can_redo (LapizDocument *doc,
	  GParamSpec    *pspec,
	  LapizWindow   *window)
{
	CtkAction *action;
	gboolean sensitive;

	sensitive = ctk_source_buffer_can_redo (CTK_SOURCE_BUFFER (doc));

	if (doc != lapiz_window_get_active_document (window))
		return;

	action = ctk_action_group_get_action (window->priv->action_group,
					     "EditRedo");
	ctk_action_set_sensitive (action, sensitive);
}

static void
selection_changed (LapizDocument *doc,
		   GParamSpec    *pspec,
		   LapizWindow   *window)
{
	LapizTab *tab;
	LapizView *view;
	CtkAction *action;
	LapizTabState state;
	gboolean state_normal;
	gboolean editable;

	lapiz_debug (DEBUG_WINDOW);

	if (doc != lapiz_window_get_active_document (window))
		return;

	tab = lapiz_tab_get_from_document (doc);
	state = lapiz_tab_get_state (tab);
	state_normal = (state == LAPIZ_TAB_STATE_NORMAL);

	view = lapiz_tab_get_view (tab);
	editable = ctk_text_view_get_editable (CTK_TEXT_VIEW (view));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditCut");
	ctk_action_set_sensitive (action,
				  state_normal &&
				  editable &&
				  ctk_text_buffer_get_has_selection (CTK_TEXT_BUFFER (doc)));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditCopy");
	ctk_action_set_sensitive (action,
				  (state_normal ||
				   state == LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) &&
				  ctk_text_buffer_get_has_selection (CTK_TEXT_BUFFER (doc)));

	action = ctk_action_group_get_action (window->priv->action_group,
					      "EditDelete");
	ctk_action_set_sensitive (action,
				  state_normal &&
				  editable &&
				  ctk_text_buffer_get_has_selection (CTK_TEXT_BUFFER (doc)));

	bean_extension_set_call (window->priv->extensions, "update_state", window);
}

static void
sync_languages_menu (LapizDocument *doc,
		     GParamSpec    *pspec,
		     LapizWindow   *window)
{
	update_languages_menu (window);
	bean_extension_set_call (window->priv->extensions, "update_state", window);
}

static void
readonly_changed (LapizDocument *doc,
		  GParamSpec    *pspec,
		  LapizWindow   *window)
{
	set_sensitivity_according_to_tab (window, window->priv->active_tab);

	sync_name (window->priv->active_tab, NULL, window);

	bean_extension_set_call (window->priv->extensions, "update_state", window);
}

static void
editable_changed (LapizView  *view,
                  GParamSpec  *arg1,
                  LapizWindow *window)
{
	bean_extension_set_call (window->priv->extensions, "update_state", window);
}

static void
update_sensitivity_according_to_open_tabs (LapizWindow *window)
{
	CtkAction *action;

	/* Set sensitivity */
	ctk_action_group_set_sensitive (window->priv->action_group,
					window->priv->num_tabs != 0);

	action = ctk_action_group_get_action (window->priv->action_group,
					     "DocumentsMoveToNewWindow");
	ctk_action_set_sensitive (action,
				  window->priv->num_tabs > 1);

	ctk_action_group_set_sensitive (window->priv->close_action_group,
	                                window->priv->num_tabs != 0);
}

static void
notebook_tab_added (LapizNotebook *notebook,
		    LapizTab      *tab,
		    LapizWindow   *window)
{
	LapizView *view;
	LapizDocument *doc;

	lapiz_debug (DEBUG_WINDOW);

	g_return_if_fail ((window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION) == 0);

	++window->priv->num_tabs;

	update_sensitivity_according_to_open_tabs (window);

	view = lapiz_tab_get_view (tab);
	doc = lapiz_tab_get_document (tab);

	/* IMPORTANT: remember to disconnect the signal in notebook_tab_removed
	 * if a new signal is connected here */

	g_signal_connect (tab,
			 "notify::name",
			  G_CALLBACK (sync_name),
			  window);
	g_signal_connect (tab,
			 "notify::state",
			  G_CALLBACK (sync_state),
			  window);

	g_signal_connect (doc,
			  "cursor-moved",
			  G_CALLBACK (update_cursor_position_statusbar),
			  window);
	g_signal_connect (doc,
			  "notify::can-search-again",
			  G_CALLBACK (can_search_again),
			  window);
	g_signal_connect (doc,
			  "notify::can-undo",
			  G_CALLBACK (can_undo),
			  window);
	g_signal_connect (doc,
			  "notify::can-redo",
			  G_CALLBACK (can_redo),
			  window);
	g_signal_connect (doc,
			  "notify::has-selection",
			  G_CALLBACK (selection_changed),
			  window);
	g_signal_connect (doc,
			  "notify::language",
			  G_CALLBACK (sync_languages_menu),
			  window);
	g_signal_connect (doc,
			  "notify::read-only",
			  G_CALLBACK (readonly_changed),
			  window);
	g_signal_connect (view,
			  "toggle_overwrite",
			  G_CALLBACK (update_overwrite_mode_statusbar),
			  window);
	g_signal_connect (view,
			  "notify::editable",
			  G_CALLBACK (editable_changed),
			  window);

	update_documents_list_menu (window);

	g_signal_connect (view,
			  "drop_uris",
			  G_CALLBACK (drop_uris_cb),
			  NULL);

	update_window_state (window);

	g_signal_emit (G_OBJECT (window), signals[TAB_ADDED], 0, tab);
}

static void
notebook_tab_removed (LapizNotebook *notebook,
		      LapizTab      *tab,
		      LapizWindow   *window)
{
	LapizView     *view;
	LapizDocument *doc;

	lapiz_debug (DEBUG_WINDOW);

	g_return_if_fail ((window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION) == 0);

	--window->priv->num_tabs;

	view = lapiz_tab_get_view (tab);
	doc = lapiz_tab_get_document (tab);

	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_name),
					      window);
	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_state),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (update_cursor_position_statusbar),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (can_search_again),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (can_undo),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (can_redo),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (selection_changed),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (sync_languages_menu),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (readonly_changed),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (update_overwrite_mode_statusbar),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (editable_changed),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (drop_uris_cb),
					      NULL);

	if (window->priv->tab_width_id && tab == lapiz_window_get_active_tab (window))
	{
		g_signal_handler_disconnect (view, window->priv->tab_width_id);
		window->priv->tab_width_id = 0;
	}

	if (window->priv->spaces_instead_of_tabs_id && tab == lapiz_window_get_active_tab (window))
	{
		g_signal_handler_disconnect (view, window->priv->spaces_instead_of_tabs_id);
		window->priv->spaces_instead_of_tabs_id = 0;
	}

	if (window->priv->language_changed_id && tab == lapiz_window_get_active_tab (window))
	{
		g_signal_handler_disconnect (doc, window->priv->language_changed_id);
		window->priv->language_changed_id = 0;
	}

	g_return_if_fail (window->priv->num_tabs >= 0);
	if (window->priv->num_tabs == 0)
	{
		window->priv->active_tab = NULL;

		set_title (window);

		/* Remove line and col info */
		lapiz_statusbar_set_cursor_position (
				LAPIZ_STATUSBAR (window->priv->statusbar),
				-1,
				-1);

		lapiz_statusbar_clear_overwrite (
				LAPIZ_STATUSBAR (window->priv->statusbar));

		/* hide the combos */
		ctk_widget_hide (window->priv->tab_width_combo);
		ctk_widget_hide (window->priv->language_combo);
	}

	if (!window->priv->removing_tabs)
	{
		update_documents_list_menu (window);
		update_next_prev_doc_sensitivity_per_window (window);
	}
	else
	{
		if (window->priv->num_tabs == 0)
		{
			update_documents_list_menu (window);
			update_next_prev_doc_sensitivity_per_window (window);
		}
	}

	update_sensitivity_according_to_open_tabs (window);

	if (window->priv->num_tabs == 0)
	{
		bean_extension_set_call (window->priv->extensions, "update_state", window);
	}

	update_window_state (window);

	g_signal_emit (G_OBJECT (window), signals[TAB_REMOVED], 0, tab);
}

static void
notebook_tabs_reordered (LapizNotebook *notebook,
			 LapizWindow   *window)
{
	update_documents_list_menu (window);
	update_next_prev_doc_sensitivity_per_window (window);

	g_signal_emit (G_OBJECT (window), signals[TABS_REORDERED], 0);
}

static void
notebook_tab_detached (LapizNotebook *notebook,
		       LapizTab      *tab,
		       LapizWindow   *window)
{
	LapizWindow *new_window;

	new_window = clone_window (window);

	lapiz_notebook_move_tab (notebook,
				 LAPIZ_NOTEBOOK (_lapiz_window_get_notebook (new_window)),
				 tab, 0);

	ctk_window_set_position (CTK_WINDOW (new_window),
				 CTK_WIN_POS_MOUSE);

	ctk_widget_show (CTK_WIDGET (new_window));
}

static void
notebook_tab_close_request (LapizNotebook *notebook,
			    LapizTab      *tab,
			    CtkWindow     *window)
{
	/* Note: we are destroying the tab before the default handler
	 * seems to be ok, but we need to keep an eye on this. */
	_lapiz_cmd_file_close_tab (tab, LAPIZ_WINDOW (window));
}

static gboolean
show_notebook_popup_menu (CtkNotebook    *notebook,
			  LapizWindow    *window,
			  CdkEventButton *event)
{
	CtkWidget *menu;
//	CtkAction *action;

	menu = ctk_ui_manager_get_widget (window->priv->manager, "/NotebookPopup");
	g_return_val_if_fail (menu != NULL, FALSE);

// CHECK do we need this?
#if 0
	/* allow extensions to sync when showing the popup */
	action = ctk_action_group_get_action (window->priv->action_group,
					      "NotebookPopupAction");
	g_return_val_if_fail (action != NULL, FALSE);
	ctk_action_activate (action);
#endif

	CtkWidget *tab;
	CtkWidget *tab_label;

	tab = CTK_WIDGET (lapiz_window_get_active_tab (window));
	g_return_val_if_fail (tab != NULL, FALSE);

	tab_label = ctk_notebook_get_tab_label (notebook, tab);

	ctk_menu_popup_at_widget (CTK_MENU (menu),
	                          tab_label,
	                          CDK_GRAVITY_SOUTH_WEST,
	                          CDK_GRAVITY_NORTH_WEST,
	                          (const CdkEvent*) event);

	ctk_menu_shell_select_first (CTK_MENU_SHELL (menu), FALSE);

	return TRUE;
}

static gboolean
notebook_button_press_event (CtkNotebook    *notebook,
			     CdkEventButton *event,
			     LapizWindow    *window)
{
	if (event->type == CDK_BUTTON_PRESS)
	{
		if (event->button == 3)
			return show_notebook_popup_menu (notebook, window, event);

		else if (event->button == 2)
		{
			LapizTab *tab;
			tab = lapiz_window_get_active_tab (window);
			notebook_tab_close_request (LAPIZ_NOTEBOOK (notebook), tab, CTK_WINDOW (window));
		}
	}
	else if ((event->type == CDK_2BUTTON_PRESS) && (event->button == 1))
	{
		lapiz_window_create_tab (window, TRUE);
	}

	return FALSE;
}

static gboolean
notebook_scroll_event (CtkNotebook    *notebook,
                       CdkEventScroll *event,
                       LapizWindow    *window)
{
	if (event->direction == CDK_SCROLL_UP || event->direction == CDK_SCROLL_LEFT)
	{
		ctk_notebook_prev_page (notebook);
	}
	else if (event->direction == CDK_SCROLL_DOWN || event->direction == CDK_SCROLL_RIGHT)
	{
		ctk_notebook_next_page (notebook);
	}

	return FALSE;
}

static gboolean
notebook_popup_menu (CtkNotebook *notebook,
		     LapizWindow *window)
{
	/* Only respond if the notebook is the actual focus */
	if (LAPIZ_IS_NOTEBOOK (ctk_window_get_focus (CTK_WINDOW (window))))
	{
		return show_notebook_popup_menu (notebook, window, NULL);
	}

	return FALSE;
}

static void
side_panel_size_allocate (CtkWidget     *widget,
			  CtkAllocation *allocation,
			  LapizWindow   *window)
{
	window->priv->side_panel_size = allocation->width;
}

static void
bottom_panel_size_allocate (CtkWidget     *widget,
			    CtkAllocation *allocation,
			    LapizWindow   *window)
{
	window->priv->bottom_panel_size = allocation->height;
}

static void
hpaned_restore_position (CtkWidget   *widget,
			 LapizWindow *window)
{
	gint pos;

	lapiz_debug_message (DEBUG_WINDOW,
			     "Restoring hpaned position: side panel size %d",
			     window->priv->side_panel_size);

	pos = MAX (100, window->priv->side_panel_size);
	ctk_paned_set_position (CTK_PANED (window->priv->hpaned), pos);

	/* start monitoring the size */
	g_signal_connect (window->priv->side_panel,
			  "size-allocate",
			  G_CALLBACK (side_panel_size_allocate),
			  window);

	/* run this only once */
	g_signal_handlers_disconnect_by_func (widget, hpaned_restore_position, window);
}

static void
vpaned_restore_position (CtkWidget   *widget,
			 LapizWindow *window)
{
	CtkAllocation allocation;
	gint pos;

	ctk_widget_get_allocation (widget, &allocation);

	lapiz_debug_message (DEBUG_WINDOW,
			     "Restoring vpaned position: bottom panel size %d",
			     window->priv->bottom_panel_size);

	pos = allocation.height -
	      MAX (50, window->priv->bottom_panel_size);
	ctk_paned_set_position (CTK_PANED (window->priv->vpaned), pos);

	/* start monitoring the size */
	g_signal_connect (window->priv->bottom_panel,
			  "size-allocate",
			  G_CALLBACK (bottom_panel_size_allocate),
			  window);

	/* run this only once */
	g_signal_handlers_disconnect_by_func (widget, vpaned_restore_position, window);
}

static void
side_panel_visibility_changed (CtkWidget   *side_panel,
			       LapizWindow *window)
{
	gboolean   visible;
	CtkAction *action;

	visible = ctk_widget_get_visible (side_panel);

	if (!g_settings_get_boolean (lapiz_prefs_manager->settings, "show-tabs-with-side-pane"))
	{
		if (visible)
			ctk_notebook_set_show_tabs (CTK_NOTEBOOK (window->priv->notebook), FALSE);
		else
			ctk_notebook_set_show_tabs (CTK_NOTEBOOK (window->priv->notebook),
						    g_settings_get_boolean (lapiz_prefs_manager->settings, "show-single-tab") ||
						    (ctk_notebook_get_n_pages (CTK_NOTEBOOK (window->priv->notebook)) > 1));
	}
	else
		ctk_notebook_set_show_tabs (CTK_NOTEBOOK (window->priv->notebook),
					    g_settings_get_boolean (lapiz_prefs_manager->settings, "show-single-tab") ||
					    (ctk_notebook_get_n_pages (CTK_NOTEBOOK (window->priv->notebook)) > 1));

	if (lapiz_prefs_manager_side_pane_visible_can_set ())
		lapiz_prefs_manager_set_side_pane_visible (visible);

	action = ctk_action_group_get_action (window->priv->panes_action_group,
	                                      "ViewSidePane");

	if (ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action)) != visible)
		ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), visible);

	/* focus the document */
	if (!visible && window->priv->active_tab != NULL)
		ctk_widget_grab_focus (CTK_WIDGET (
				lapiz_tab_get_view (LAPIZ_TAB (window->priv->active_tab))));
}

static void
create_side_panel (LapizWindow *window)
{
	CtkWidget *documents_panel;

	lapiz_debug (DEBUG_WINDOW);

	window->priv->side_panel = lapiz_panel_new (CTK_ORIENTATION_VERTICAL);

	ctk_paned_pack1 (CTK_PANED (window->priv->hpaned),
			 window->priv->side_panel,
			 FALSE,
			 FALSE);

	g_signal_connect_after (window->priv->side_panel,
				"show",
				G_CALLBACK (side_panel_visibility_changed),
				window);
	g_signal_connect_after (window->priv->side_panel,
				"hide",
				G_CALLBACK (side_panel_visibility_changed),
				window);

	documents_panel = lapiz_documents_panel_new (window);
	lapiz_panel_add_item_with_icon (LAPIZ_PANEL (window->priv->side_panel),
					documents_panel,
					_("Documents"),
					"text-x-generic");
}

static void
bottom_panel_visibility_changed (LapizPanel  *bottom_panel,
				 LapizWindow *window)
{
	gboolean visible;
	CtkAction *action;

	visible = ctk_widget_get_visible (CTK_WIDGET (bottom_panel));

	if (lapiz_prefs_manager_bottom_panel_visible_can_set ())
		lapiz_prefs_manager_set_bottom_panel_visible (visible);

	action = ctk_action_group_get_action (window->priv->panes_action_group,
					      "ViewBottomPane");

	if (ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action)) != visible)
		ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), visible);

	/* focus the document */
	if (!visible && window->priv->active_tab != NULL)
		ctk_widget_grab_focus (CTK_WIDGET (
				lapiz_tab_get_view (LAPIZ_TAB (window->priv->active_tab))));
}

static void
bottom_panel_item_removed (LapizPanel  *panel,
			   CtkWidget   *item,
			   LapizWindow *window)
{
	if (lapiz_panel_get_n_items (panel) == 0)
	{
		CtkAction *action;

		ctk_widget_hide (CTK_WIDGET (panel));

		action = ctk_action_group_get_action (window->priv->panes_action_group,
						      "ViewBottomPane");
		ctk_action_set_sensitive (action, FALSE);
	}
}

static void
bottom_panel_item_added (LapizPanel  *panel,
			 CtkWidget   *item,
			 LapizWindow *window)
{
	/* if it's the first item added, set the menu item
	 * sensitive and if needed show the panel */
	if (lapiz_panel_get_n_items (panel) == 1)
	{
		CtkAction *action;
		gboolean show;

		action = ctk_action_group_get_action (window->priv->panes_action_group,
						      "ViewBottomPane");
		ctk_action_set_sensitive (action, TRUE);

		show = ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action));
		if (show)
			ctk_widget_show (CTK_WIDGET (panel));
	}
}

static void
create_bottom_panel (LapizWindow *window)
{
	lapiz_debug (DEBUG_WINDOW);

	window->priv->bottom_panel = lapiz_panel_new (CTK_ORIENTATION_HORIZONTAL);

	ctk_paned_pack2 (CTK_PANED (window->priv->vpaned),
			 window->priv->bottom_panel,
			 FALSE,
			 FALSE);

	g_signal_connect_after (window->priv->bottom_panel,
				"show",
				G_CALLBACK (bottom_panel_visibility_changed),
				window);
	g_signal_connect_after (window->priv->bottom_panel,
				"hide",
				G_CALLBACK (bottom_panel_visibility_changed),
				window);
}

static void
init_panels_visibility (LapizWindow *window)
{
	gint active_page;

	lapiz_debug (DEBUG_WINDOW);

	/* side pane */
	active_page = lapiz_prefs_manager_get_side_panel_active_page ();
	_lapiz_panel_set_active_item_by_id (LAPIZ_PANEL (window->priv->side_panel),
					    active_page);

	if (lapiz_prefs_manager_get_side_pane_visible ())
	{
		ctk_widget_show (window->priv->side_panel);
	}

	/* bottom pane, it can be empty */
	if (lapiz_panel_get_n_items (LAPIZ_PANEL (window->priv->bottom_panel)) > 0)
	{
		active_page = lapiz_prefs_manager_get_bottom_panel_active_page ();
		_lapiz_panel_set_active_item_by_id (LAPIZ_PANEL (window->priv->bottom_panel),
						    active_page);

		if (lapiz_prefs_manager_get_bottom_panel_visible ())
		{
			ctk_widget_show (window->priv->bottom_panel);
		}
	}
	else
	{
		CtkAction *action;
		action = ctk_action_group_get_action (window->priv->panes_action_group,
						      "ViewBottomPane");
		ctk_action_set_sensitive (action, FALSE);
	}

	/* start track sensitivity after the initial state is set */
	window->priv->bottom_panel_item_removed_handler_id =
		g_signal_connect (window->priv->bottom_panel,
				  "item_removed",
				  G_CALLBACK (bottom_panel_item_removed),
				  window);

	g_signal_connect (window->priv->bottom_panel,
			  "item_added",
			  G_CALLBACK (bottom_panel_item_added),
			  window);
}

static void
clipboard_owner_change (CtkClipboard        *clipboard,
			CdkEventOwnerChange *event,
			LapizWindow         *window)
{
	set_paste_sensitivity_according_to_clipboard (window,
						      clipboard);
}

static void
window_realized (CtkWidget *window,
		 gpointer  *data)
{
	CtkClipboard *clipboard;

	clipboard = ctk_widget_get_clipboard (window,
					      CDK_SELECTION_CLIPBOARD);

	g_signal_connect (clipboard,
			  "owner_change",
			  G_CALLBACK (clipboard_owner_change),
			  window);
}

static void
window_unrealized (CtkWidget *window,
		   gpointer  *data)
{
	CtkClipboard *clipboard;

	clipboard = ctk_widget_get_clipboard (window,
					      CDK_SELECTION_CLIPBOARD);

	g_signal_handlers_disconnect_by_func (clipboard,
					      G_CALLBACK (clipboard_owner_change),
					      window);
}

static void
check_window_is_active (LapizWindow *window,
			GParamSpec *property,
			gpointer useless)
{
	if (window->priv->window_state & CDK_WINDOW_STATE_FULLSCREEN)
	{
		if (ctk_window_is_active (CTK_WINDOW (window)))
		{
			ctk_widget_show (window->priv->fullscreen_controls);
		}
		else
		{
			ctk_widget_hide (window->priv->fullscreen_controls);
		}
	}
}

static void
connect_notebook_signals (LapizWindow *window,
			  CtkWidget   *notebook)
{
	g_signal_connect (notebook,
			  "switch-page",
			  G_CALLBACK (notebook_switch_page),
			  window);
	g_signal_connect (notebook,
			  "tab-added",
			  G_CALLBACK (notebook_tab_added),
			  window);
	g_signal_connect (notebook,
			  "tab-removed",
			  G_CALLBACK (notebook_tab_removed),
			  window);
	g_signal_connect (notebook,
			  "tabs-reordered",
			  G_CALLBACK (notebook_tabs_reordered),
			  window);
	g_signal_connect (notebook,
			  "tab-detached",
			  G_CALLBACK (notebook_tab_detached),
			  window);
	g_signal_connect (notebook,
			  "tab-close-request",
			  G_CALLBACK (notebook_tab_close_request),
			  window);
	g_signal_connect (notebook,
			  "button-press-event",
			  G_CALLBACK (notebook_button_press_event),
			  window);
	g_signal_connect (notebook,
			  "popup-menu",
			  G_CALLBACK (notebook_popup_menu),
			  window);
	g_signal_connect (notebook,
			  "scroll-event",
			  G_CALLBACK (notebook_scroll_event),
			  window);
}

static void
add_notebook (LapizWindow *window,
	      CtkWidget   *notebook)
{
	ctk_paned_pack1 (CTK_PANED (window->priv->vpaned),
	                 notebook,
	                 TRUE,
	                 TRUE);

	ctk_widget_show (notebook);

	ctk_widget_add_events (notebook, CDK_SCROLL_MASK);
	connect_notebook_signals (window, notebook);
}

static void
on_extension_added (PeasExtensionSet *extensions,
		    PeasPluginInfo   *info,
		    PeasExtension    *exten,
		    LapizWindow      *window)
{
	bean_extension_call (exten, "activate", window);
}

static void
on_extension_removed (PeasExtensionSet *extensions,
		      PeasPluginInfo   *info,
		      PeasExtension    *exten,
		      LapizWindow      *window)
{
	bean_extension_call (exten, "deactivate", window);

	/* Ensure update of ui manager, because we suspect it does something
	 * with expected static strings in the type module (when unloaded the
	 * strings don't exist anymore, and ui manager updates in an idle
	 * func) */
	ctk_ui_manager_ensure_update (window->priv->manager);
}

static void
lapiz_window_init (LapizWindow *window)
{
	CtkWidget *main_box;
	CtkTargetList *tl;

	lapiz_debug (DEBUG_WINDOW);

	window->priv = lapiz_window_get_instance_private (window);
	window->priv->active_tab = NULL;
	window->priv->num_tabs = 0;
	window->priv->removing_tabs = FALSE;
	window->priv->state = LAPIZ_WINDOW_STATE_NORMAL;
	window->priv->dispose_has_run = FALSE;
	window->priv->fullscreen_controls = NULL;
	window->priv->fullscreen_animation_timeout_id = 0;

	window->priv->message_bus = lapiz_message_bus_new ();

	window->priv->window_group = ctk_window_group_new ();
	ctk_window_group_add_window (window->priv->window_group, CTK_WINDOW (window));

	CtkStyleContext *context;

	context = ctk_widget_get_style_context (CTK_WIDGET (window));
	ctk_style_context_add_class (context, "lapiz-window");

	main_box = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
	ctk_container_add (CTK_CONTAINER (window), main_box);
	ctk_widget_show (main_box);

	/* Add menu bar and toolbar bar */
	create_menu_bar_and_toolbar (window, main_box);

	/* Add status bar */
	create_statusbar (window, main_box);

	/* Add the main area */
	lapiz_debug_message (DEBUG_WINDOW, "Add main area");
	window->priv->hpaned = ctk_paned_new (CTK_ORIENTATION_HORIZONTAL);
  	ctk_box_pack_start (CTK_BOX (main_box),
  			    window->priv->hpaned,
  			    TRUE,
  			    TRUE,
  			    0);

	window->priv->vpaned = ctk_paned_new (CTK_ORIENTATION_VERTICAL);
  	ctk_paned_pack2 (CTK_PANED (window->priv->hpaned),
  			 window->priv->vpaned,
  			 TRUE,
  			 FALSE);

	lapiz_debug_message (DEBUG_WINDOW, "Create lapiz notebook");
	window->priv->notebook = lapiz_notebook_new ();
	add_notebook (window, window->priv->notebook);

	/* side and bottom panels */
  	create_side_panel (window);
	create_bottom_panel (window);

	/* panes' state must be restored after panels have been mapped,
	 * since the bottom pane position depends on the size of the vpaned. */
	window->priv->side_panel_size = lapiz_prefs_manager_get_side_panel_size ();
	window->priv->bottom_panel_size = lapiz_prefs_manager_get_bottom_panel_size ();

	g_signal_connect_after (window->priv->hpaned,
				"map",
				G_CALLBACK (hpaned_restore_position),
				window);
	g_signal_connect_after (window->priv->vpaned,
				"map",
				G_CALLBACK (vpaned_restore_position),
				window);

	ctk_widget_show (window->priv->hpaned);
	ctk_widget_show (window->priv->vpaned);

	/* Drag and drop support, set targets to NULL because we add the
	   default uri_targets below */
	ctk_drag_dest_set (CTK_WIDGET (window),
			   CTK_DEST_DEFAULT_MOTION |
			   CTK_DEST_DEFAULT_HIGHLIGHT |
			   CTK_DEST_DEFAULT_DROP,
			   NULL,
			   0,
			   CDK_ACTION_COPY);

	/* Add uri targets */
	tl = ctk_drag_dest_get_target_list (CTK_WIDGET (window));

	if (tl == NULL)
	{
		tl = ctk_target_list_new (NULL, 0);
		ctk_drag_dest_set_target_list (CTK_WIDGET (window), tl);
		ctk_target_list_unref (tl);
	}

	ctk_target_list_add_uri_targets (tl, TARGET_URI_LIST);

	/* connect instead of override, so that we can
	 * share the cb code with the view */
	g_signal_connect (window,
			  "drag_data_received",
	                  G_CALLBACK (drag_data_received_cb),
	                  NULL);

	/* we can get the clipboard only after the widget
	 * is realized */
	g_signal_connect (window,
			  "realize",
			  G_CALLBACK (window_realized),
			  NULL);
	g_signal_connect (window,
			  "unrealize",
			  G_CALLBACK (window_unrealized),
			  NULL);

	/* Check if the window is active for fullscreen */
	g_signal_connect (window,
			  "notify::is-active",
			  G_CALLBACK (check_window_is_active),
			  NULL);

	lapiz_debug_message (DEBUG_WINDOW, "Update plugins ui");

	window->priv->extensions = bean_extension_set_new (PEAS_ENGINE (lapiz_plugins_engine_get_default ()),
	                                                   PEAS_TYPE_ACTIVATABLE, "object", window, NULL);

	bean_extension_set_call (window->priv->extensions, "activate");

	g_signal_connect (window->priv->extensions,
	                  "extension-added",
	                  G_CALLBACK (on_extension_added),
	                  window);
	g_signal_connect (window->priv->extensions,
	                  "extension-removed",
	                  G_CALLBACK (on_extension_removed),
	                  window);

	/* set visibility of panes.
	 * This needs to be done after plugins activatation */
	init_panels_visibility (window);

	update_sensitivity_according_to_open_tabs (window);

	lapiz_debug_message (DEBUG_WINDOW, "END");
}

/**
 * lapiz_window_get_active_view:
 * @window: a #LapizWindow
 *
 * Gets the active #LapizView.
 *
 * Returns: (transfer none): the active #LapizView
 */
LapizView *
lapiz_window_get_active_view (LapizWindow *window)
{
	LapizView *view;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	if (window->priv->active_tab == NULL)
		return NULL;

	view = lapiz_tab_get_view (LAPIZ_TAB (window->priv->active_tab));

	return view;
}

/**
 * lapiz_window_get_active_document:
 * @window: a #LapizWindow
 *
 * Gets the active #LapizDocument.
 *
 * Returns: (transfer none): the active #LapizDocument
 */
LapizDocument *
lapiz_window_get_active_document (LapizWindow *window)
{
	LapizView *view;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	view = lapiz_window_get_active_view (window);
	if (view == NULL)
		return NULL;

	return LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (view)));
}

CtkWidget *
_lapiz_window_get_notebook (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	return window->priv->notebook;
}

/**
 * lapiz_window_create_tab:
 * @window: a #LapizWindow
 * @jump_to: %TRUE to set the new #LapizTab as active
 *
 * Creates a new #LapizTab and adds the new tab to the #LapizNotebook.
 * In case @jump_to is %TRUE the #LapizNotebook switches to that new #LapizTab.
 *
 * Returns: (transfer none): a new #LapizTab
 */
LapizTab *
lapiz_window_create_tab (LapizWindow *window,
			 gboolean     jump_to)
{
	LapizTab *tab;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	tab = LAPIZ_TAB (_lapiz_tab_new ());
	ctk_widget_show (CTK_WIDGET (tab));

	lapiz_notebook_add_tab (LAPIZ_NOTEBOOK (window->priv->notebook),
				tab,
				-1,
				jump_to);

	if (!ctk_widget_get_visible (CTK_WIDGET (window)))
	{
		ctk_window_present (CTK_WINDOW (window));
	}

	return tab;
}

/**
 * lapiz_window_create_tab_from_uri:
 * @window: a #LapizWindow
 * @uri: the uri of the document
 * @encoding: a #LapizEncoding
 * @line_pos: the line position to visualize
 * @create: %TRUE to create a new document in case @uri does exist
 * @jump_to: %TRUE to set the new #LapizTab as active
 *
 * Creates a new #LapizTab loading the document specified by @uri.
 * In case @jump_to is %TRUE the #LapizNotebook swithes to that new #LapizTab.
 * Whether @create is %TRUE, creates a new empty document if location does
 * not refer to an existing file
 *
 * Returns: (transfer none): a new #LapizTab
 */
LapizTab *
lapiz_window_create_tab_from_uri (LapizWindow         *window,
				  const gchar         *uri,
				  const LapizEncoding *encoding,
				  gint                 line_pos,
				  gboolean             create,
				  gboolean             jump_to)
{
	CtkWidget *tab;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	tab = _lapiz_tab_new_from_uri (uri,
				       encoding,
				       line_pos,
				       create);
	if (tab == NULL)
		return NULL;

	ctk_widget_show (tab);

	lapiz_notebook_add_tab (LAPIZ_NOTEBOOK (window->priv->notebook),
				LAPIZ_TAB (tab),
				-1,
				jump_to);


	if (!ctk_widget_get_visible (CTK_WIDGET (window)))
	{
		ctk_window_present (CTK_WINDOW (window));
	}

	return LAPIZ_TAB (tab);
}

/**
 * lapiz_window_get_active_tab:
 * @window: a LapizWindow
 *
 * Gets the active #LapizTab in the @window.
 *
 * Returns: (transfer none): the active #LapizTab in the @window.
 */
LapizTab *
lapiz_window_get_active_tab (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	return (window->priv->active_tab == NULL) ?
				NULL : LAPIZ_TAB (window->priv->active_tab);
}

static void
add_document (LapizTab *tab, GList **res)
{
	LapizDocument *doc;

	doc = lapiz_tab_get_document (tab);

	*res = g_list_prepend (*res, doc);
}

/**
 * lapiz_window_get_documents:
 * @window: a #LapizWindow
 *
 * Gets a newly allocated list with all the documents in the window.
 * This list must be freed.
 *
 * Returns: (element-type Lapiz.Document) (transfer container): a newly
 * allocated list with all the documents in the window
 */
GList *
lapiz_window_get_documents (LapizWindow *window)
{
	GList *res = NULL;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	ctk_container_foreach (CTK_CONTAINER (window->priv->notebook),
			       (CtkCallback)add_document,
			       &res);

	res = g_list_reverse (res);

	return res;
}

static void
add_view (LapizTab *tab, GList **res)
{
	LapizView *view;

	view = lapiz_tab_get_view (tab);

	*res = g_list_prepend (*res, view);
}

/**
 * lapiz_window_get_views:
 * @window: a #LapizWindow
 *
 * Gets a list with all the views in the window. This list must be freed.
 *
 * Returns: (element-type Lapiz.View) (transfer container): a newly allocated
 * list with all the views in the window
 */
GList *
lapiz_window_get_views (LapizWindow *window)
{
	GList *res = NULL;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	ctk_container_foreach (CTK_CONTAINER (window->priv->notebook),
			       (CtkCallback)add_view,
			       &res);

	res = g_list_reverse (res);

	return res;
}

/**
 * lapiz_window_close_tab:
 * @window: a #LapizWindow
 * @tab: the #LapizTab to close
 *
 * Closes the @tab.
 */
void
lapiz_window_close_tab (LapizWindow *window,
			LapizTab    *tab)
{
	g_return_if_fail (LAPIZ_IS_WINDOW (window));
	g_return_if_fail (LAPIZ_IS_TAB (tab));
	g_return_if_fail ((lapiz_tab_get_state (tab) != LAPIZ_TAB_STATE_SAVING) &&
			  (lapiz_tab_get_state (tab) != LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW));

	lapiz_notebook_remove_tab (LAPIZ_NOTEBOOK (window->priv->notebook),
				   tab);
}

/**
 * lapiz_window_close_all_tabs:
 * @window: a #LapizWindow
 *
 * Closes all opened tabs.
 */
void
lapiz_window_close_all_tabs (LapizWindow *window)
{
	g_return_if_fail (LAPIZ_IS_WINDOW (window));
	g_return_if_fail (!(window->priv->state & LAPIZ_WINDOW_STATE_SAVING) &&
			  !(window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION));

	window->priv->removing_tabs = TRUE;

	lapiz_notebook_remove_all_tabs (LAPIZ_NOTEBOOK (window->priv->notebook));

	window->priv->removing_tabs = FALSE;
}

/**
 * lapiz_window_close_tabs:
 * @window: a #LapizWindow
 * @tabs: (element-type Lapiz.Tab): a list of #LapizTab
 *
 * Closes all tabs specified by @tabs.
 */
void
lapiz_window_close_tabs (LapizWindow *window,
			 const GList *tabs)
{
	g_return_if_fail (LAPIZ_IS_WINDOW (window));
	g_return_if_fail (!(window->priv->state & LAPIZ_WINDOW_STATE_SAVING) &&
			  !(window->priv->state & LAPIZ_WINDOW_STATE_SAVING_SESSION));

	if (tabs == NULL)
		return;

	window->priv->removing_tabs = TRUE;

	while (tabs != NULL)
	{
		if (tabs->next == NULL)
			window->priv->removing_tabs = FALSE;

		lapiz_notebook_remove_tab (LAPIZ_NOTEBOOK (window->priv->notebook),
				   	   LAPIZ_TAB (tabs->data));

		tabs = g_list_next (tabs);
	}

	g_return_if_fail (window->priv->removing_tabs == FALSE);
}

LapizWindow *
_lapiz_window_move_tab_to_new_window (LapizWindow *window,
				      LapizTab    *tab)
{
	LapizWindow *new_window;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);
	g_return_val_if_fail (LAPIZ_IS_TAB (tab), NULL);
	g_return_val_if_fail (ctk_notebook_get_n_pages (
				CTK_NOTEBOOK (window->priv->notebook)) > 1,
			      NULL);

	new_window = clone_window (window);

	lapiz_notebook_move_tab (LAPIZ_NOTEBOOK (window->priv->notebook),
				 LAPIZ_NOTEBOOK (new_window->priv->notebook),
				 tab,
				 -1);

	ctk_widget_show (CTK_WIDGET (new_window));

	return new_window;
}

/**
 * lapiz_window_set_active_tab:
 * @window: a #LapizWindow
 * @tab: a #LapizTab
 *
 * Switches to the tab that matches with @tab.
 */
void
lapiz_window_set_active_tab (LapizWindow *window,
			     LapizTab    *tab)
{
	gint page_num;

	g_return_if_fail (LAPIZ_IS_WINDOW (window));
	g_return_if_fail (LAPIZ_IS_TAB (tab));

	page_num = ctk_notebook_page_num (CTK_NOTEBOOK (window->priv->notebook),
					  CTK_WIDGET (tab));
	g_return_if_fail (page_num != -1);

	ctk_notebook_set_current_page (CTK_NOTEBOOK (window->priv->notebook),
				       page_num);
}

/**
 * lapiz_window_get_group:
 * @window: a #LapizWindow
 *
 * Gets the #CtkWindowGroup in which @window resides.
 *
 * Returns: (transfer none): the #CtkWindowGroup
 */
CtkWindowGroup *
lapiz_window_get_group (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	return window->priv->window_group;
}

gboolean
_lapiz_window_is_removing_tabs (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), FALSE);

	return window->priv->removing_tabs;
}

/**
 * lapiz_window_get_ui_manager:
 * @window: a #LapizWindow
 *
 * Gets the #CtkUIManager associated with the @window.
 *
 * Returns: (transfer none): the #CtkUIManager of the @window.
 */
CtkUIManager *
lapiz_window_get_ui_manager (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	return window->priv->manager;
}

/**
 * lapiz_window_get_side_panel:
 * @window: a #LapizWindow
 *
 * Gets the side #LapizPanel of the @window.
 *
 * Returns: (transfer none): the side #LapizPanel.
 */
LapizPanel *
lapiz_window_get_side_panel (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	return LAPIZ_PANEL (window->priv->side_panel);
}

/**
 * lapiz_window_get_bottom_panel:
 * @window: a #LapizWindow
 *
 * Gets the bottom #LapizPanel of the @window.
 *
 * Returns: (transfer none): the bottom #LapizPanel.
 */
LapizPanel *
lapiz_window_get_bottom_panel (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	return LAPIZ_PANEL (window->priv->bottom_panel);
}

/**
 * lapiz_window_get_statusbar:
 * @window: a #LapizWindow
 *
 * Gets the #LapizStatusbar of the @window.
 *
 * Returns: (transfer none): the #LapizStatusbar of the @window.
 */
CtkWidget *
lapiz_window_get_statusbar (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), 0);

	return window->priv->statusbar;
}

/**
 * lapiz_window_get_state:
 * @window: a #LapizWindow
 *
 * Retrieves the state of the @window.
 *
 * Returns: the current #LapizWindowState of the @window.
 */
LapizWindowState
lapiz_window_get_state (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), LAPIZ_WINDOW_STATE_NORMAL);

	return window->priv->state;
}

GFile *
_lapiz_window_get_default_location (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	return window->priv->default_location != NULL ?
		g_object_ref (window->priv->default_location) : NULL;
}

void
_lapiz_window_set_default_location (LapizWindow *window,
				    GFile       *location)
{
	GFile *dir;

	g_return_if_fail (LAPIZ_IS_WINDOW (window));
	g_return_if_fail (G_IS_FILE (location));

	dir = g_file_get_parent (location);
	g_return_if_fail (dir != NULL);

	if (window->priv->default_location != NULL)
		g_object_unref (window->priv->default_location);

	window->priv->default_location = dir;
}

/**
 * lapiz_window_get_unsaved_documents:
 * @window: a #LapizWindow
 *
 * Gets the list of documents that need to be saved before closing the window.
 *
 * Returns: (element-type Lapiz.Document) (transfer container): a list of
 * #LapizDocument that need to be saved before closing the window
 */
GList *
lapiz_window_get_unsaved_documents (LapizWindow *window)
{
	GList *unsaved_docs = NULL;
	GList *tabs;
	GList *l;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	tabs = ctk_container_get_children (CTK_CONTAINER (window->priv->notebook));

	l = tabs;
	while (l != NULL)
	{
		LapizTab *tab;

		tab = LAPIZ_TAB (l->data);

		if (!_lapiz_tab_can_close (tab))
		{
			LapizDocument *doc;

			doc = lapiz_tab_get_document (tab);
			unsaved_docs = g_list_prepend (unsaved_docs, doc);
		}

		l = g_list_next (l);
	}

	g_list_free (tabs);

	return g_list_reverse (unsaved_docs);
}

void
_lapiz_window_set_saving_session_state (LapizWindow *window,
					gboolean     saving_session)
{
	LapizWindowState old_state;

	g_return_if_fail (LAPIZ_IS_WINDOW (window));

	old_state = window->priv->state;

	if (saving_session)
		window->priv->state |= LAPIZ_WINDOW_STATE_SAVING_SESSION;
	else
		window->priv->state &= ~LAPIZ_WINDOW_STATE_SAVING_SESSION;

	if (old_state != window->priv->state)
	{
		set_sensitivity_according_to_window_state (window);

		g_object_notify (G_OBJECT (window), "state");
	}
}

static void
hide_notebook_tabs_on_fullscreen (CtkNotebook	*notebook,
				  GParamSpec	*pspec,
				  LapizWindow	*window)
{
	ctk_notebook_set_show_tabs (notebook, FALSE);
}

void
_lapiz_window_fullscreen (LapizWindow *window)
{
	g_return_if_fail (LAPIZ_IS_WINDOW (window));

	if (_lapiz_window_is_fullscreen (window))
		return;

	/* Go to fullscreen mode and hide bars */
	ctk_window_fullscreen (&window->window);
	ctk_notebook_set_show_tabs (CTK_NOTEBOOK (window->priv->notebook), FALSE);
	g_signal_connect (window->priv->notebook, "notify::show-tabs",
			  G_CALLBACK (hide_notebook_tabs_on_fullscreen), window);

	ctk_widget_hide (window->priv->menubar);

	g_signal_handlers_block_by_func (window->priv->toolbar,
					 toolbar_visibility_changed,
					 window);
	ctk_widget_hide (window->priv->toolbar);

	g_signal_handlers_block_by_func (window->priv->statusbar,
					 statusbar_visibility_changed,
					 window);
	ctk_widget_hide (window->priv->statusbar);

	fullscreen_controls_build (window);
	fullscreen_controls_show (window);
}

void
_lapiz_window_unfullscreen (LapizWindow *window)
{
	gboolean visible;
	CtkAction *action;

	g_return_if_fail (LAPIZ_IS_WINDOW (window));

	if (!_lapiz_window_is_fullscreen (window))
		return;

	/* Unfullscreen and show bars */
	ctk_window_unfullscreen (&window->window);
	g_signal_handlers_disconnect_by_func (window->priv->notebook,
					      hide_notebook_tabs_on_fullscreen,
					      window);
	ctk_notebook_set_show_tabs (CTK_NOTEBOOK (window->priv->notebook), TRUE);
	ctk_widget_show (window->priv->menubar);

	action = ctk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewToolbar");
	visible = ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action));
	if (visible)
		ctk_widget_show (window->priv->toolbar);
	g_signal_handlers_unblock_by_func (window->priv->toolbar,
					   toolbar_visibility_changed,
					   window);

	action = ctk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewStatusbar");
	visible = ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action));
	if (visible)
		ctk_widget_show (window->priv->statusbar);
	g_signal_handlers_unblock_by_func (window->priv->statusbar,
					   statusbar_visibility_changed,
					   window);

	ctk_widget_hide (window->priv->fullscreen_controls);
}

gboolean
_lapiz_window_is_fullscreen (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), FALSE);

	return window->priv->window_state & CDK_WINDOW_STATE_FULLSCREEN;
}

/**
 * lapiz_window_get_tab_from_location:
 * @window: a #LapizWindow
 * @location: a #GFile
 *
 * Gets the #LapizTab that matches with the given @location.
 *
 * Returns: (transfer none): the #LapizTab that matches with the given @location.
 */
LapizTab *
lapiz_window_get_tab_from_location (LapizWindow *window,
				    GFile       *location)
{
	GList *tabs;
	GList *l;
	LapizTab *ret = NULL;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);
	g_return_val_if_fail (G_IS_FILE (location), NULL);

	tabs = ctk_container_get_children (CTK_CONTAINER (window->priv->notebook));

	for (l = tabs; l != NULL; l = g_list_next (l))
	{
		LapizDocument *d;
		LapizTab *t;
		GFile *f;

		t = LAPIZ_TAB (l->data);
		d = lapiz_tab_get_document (t);

		f = lapiz_document_get_location (d);

		if ((f != NULL))
		{
			gboolean found = g_file_equal (location, f);

			g_object_unref (f);

			if (found)
			{
				ret = t;
				break;
			}
		}
	}

	g_list_free (tabs);

	return ret;
}

/**
 * lapiz_window_get_message_bus:
 * @window: a #LapizWindow
 *
 * Gets the #LapizMessageBus associated with @window. The returned reference
 * is owned by the window and should not be unreffed.
 *
 * Return value: (transfer none): the #LapizMessageBus associated with @window
 */
LapizMessageBus	*
lapiz_window_get_message_bus (LapizWindow *window)
{
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	return window->priv->message_bus;
}
