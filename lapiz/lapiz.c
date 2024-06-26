/*
 * lapiz.c
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

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <cdk/cdkx.h>

#ifdef HAVE_INTROSPECTION
#include <girepository.h>
#endif

#include "lapiz-app.h"
#include "lapiz-commands.h"
#include "lapiz-debug.h"
#include "lapiz-dirs.h"
#include "lapiz-encodings.h"
#include "lapiz-plugins-engine.h"
#include "lapiz-prefs-manager-app.h"
#include "lapiz-session.h"
#include "lapiz-utils.h"
#include "lapiz-window.h"

#include "eggsmclient.h"
#include "eggdesktopfile.h"

#ifndef ENABLE_GVFS_METADATA
#include "lapiz-metadata-manager.h"
#endif

#include "bacon-message-connection.h"

static guint32 startup_timestamp = 0;

static BaconMessageConnection *connection;

/* command line */
static gint line_position = 0;
static gchar *encoding_charset = NULL;
static gboolean new_window_option = FALSE;
static gboolean new_document_option = FALSE;
static gchar **remaining_args = NULL;
static GSList *file_list = NULL;

static void
show_version_and_quit (void)
{
	g_print ("%s - Version %s\n", g_get_application_name (), VERSION);

	exit (0);
}

static void
list_encodings_and_quit (void)
{
	gint i = 0;
	const LapizEncoding *enc;

	while ((enc = lapiz_encoding_get_from_index (i)) != NULL)
	{
		g_print ("%s\n", lapiz_encoding_get_charset (enc));

		++i;
	}

	exit (0);
}

static const GOptionEntry options [] =
{
	{ "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	  show_version_and_quit, N_("Show the application's version"), NULL },

	{ "encoding", '\0', 0, G_OPTION_ARG_STRING, &encoding_charset,
	  N_("Set the character encoding to be used to open the files listed on the command line"), N_("ENCODING")},

	{ "list-encodings", '\0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	  list_encodings_and_quit, N_("Display list of possible values for the encoding option"), NULL},

	{ "new-window", '\0', 0, G_OPTION_ARG_NONE, &new_window_option,
	  N_("Create a new top-level window in an existing instance of lapiz"), NULL },

	{ "new-document", '\0', 0, G_OPTION_ARG_NONE, &new_document_option,
	  N_("Create a new document in an existing instance of lapiz"), NULL },

	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &remaining_args,
	  NULL, N_("[FILE...]") }, /* collects file arguments */

	{NULL}
};

static void
free_command_line_data (void)
{
	g_slist_foreach (file_list, (GFunc) g_object_unref, NULL);
	g_slist_free (file_list);
	file_list = NULL;

	g_strfreev (remaining_args);
	remaining_args = NULL;

	g_free (encoding_charset);
	encoding_charset = NULL;

	new_window_option = FALSE;
	new_document_option = FALSE;
	line_position = 0;
}

static void
lapiz_get_command_line_data (void)
{
	if (remaining_args)
	{
		gint i;

		for (i = 0; remaining_args[i]; i++)
		{
			if (*remaining_args[i] == '+')
			{
				if (*(remaining_args[i] + 1) == '\0')
					/* goto the last line of the document */
					line_position = G_MAXINT;
				else
					line_position = atoi (remaining_args[i] + 1);
			}
			else
			{
				GFile *file;

				file = g_file_new_for_commandline_arg (remaining_args[i]);
				file_list = g_slist_prepend (file_list, file);
			}
		}

		file_list = g_slist_reverse (file_list);
	}

	if (encoding_charset &&
	    (lapiz_encoding_get_from_charset (encoding_charset) == NULL))
	{
		g_print (_("%s: invalid encoding.\n"),
			 encoding_charset);
	}
}

static guint32
get_startup_timestamp (void)
{
	const gchar *startup_id_env;
	gchar *startup_id = NULL;
	gchar *time_str;
	gchar *end;
	gulong retval = 0;

	/* we don't unset the env, since startup-notification
	 * may still need it */
	startup_id_env = g_getenv ("DESKTOP_STARTUP_ID");
	if (startup_id_env == NULL)
		goto out;

	startup_id = g_strdup (startup_id_env);

	time_str = g_strrstr (startup_id, "_TIME");
	if (time_str == NULL)
		goto out;

	errno = 0;

	/* Skip past the "_TIME" part */
	time_str += 5;

	retval = strtoul (time_str, &end, 0);
	if (end == time_str || errno != 0)
		retval = 0;

 out:
	g_free (startup_id);

	return (retval > 0) ? retval : 0;
}

static CdkDisplay *
display_open_if_needed (const gchar *name)
{
	GSList *displays;
	GSList *l;
	CdkDisplay *display = NULL;

	displays = cdk_display_manager_list_displays (cdk_display_manager_get ());

	for (l = displays; l != NULL; l = l->next)
	{
		if (strcmp (cdk_display_get_name ((CdkDisplay *) l->data), name) == 0)
		{
			display = l->data;
			break;
		}
	}

	g_slist_free (displays);

	return display != NULL ? display : cdk_display_open (name);
}

/* serverside */
static void
on_message_received (const char *message,
		     gpointer    data G_GNUC_UNUSED)
{
	const LapizEncoding *encoding = NULL;
	gchar **commands;
	gchar **params;
	gint workspace;
	gint viewport_x;
	gint viewport_y;
	gchar *display_name;
	gint i;
	LapizApp *app;
	LapizWindow *window;
	CdkDisplay *display;
	CdkScreen *screen;

	g_return_if_fail (message != NULL);

	lapiz_debug_message (DEBUG_APP, "Received message:\n%s\n", message);

	commands = g_strsplit (message, "\v", -1);

	/* header */
	params = g_strsplit (commands[0], "\t", 6);
	startup_timestamp = atoi (params[0]);
	display_name = params[1];
	workspace = atoi (params[3]);
	viewport_x = atoi (params[4]);
	viewport_y = atoi (params[5]);

	display = display_open_if_needed (display_name);
	if (display == NULL)
	{
		g_warning ("Could not open display %s\n", display_name);
		g_strfreev (params);
		goto out;
	}

	screen = cdk_display_get_default_screen (display);

	g_strfreev (params);

	/* body */
	for (i = 1; commands[i] != NULL; i++)
	{
		params = g_strsplit (commands[i], "\t", -1);

		if (strcmp (params[0], "NEW-WINDOW") == 0)
		{
			new_window_option = TRUE;
		}
		else if (strcmp (params[0], "NEW-DOCUMENT") == 0)
		{
			new_document_option = TRUE;
		}
		else if (strcmp (params[0], "OPEN-URIS") == 0)
		{
			gint n_uris, j;
			gchar **uris;

			line_position = atoi (params[1]);

			if (*params[2] != '\0')
				encoding = lapiz_encoding_get_from_charset (params[2]);

			n_uris = atoi (params[3]);
			uris = g_strsplit (params[4], " ", n_uris);

			for (j = 0; j < n_uris; j++)
			{
				GFile *file;

				file = g_file_new_for_uri (uris[j]);
				file_list = g_slist_prepend (file_list, file);
			}

			file_list = g_slist_reverse (file_list);

			/* the list takes ownerhip of the strings,
			 * only free the array */
			g_free (uris);
		}
		else
		{
			g_warning ("Unexpected bacon command");
		}

		g_strfreev (params);
	}

	/* execute the commands */

	app = lapiz_app_get_default ();

	if (new_window_option)
	{
		window = lapiz_app_create_window (app, screen);
	}
	else
	{
		/* get a window in the current workspace (if exists) and raise it */
		window = _lapiz_app_get_window_in_viewport (app,
							    screen,
							    workspace,
							    viewport_x,
							    viewport_y);
	}

	if (file_list != NULL)
	{
		_lapiz_cmd_load_files_from_prompt (window,
						   file_list,
						   encoding,
						   line_position);

		if (new_document_option)
			lapiz_window_create_tab (window, TRUE);
	}
	else
	{
		LapizDocument *doc;
		doc = lapiz_window_get_active_document (window);

		if (doc == NULL ||
		    !lapiz_document_is_untouched (doc) ||
		    new_document_option)
			lapiz_window_create_tab (window, TRUE);
	}

	/* set the proper interaction time on the window.
	 * Fall back to roundtripping to the X server when we
	 * don't have the timestamp, e.g. when launched from
	 * terminal. We also need to make sure that the window
	 * has been realized otherwise it will not work. lame.
	 */
	if (!ctk_widget_get_realized (CTK_WIDGET (window)))
		ctk_widget_realize (CTK_WIDGET (window));

	if (startup_timestamp <= 0)
		startup_timestamp = cdk_x11_get_server_time (ctk_widget_get_window (CTK_WIDGET (window)));

	cdk_x11_window_set_user_time (ctk_widget_get_window (CTK_WIDGET (window)),
				      startup_timestamp);

	ctk_window_present (CTK_WINDOW (window));

 out:
	g_strfreev (commands);

	free_command_line_data ();
}

/* clientside */
static void
send_bacon_message (void)
{
	CdkScreen *screen;
	CdkDisplay *display;
	const gchar *display_name;
	gint screen_number;
	gint ws;
	gint viewport_x;
	gint viewport_y;
	GString *command;

	/* the messages have the following format:
	 * <---                                  header                                     ---> <----            body             ----->
	 * timestamp \t display_name \t screen_number \t workspace \t viewport_x \t viewport_y \v OP1 \t arg \t arg \v OP2 \t arg \t arg|...
	 *
	 * when the arg is a list of uri, they are separated by a space.
	 * So the delimiters are \v for the commands, \t for the tokens in
	 * a command and ' ' for the uris: note that such delimiters cannot
	 * be part of an uri, this way parsing is easier.
	 */

	lapiz_debug (DEBUG_APP);

	screen = cdk_screen_get_default ();
	display = cdk_screen_get_display (screen);

	display_name = cdk_display_get_name (display);
	screen_number = cdk_x11_screen_get_screen_number (screen);

	lapiz_debug_message (DEBUG_APP, "Display: %s", display_name);
	lapiz_debug_message (DEBUG_APP, "Screen: %d", screen_number);

	ws = lapiz_utils_get_current_workspace (screen);
	lapiz_utils_get_current_viewport (screen, &viewport_x, &viewport_y);

	command = g_string_new (NULL);

	/* header */
	g_string_append_printf (command,
				"%" G_GUINT32_FORMAT "\t%s\t%d\t%d\t%d\t%d",
				startup_timestamp,
				display_name,
				screen_number,
				ws,
				viewport_x,
				viewport_y);

	/* NEW-WINDOW command */
	if (new_window_option)
	{
		command = g_string_append_c (command, '\v');
		command = g_string_append (command, "NEW-WINDOW");
	}

	/* NEW-DOCUMENT command */
	if (new_document_option)
	{
		command = g_string_append_c (command, '\v');
		command = g_string_append (command, "NEW-DOCUMENT");
	}

	/* OPEN_URIS command, optionally specify line_num and encoding */
	if (file_list)
	{
		GSList *l;

		command = g_string_append_c (command, '\v');
		command = g_string_append (command, "OPEN-URIS");

		g_string_append_printf (command,
					"\t%d\t%s\t%u\t",
					line_position,
					encoding_charset ? encoding_charset : "",
					g_slist_length (file_list));

		for (l = file_list; l != NULL; l = l->next)
		{
			gchar *uri;

			uri = g_file_get_uri (G_FILE (l->data));
			command = g_string_append (command, uri);
			if (l->next != NULL)
				command = g_string_append_c (command, ' ');

			g_free (uri);
		}
	}

	lapiz_debug_message (DEBUG_APP, "Bacon Message: %s", command->str);

	bacon_message_connection_send (connection,
				       command->str);

	g_string_free (command, TRUE);
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	LapizPluginsEngine *engine;
	LapizWindow *window;
	LapizApp *app;
	gboolean restored = FALSE;
	GError *error = NULL;
	gchar *dir;
	gchar *icon_dir;


	/* Setup debugging */
	lapiz_debug_init ();
	lapiz_debug_message (DEBUG_APP, "Startup");

	setlocale (LC_ALL, "");

	dir = lapiz_dirs_get_lapiz_locale_dir ();
	bindtextdomain (GETTEXT_PACKAGE, dir);
	g_free (dir);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	cdk_set_allowed_backends ("x11");

	startup_timestamp = get_startup_timestamp();

	/* Setup command line options */
	context = g_option_context_new (_("- Edit text files"));
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, ctk_get_option_group (TRUE));
	g_option_context_add_group (context, egg_sm_client_get_option_group ());

#ifdef HAVE_INTROSPECTION
	g_option_context_add_group (context, g_irepository_get_option_group ());
#endif

	if (!g_option_context_parse (context, &argc, &argv, &error))
	{
		g_print(_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
		        error->message, argv[0]);
		g_error_free (error);
		g_option_context_free (context);
		return 1;
	}

	g_option_context_free (context);

	lapiz_debug_message (DEBUG_APP, "Create bacon connection");

	connection = bacon_message_connection_new ("lapiz");

	if (connection != NULL)
	{
		if (!bacon_message_connection_get_is_server (connection))
		{
			lapiz_debug_message (DEBUG_APP, "I'm a client");

			lapiz_get_command_line_data ();

			send_bacon_message ();

			free_command_line_data ();

			/* we never popup a window... tell startup-notification
			 * that we are done.
			 */
			cdk_notify_startup_complete ();

			bacon_message_connection_free (connection);

			exit (0);
		}
		else
		{
		  	lapiz_debug_message (DEBUG_APP, "I'm a server");

			bacon_message_connection_set_callback (connection,
							       on_message_received,
							       NULL);
		}
	}
	else
	{
		g_warning ("Cannot create the 'lapiz' connection.");
	}

	lapiz_debug_message (DEBUG_APP, "Set icon");

	dir = lapiz_dirs_get_lapiz_data_dir ();
	icon_dir = g_build_filename (dir,
				     "icons",
				     NULL);
	g_free (dir);

	ctk_icon_theme_append_search_path (ctk_icon_theme_get_default (),
					   icon_dir);
	g_free (icon_dir);

	/* Set the associated .desktop file */
	egg_set_desktop_file (DATADIR "/applications/lapiz.desktop");

	/* Load user preferences */
	lapiz_debug_message (DEBUG_APP, "Init prefs manager");
	lapiz_prefs_manager_app_init ();

	/* Init plugins engine */
	lapiz_debug_message (DEBUG_APP, "Init plugins");
	engine = lapiz_plugins_engine_get_default ();

	/* Initialize session management */
	lapiz_debug_message (DEBUG_APP, "Init session manager");
	lapiz_session_init ();

	if (lapiz_session_is_restored ())
		restored = lapiz_session_load ();

	if (!restored)
	{
		lapiz_debug_message (DEBUG_APP, "Analyze command line data");
		lapiz_get_command_line_data ();

		lapiz_debug_message (DEBUG_APP, "Get default app");
		app = lapiz_app_get_default ();

		lapiz_debug_message (DEBUG_APP, "Create main window");
		window = lapiz_app_create_window (app, NULL);

		if (file_list != NULL)
		{
			const LapizEncoding *encoding = NULL;

			if (encoding_charset)
				encoding = lapiz_encoding_get_from_charset (encoding_charset);

			lapiz_debug_message (DEBUG_APP, "Load files");
			_lapiz_cmd_load_files_from_prompt (window,
							   file_list,
							   encoding,
							   line_position);
		}
		else
		{
			lapiz_debug_message (DEBUG_APP, "Create tab");
			lapiz_window_create_tab (window, TRUE);
		}

		lapiz_debug_message (DEBUG_APP, "Show window");
		ctk_widget_show (CTK_WIDGET (window));

		free_command_line_data ();
	}

	lapiz_debug_message (DEBUG_APP, "Start ctk-main");

	g_object_set (ctk_settings_get_default (), "ctk-button-images", TRUE, NULL);
	g_object_set (ctk_settings_get_default (), "ctk-menu-images", TRUE, NULL);
	g_object_set (ctk_settings_get_default (), "ctk-overlay-scrolling", FALSE, NULL);

	ctk_main();

	bacon_message_connection_free (connection);

	/* We kept the original engine reference here. So let's unref it to
	 * finalize it properly.
	 */
	g_object_unref (engine);
	lapiz_prefs_manager_app_shutdown ();

#ifndef ENABLE_GVFS_METADATA
	lapiz_metadata_manager_shutdown ();
#endif

	return 0;
}

