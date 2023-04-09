/*
 * lapiz-app.c
 * This file is part of lapiz
 *
 * Copyright (C) 2005-2006 - Paolo Maggi
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

#include <string.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <cdk/cdkx.h>

#include "lapiz-app.h"
#include "lapiz-prefs-manager-app.h"
#include "lapiz-commands.h"
#include "lapiz-notebook.h"
#include "lapiz-debug.h"
#include "lapiz-utils.h"
#include "lapiz-enum-types.h"
#include "lapiz-dirs.h"

#define LAPIZ_PAGE_SETUP_FILE		"lapiz-page-setup"
#define LAPIZ_PRINT_SETTINGS_FILE	"lapiz-print-settings"

/* Properties */
enum
{
	PROP_0,
	PROP_LOCKDOWN
};

struct _LapizAppPrivate
{
	GList	          *windows;
	LapizWindow       *active_window;

	LapizLockdownMask  lockdown;

	CtkPageSetup      *page_setup;
	CtkPrintSettings  *print_settings;
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizApp, lapiz_app, G_TYPE_OBJECT)

static void
lapiz_app_finalize (GObject *object)
{
	LapizApp *app = LAPIZ_APP (object);

	g_list_free (app->priv->windows);

	if (app->priv->page_setup)
		g_object_unref (app->priv->page_setup);
	if (app->priv->print_settings)
		g_object_unref (app->priv->print_settings);

	G_OBJECT_CLASS (lapiz_app_parent_class)->finalize (object);
}

static void
lapiz_app_get_property (GObject    *object,
			guint       prop_id,
			GValue     *value,
			GParamSpec *pspec)
{
	LapizApp *app = LAPIZ_APP (object);

	switch (prop_id)
	{
		case PROP_LOCKDOWN:
			g_value_set_flags (value, lapiz_app_get_lockdown (app));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_app_class_init (LapizAppClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_app_finalize;
	object_class->get_property = lapiz_app_get_property;

	g_object_class_install_property (object_class,
					 PROP_LOCKDOWN,
					 g_param_spec_flags ("lockdown",
							     "Lockdown",
							     "The lockdown mask",
							     LAPIZ_TYPE_LOCKDOWN_MASK,
							     0,
							     G_PARAM_READABLE |
							     G_PARAM_STATIC_STRINGS));
}

static gboolean
ensure_user_config_dir (void)
{
	gchar *config_dir;
	gboolean ret = TRUE;
	gint res;

	config_dir = lapiz_dirs_get_user_config_dir ();
	if (config_dir == NULL)
	{
		g_warning ("Could not get config directory\n");
		return FALSE;
	}

	res = g_mkdir_with_parents (config_dir, 0755);
	if (res < 0)
	{
		g_warning ("Could not create config directory\n");
		ret = FALSE;
	}

	g_free (config_dir);

	return ret;
}

static void
load_accels (void)
{
	gchar *filename;

	filename = lapiz_dirs_get_user_accels_file ();
	if (filename != NULL)
	{
		lapiz_debug_message (DEBUG_APP, "Loading keybindings from %s\n", filename);
		ctk_accel_map_load (filename);
		g_free (filename);
	}
}

static void
save_accels (void)
{
	gchar *filename;

	filename = lapiz_dirs_get_user_accels_file ();
	if (filename != NULL)
	{
		lapiz_debug_message (DEBUG_APP, "Saving keybindings in %s\n", filename);
		ctk_accel_map_save (filename);
		g_free (filename);
	}
}

static gchar *
get_page_setup_file (void)
{
	gchar *config_dir;
	gchar *setup = NULL;

	config_dir = lapiz_dirs_get_user_config_dir ();

	if (config_dir != NULL)
	{
		setup = g_build_filename (config_dir,
					  LAPIZ_PAGE_SETUP_FILE,
					  NULL);
		g_free (config_dir);
	}

	return setup;
}

static void
load_page_setup (LapizApp *app)
{
	gchar *filename;
	GError *error = NULL;

	g_return_if_fail (app->priv->page_setup == NULL);

	filename = get_page_setup_file ();

	app->priv->page_setup = ctk_page_setup_new_from_file (filename,
							      &error);
	if (error)
	{
		/* Ignore file not found error */
		if (error->domain != G_FILE_ERROR ||
		    error->code != G_FILE_ERROR_NOENT)
		{
			g_warning ("%s", error->message);
		}

		g_error_free (error);
	}

	g_free (filename);

	/* fall back to default settings */
	if (app->priv->page_setup == NULL)
		app->priv->page_setup = ctk_page_setup_new ();
}

static void
save_page_setup (LapizApp *app)
{
	gchar *filename;
	GError *error = NULL;

	if (app->priv->page_setup == NULL)
		return;

	filename = get_page_setup_file ();

	ctk_page_setup_to_file (app->priv->page_setup,
				filename,
				&error);
	if (error)
	{
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_free (filename);
}

static gchar *
get_print_settings_file (void)
{
	gchar *config_dir;
	gchar *settings = NULL;

	config_dir = lapiz_dirs_get_user_config_dir ();

	if (config_dir != NULL)
	{
		settings = g_build_filename (config_dir,
					     LAPIZ_PRINT_SETTINGS_FILE,
					     NULL);
		g_free (config_dir);
	}

	return settings;
}

static void
load_print_settings (LapizApp *app)
{
	gchar *filename;
	GError *error = NULL;

	g_return_if_fail (app->priv->print_settings == NULL);

	filename = get_print_settings_file ();

	app->priv->print_settings = ctk_print_settings_new_from_file (filename,
								      &error);
	if (error)
	{
		/* Ignore file not found error */
		if (error->domain != G_FILE_ERROR ||
		    error->code != G_FILE_ERROR_NOENT)
		{
			g_warning ("%s", error->message);
		}

		g_error_free (error);
	}

	g_free (filename);

	/* fall back to default settings */
	if (app->priv->print_settings == NULL)
		app->priv->print_settings = ctk_print_settings_new ();
}

static void
save_print_settings (LapizApp *app)
{
	gchar *filename;
	GError *error = NULL;

	if (app->priv->print_settings == NULL)
		return;

	filename = get_print_settings_file ();

	ctk_print_settings_to_file (app->priv->print_settings,
				    filename,
				    &error);
	if (error)
	{
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_free (filename);
}

static void
lapiz_app_init (LapizApp *app)
{
	app->priv = lapiz_app_get_instance_private (app);

	load_accels ();

	/* initial lockdown state */
	app->priv->lockdown = lapiz_prefs_manager_get_lockdown ();
}

static void
app_weak_notify (gpointer data,
		 GObject *where_the_app_was)
{
	ctk_main_quit ();
}

/**
 * lapiz_app_get_default:
 *
 * Returns the #LapizApp object. This object is a singleton and
 * represents the running lapiz instance.
 *
 * Return value: (transfer none): the #LapizApp pointer
 */
LapizApp *
lapiz_app_get_default (void)
{
	static LapizApp *app = NULL;

	if (app != NULL)
		return app;

	app = LAPIZ_APP (g_object_new (LAPIZ_TYPE_APP, NULL));

	g_object_add_weak_pointer (G_OBJECT (app),
				   (gpointer) &app);
	g_object_weak_ref (G_OBJECT (app),
			   app_weak_notify,
			   NULL);

	return app;
}

static void
set_active_window (LapizApp    *app,
                   LapizWindow *window)
{
	app->priv->active_window = window;
}

static gboolean
window_focus_in_event (LapizWindow   *window,
		       GdkEventFocus *event,
		       LapizApp      *app)
{
	/* updates active_view and active_child when a new toplevel receives focus */
	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), FALSE);

	set_active_window (app, window);

	return FALSE;
}

static gboolean
window_delete_event (LapizWindow *window,
                     GdkEvent    *event,
                     LapizApp    *app)
{
	LapizWindowState ws;

	ws = lapiz_window_get_state (window);

	if (ws &
	    (LAPIZ_WINDOW_STATE_SAVING |
	     LAPIZ_WINDOW_STATE_PRINTING |
	     LAPIZ_WINDOW_STATE_SAVING_SESSION))
	    	return TRUE;

	_lapiz_cmd_file_quit (NULL, window);

	/* Do not destroy the window */
	return TRUE;
}

static void
window_destroy (LapizWindow *window,
		LapizApp    *app)
{
	app->priv->windows = g_list_remove (app->priv->windows,
					    window);

	if (window == app->priv->active_window)
	{
		set_active_window (app, app->priv->windows != NULL ? app->priv->windows->data : NULL);
	}

/* CHECK: I don't think we have to disconnect this function, since windows
   is being destroyed */
/*
	g_signal_handlers_disconnect_by_func (window,
					      G_CALLBACK (window_focus_in_event),
					      app);
	g_signal_handlers_disconnect_by_func (window,
					      G_CALLBACK (window_destroy),
					      app);
*/
	if (app->priv->windows == NULL)
	{
		/* Last window is gone... save some settings and exit */
		ensure_user_config_dir ();

		save_accels ();
		save_page_setup (app);
		save_print_settings (app);

		g_object_unref (app);
	}
}

/* Generates a unique string for a window role */
static gchar *
gen_role (void)
{
	GTimeVal result;
	static gint serial;

	g_get_current_time (&result);

	return g_strdup_printf ("lapiz-window-%ld-%ld-%d-%s",
				result.tv_sec,
				result.tv_usec,
				serial++,
				g_get_host_name ());
}

static LapizWindow *
lapiz_app_create_window_real (LapizApp    *app,
			      gboolean     set_geometry,
			      const gchar *role)
{
	LapizWindow *window;

	lapiz_debug (DEBUG_APP);

	/*
	 * We need to be careful here, there is a race condition:
	 * when another lapiz is launched it checks active_window,
	 * so we must do our best to ensure that active_window
	 * is never NULL when at least a window exists.
	 */
	if (app->priv->windows == NULL)
	{
		window = g_object_new (LAPIZ_TYPE_WINDOW, NULL);
		set_active_window (app, window);
	}
	else
	{
		window = g_object_new (LAPIZ_TYPE_WINDOW, NULL);
	}

	app->priv->windows = g_list_prepend (app->priv->windows,
					     window);

	lapiz_debug_message (DEBUG_APP, "Window created");

	if (role != NULL)
	{
		ctk_window_set_role (CTK_WINDOW (window), role);
	}
	else
	{
		gchar *newrole;

		newrole = gen_role ();
		ctk_window_set_role (CTK_WINDOW (window), newrole);
		g_free (newrole);
	}

	if (set_geometry)
	{
		GdkWindowState state;
		gint w, h;

		state = lapiz_prefs_manager_get_window_state ();

		if ((state & GDK_WINDOW_STATE_MAXIMIZED) != 0)
		{
			lapiz_prefs_manager_get_default_window_size (&w, &h);
			ctk_window_set_default_size (CTK_WINDOW (window), w, h);
			ctk_window_maximize (CTK_WINDOW (window));
		}
		else
		{
			lapiz_prefs_manager_get_window_size (&w, &h);
			ctk_window_set_default_size (CTK_WINDOW (window), w, h);
			ctk_window_unmaximize (CTK_WINDOW (window));
		}

		if ((state & GDK_WINDOW_STATE_STICKY ) != 0)
			ctk_window_stick (CTK_WINDOW (window));
		else
			ctk_window_unstick (CTK_WINDOW (window));
	}

	g_signal_connect (window,
			  "focus_in_event",
			  G_CALLBACK (window_focus_in_event),
			  app);
	g_signal_connect (window,
			  "delete_event",
			  G_CALLBACK (window_delete_event),
			  app);
	g_signal_connect (window,
			  "destroy",
			  G_CALLBACK (window_destroy),
			  app);

	return window;
}

/**
 * lapiz_app_create_window:
 * @app: the #LapizApp
 * @screen: (allow-none):
 *
 * Create a new #LapizWindow part of @app.
 *
 * Return value: (transfer none): the new #LapizWindow
 */
LapizWindow *
lapiz_app_create_window (LapizApp  *app,
			 GdkScreen *screen)
{
	LapizWindow *window;

	window = lapiz_app_create_window_real (app, TRUE, NULL);

	if (screen != NULL)
		ctk_window_set_screen (CTK_WINDOW (window), screen);

	return window;
}

/*
 * Same as _create_window, but doesn't set the geometry.
 * The session manager takes care of it. Used in cafe-session.
 */
LapizWindow *
_lapiz_app_restore_window (LapizApp    *app,
			   const gchar *role)
{
	LapizWindow *window;

	window = lapiz_app_create_window_real (app, FALSE, role);

	return window;
}

/**
 * lapiz_app_get_windows:
 * @app: the #LapizApp
 *
 * Returns all the windows currently present in #LapizApp.
 *
 * Return value: (element-type Lapiz.Window) (transfer none): the list of #LapizWindows objects.
 * The list should not be freed
 */
const GList *
lapiz_app_get_windows (LapizApp *app)
{
	g_return_val_if_fail (LAPIZ_IS_APP (app), NULL);

	return app->priv->windows;
}

/**
 * lapiz_app_get_active_window:
 * @app: the #LapizApp
 *
 * Retrives the #LapizWindow currently active.
 *
 * Return value: (transfer none): the active #LapizWindow
 */
LapizWindow *
lapiz_app_get_active_window (LapizApp *app)
{
	g_return_val_if_fail (LAPIZ_IS_APP (app), NULL);

	/* make sure our active window is always realized:
	 * this is needed on startup if we launch two lapiz fast
	 * enough that the second instance comes up before the
	 * first one shows its window.
	 */
	if (!ctk_widget_get_realized (CTK_WIDGET (app->priv->active_window)))
		ctk_widget_realize (CTK_WIDGET (app->priv->active_window));

	return app->priv->active_window;
}

static gboolean
is_in_viewport (LapizWindow  *window,
		GdkScreen    *screen,
		gint          workspace,
		gint          viewport_x,
		gint          viewport_y)
{
	GdkWindow *cdkwindow;
	gint ws;
	gint sc_width, sc_height;
	gint x, y, width, height;
	gint vp_x, vp_y;

	/* Check for workspace match */
	ws = lapiz_utils_get_window_workspace (CTK_WINDOW (window));
	if (ws != workspace && ws != LAPIZ_ALL_WORKSPACES)
		return FALSE;

	/* Check for viewport match */
	cdkwindow = ctk_widget_get_window (CTK_WIDGET (window));
	cdk_window_get_position (cdkwindow, &x, &y);

		width = cdk_window_get_width(cdkwindow);
		height = cdk_window_get_height(cdkwindow);

	lapiz_utils_get_current_viewport (screen, &vp_x, &vp_y);
	x += vp_x;
	y += vp_y;

	sc_width = WidthOfScreen (cdk_x11_screen_get_xscreen (screen));
	sc_height = HeightOfScreen (cdk_x11_screen_get_xscreen (screen));

	return x + width * .25 >= viewport_x &&
	       x + width * .75 <= viewport_x + sc_width &&
	       y  >= viewport_y &&
	       y + height <= viewport_y + sc_height;
}

/**
 * _lapiz_app_get_window_in_viewport:
 * @app: the #LapizApp
 * @screen: the #GdkScreen
 * @workspace: the workspace number
 * @viewport_x: the viewport horizontal origin
 * @viewport_y: the viewport vertical origin
 *
 * Since a workspace can be larger than the screen, it is divided into several
 * equal parts called viewports. This function retrives the #LapizWindow in
 * the given viewport of the given workspace.
 *
 * Return value: the #LapizWindow in the given viewport of the given workspace.
 */
LapizWindow *
_lapiz_app_get_window_in_viewport (LapizApp  *app,
				   GdkScreen *screen,
				   gint       workspace,
				   gint       viewport_x,
				   gint       viewport_y)
{
	LapizWindow *window;

	GList *l;

	g_return_val_if_fail (LAPIZ_IS_APP (app), NULL);

	/* first try if the active window */
	window = app->priv->active_window;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	if (is_in_viewport (window, screen, workspace, viewport_x, viewport_y))
		return window;

	/* otherwise try to see if there is a window on this workspace */
	for (l = app->priv->windows; l != NULL; l = l->next)
	{
		window = l->data;

		if (is_in_viewport (window, screen, workspace, viewport_x, viewport_y))
			return window;
	}

	/* no window on this workspace... create a new one */
	return lapiz_app_create_window (app, screen);
}

/**
 * lapiz_app_get_documents:
 * @app: the #LapizApp
 *
 * Returns all the documents currently open in #LapizApp.
 *
 * Return value: (element-type Lapiz.Document) (transfer container):
 * a newly allocated list of #LapizDocument objects
 */
GList *
lapiz_app_get_documents	(LapizApp *app)
{
	GList *res = NULL;
	GList *windows;

	g_return_val_if_fail (LAPIZ_IS_APP (app), NULL);

	windows = app->priv->windows;

	while (windows != NULL)
	{
		res = g_list_concat (res,
				     lapiz_window_get_documents (LAPIZ_WINDOW (windows->data)));

		windows = g_list_next (windows);
	}

	return res;
}

/**
 * lapiz_app_get_views:
 * @app: the #LapizApp
 *
 * Returns all the views currently present in #LapizApp.
 *
 * Return value: (element-type Lapiz.View) (transfer container):
 * a newly allocated list of #LapizView objects
 */
GList *
lapiz_app_get_views (LapizApp *app)
{
	GList *res = NULL;
	GList *windows;

	g_return_val_if_fail (LAPIZ_IS_APP (app), NULL);

	windows = app->priv->windows;

	while (windows != NULL)
	{
		res = g_list_concat (res,
				     lapiz_window_get_views (LAPIZ_WINDOW (windows->data)));

		windows = g_list_next (windows);
	}

	return res;
}

/**
 * lapiz_app_get_lockdown:
 * @app: a #LapizApp
 *
 * Gets the lockdown mask (see #LapizLockdownMask) for the application.
 * The lockdown mask determines which functions are locked down using
 * the CAFE-wise lockdown GSettings keys.
 **/
LapizLockdownMask
lapiz_app_get_lockdown (LapizApp *app)
{
	g_return_val_if_fail (LAPIZ_IS_APP (app), LAPIZ_LOCKDOWN_ALL);

	return app->priv->lockdown;
}

static void
app_lockdown_changed (LapizApp *app)
{
	GList *l;

	for (l = app->priv->windows; l != NULL; l = l->next)
		_lapiz_window_set_lockdown (LAPIZ_WINDOW (l->data),
					    app->priv->lockdown);

	g_object_notify (G_OBJECT (app), "lockdown");
}

void
_lapiz_app_set_lockdown (LapizApp          *app,
			 LapizLockdownMask  lockdown)
{
	g_return_if_fail (LAPIZ_IS_APP (app));

	app->priv->lockdown = lockdown;

	app_lockdown_changed (app);
}

void
_lapiz_app_set_lockdown_bit (LapizApp          *app,
			     LapizLockdownMask  bit,
			     gboolean           value)
{
	g_return_if_fail (LAPIZ_IS_APP (app));

	if (value)
		app->priv->lockdown |= bit;
	else
		app->priv->lockdown &= ~bit;

	app_lockdown_changed (app);
}

/* Returns a copy */
CtkPageSetup *
_lapiz_app_get_default_page_setup (LapizApp *app)
{
	g_return_val_if_fail (LAPIZ_IS_APP (app), NULL);

	if (app->priv->page_setup == NULL)
		load_page_setup (app);

	return ctk_page_setup_copy (app->priv->page_setup);
}

void
_lapiz_app_set_default_page_setup (LapizApp     *app,
				   CtkPageSetup *page_setup)
{
	g_return_if_fail (LAPIZ_IS_APP (app));
	g_return_if_fail (CTK_IS_PAGE_SETUP (page_setup));

	if (app->priv->page_setup != NULL)
		g_object_unref (app->priv->page_setup);

	app->priv->page_setup = g_object_ref (page_setup);
}

/* Returns a copy */
CtkPrintSettings *
_lapiz_app_get_default_print_settings (LapizApp *app)
{
	g_return_val_if_fail (LAPIZ_IS_APP (app), NULL);

	if (app->priv->print_settings == NULL)
		load_print_settings (app);

	return ctk_print_settings_copy (app->priv->print_settings);
}

void
_lapiz_app_set_default_print_settings (LapizApp         *app,
				       CtkPrintSettings *settings)
{
	g_return_if_fail (LAPIZ_IS_APP (app));
	g_return_if_fail (CTK_IS_PRINT_SETTINGS (settings));

	if (app->priv->print_settings != NULL)
		g_object_unref (app->priv->print_settings);

	app->priv->print_settings = g_object_ref (settings);
}

