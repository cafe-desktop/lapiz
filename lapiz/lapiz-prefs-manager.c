/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-prefs-manager.c
 * This file is part of lapiz
 *
 * Copyright (C) 2002  Paolo Maggi
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
 * Modified by the lapiz Team, 2002. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "lapiz-prefs-manager.h"
#include "lapiz-prefs-manager-private.h"
#include "lapiz-debug.h"
#include "lapiz-encodings.h"
#include "lapiz-utils.h"

#define DEFINE_BOOL_PREF(name, key) gboolean 				\
lapiz_prefs_manager_get_ ## name (void)					\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	return lapiz_prefs_manager_get_bool (key);			\
}									\
									\
void 									\
lapiz_prefs_manager_set_ ## name (gboolean v)				\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	lapiz_prefs_manager_set_bool (key,				\
				      v);				\
}									\
				      					\
gboolean								\
lapiz_prefs_manager_ ## name ## _can_set (void)				\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	return lapiz_prefs_manager_key_is_writable (key);		\
}



#define DEFINE_INT_PREF(name, key) gint					\
lapiz_prefs_manager_get_ ## name (void)			 		\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	return lapiz_prefs_manager_get_int (key);			\
}									\
									\
void 									\
lapiz_prefs_manager_set_ ## name (gint v)				\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	lapiz_prefs_manager_set_int (key,				\
				     v);				\
}									\
				      					\
gboolean								\
lapiz_prefs_manager_ ## name ## _can_set (void)				\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	return lapiz_prefs_manager_key_is_writable (key);		\
}



#define DEFINE_STRING_PREF(name, key) gchar*				\
lapiz_prefs_manager_get_ ## name (void)			 		\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	return lapiz_prefs_manager_get_string (key);			\
}									\
									\
void 									\
lapiz_prefs_manager_set_ ## name (const gchar* v)			\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	lapiz_prefs_manager_set_string (key,				\
				        v);				\
}									\
				      					\
gboolean								\
lapiz_prefs_manager_ ## name ## _can_set (void)				\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	return lapiz_prefs_manager_key_is_writable (key);		\
}



#define DEFINE_ENUM_PREF(name, key) gint				\
lapiz_prefs_manager_get_ ## name (void)			 		\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	return lapiz_prefs_manager_get_enum (key);			\
}									\
									\
void 									\
lapiz_prefs_manager_set_ ## name (gint v)				\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	lapiz_prefs_manager_set_enum (key,				\
				      v);				\
}									\
									\
gboolean								\
lapiz_prefs_manager_ ## name ## _can_set (void)				\
{									\
	lapiz_debug (DEBUG_PREFS);					\
									\
	return lapiz_prefs_manager_key_is_writable (key);		\
}


LapizPrefsManager *lapiz_prefs_manager = NULL;


static CtkWrapMode 	 get_wrap_mode_from_string 		(const gchar* str);

static gboolean		 lapiz_prefs_manager_get_bool		(const gchar* key);

static gint		 lapiz_prefs_manager_get_int		(const gchar* key);

static gchar		*lapiz_prefs_manager_get_string		(const gchar* key);

static gint		 lapiz_prefs_manager_get_enum		(const gchar* key);


gboolean
lapiz_prefs_manager_init (void)
{
	lapiz_debug (DEBUG_PREFS);

	if (lapiz_prefs_manager == NULL)
	{
		lapiz_prefs_manager = g_new0 (LapizPrefsManager, 1);
		lapiz_prefs_manager->settings = g_settings_new (LAPIZ_SCHEMA);
		lapiz_prefs_manager->lockdown_settings = g_settings_new (GPM_LOCKDOWN_SCHEMA);
		lapiz_prefs_manager->interface_settings = g_settings_new (GPM_INTERFACE_SCHEMA);
	}

	return lapiz_prefs_manager != NULL;
}

void
lapiz_prefs_manager_shutdown (void)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (lapiz_prefs_manager != NULL);

	g_object_unref (lapiz_prefs_manager->settings);
	lapiz_prefs_manager->settings = NULL;
	g_object_unref (lapiz_prefs_manager->lockdown_settings);
	lapiz_prefs_manager->lockdown_settings = NULL;
	g_object_unref (lapiz_prefs_manager->interface_settings);
	lapiz_prefs_manager->interface_settings = NULL;
}

static gboolean
lapiz_prefs_manager_get_bool (const gchar* key)
{
	lapiz_debug (DEBUG_PREFS);

	return g_settings_get_boolean (lapiz_prefs_manager->settings, key);
}

static gint
lapiz_prefs_manager_get_int (const gchar* key)
{
	lapiz_debug (DEBUG_PREFS);

	return g_settings_get_int (lapiz_prefs_manager->settings, key);
}

static gchar *
lapiz_prefs_manager_get_string (const gchar* key)
{
	lapiz_debug (DEBUG_PREFS);

	return g_settings_get_string (lapiz_prefs_manager->settings, key);
}

static gint
lapiz_prefs_manager_get_enum (const gchar* key)
{
	lapiz_debug (DEBUG_PREFS);

	return g_settings_get_enum (lapiz_prefs_manager->settings, key);
}

static void
lapiz_prefs_manager_set_bool (const gchar* key, gboolean value)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (g_settings_is_writable (
				lapiz_prefs_manager->settings, key));

	g_settings_set_boolean (lapiz_prefs_manager->settings, key, value);
}

static void
lapiz_prefs_manager_set_int (const gchar* key, gint value)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (g_settings_is_writable (
				lapiz_prefs_manager->settings, key));

	g_settings_set_int (lapiz_prefs_manager->settings, key, value);
}

static void
lapiz_prefs_manager_set_string (const gchar* key, const gchar* value)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (value != NULL);

	g_return_if_fail (g_settings_is_writable (
				lapiz_prefs_manager->settings, key));

	g_settings_set_string (lapiz_prefs_manager->settings, key, value);
}

static void
lapiz_prefs_manager_set_enum (const gchar* key, gint value)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (g_settings_is_writable (
				lapiz_prefs_manager->settings, key));

	g_settings_set_enum (lapiz_prefs_manager->settings, key, value);
}

static gboolean
lapiz_prefs_manager_key_is_writable (const gchar* key)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_val_if_fail (lapiz_prefs_manager != NULL, FALSE);
	g_return_val_if_fail (lapiz_prefs_manager->settings != NULL, FALSE);

	return g_settings_is_writable (lapiz_prefs_manager->settings, key);
}

/* Use default font */
DEFINE_BOOL_PREF (use_default_font,
		  GPM_USE_DEFAULT_FONT)

/* Editor font */
DEFINE_STRING_PREF (editor_font,
		    GPM_EDITOR_FONT)

/* System font */
gchar *
lapiz_prefs_manager_get_system_font (void)
{
	lapiz_debug (DEBUG_PREFS);

	return g_settings_get_string (lapiz_prefs_manager->interface_settings,
				      GPM_SYSTEM_FONT);
}

/* Create backup copy */
DEFINE_BOOL_PREF (create_backup_copy,
		  GPM_CREATE_BACKUP_COPY)

/* Auto save */
DEFINE_BOOL_PREF (auto_save,
		  GPM_AUTO_SAVE)

/* Auto save interval */
DEFINE_INT_PREF (auto_save_interval,
		 GPM_AUTO_SAVE_INTERVAL)


/* Undo actions limit: if < 1 then no limits */
DEFINE_INT_PREF (undo_actions_limit,
		 GPM_UNDO_ACTIONS_LIMIT)

static CtkWrapMode
get_wrap_mode_from_string (const gchar* str)
{
	CtkWrapMode res;

	g_return_val_if_fail (str != NULL, CTK_WRAP_WORD);

	if (strcmp (str, "CTK_WRAP_NONE") == 0)
		res = CTK_WRAP_NONE;
	else
	{
		if (strcmp (str, "CTK_WRAP_CHAR") == 0)
			res = CTK_WRAP_CHAR;
		else
			res = CTK_WRAP_WORD;
	}

	return res;
}

/* Wrap mode */
CtkWrapMode
lapiz_prefs_manager_get_wrap_mode (void)
{
	gchar *str;
	CtkWrapMode res;

	lapiz_debug (DEBUG_PREFS);

	str = lapiz_prefs_manager_get_string (GPM_WRAP_MODE);

	res = get_wrap_mode_from_string (str);

	g_free (str);

	return res;
}

void
lapiz_prefs_manager_set_wrap_mode (CtkWrapMode wp)
{
	const gchar * str;

	lapiz_debug (DEBUG_PREFS);

	switch (wp)
	{
		case CTK_WRAP_NONE:
			str = "CTK_WRAP_NONE";
			break;

		case CTK_WRAP_CHAR:
			str = "CTK_WRAP_CHAR";
			break;

		default: /* CTK_WRAP_WORD */
			str = "CTK_WRAP_WORD";
	}

	lapiz_prefs_manager_set_string (GPM_WRAP_MODE,
					str);
}

gboolean
lapiz_prefs_manager_wrap_mode_can_set (void)
{
	lapiz_debug (DEBUG_PREFS);

	return lapiz_prefs_manager_key_is_writable (GPM_WRAP_MODE);
}


/* Tabs size */
DEFINE_INT_PREF (tabs_size,
		 GPM_TABS_SIZE)

/* Insert spaces */
DEFINE_BOOL_PREF (insert_spaces,
		  GPM_INSERT_SPACES)

/* Auto indent */
DEFINE_BOOL_PREF (auto_indent,
		  GPM_AUTO_INDENT)

/* Display line numbers */
DEFINE_BOOL_PREF (display_line_numbers,
		  GPM_DISPLAY_LINE_NUMBERS)

/* Toolbar visibility */
DEFINE_BOOL_PREF (toolbar_visible,
		  GPM_TOOLBAR_VISIBLE)


/* Toolbar suttons style */
LapizToolbarSetting
lapiz_prefs_manager_get_toolbar_buttons_style (void)
{
	gchar *str;
	LapizToolbarSetting res;

	lapiz_debug (DEBUG_PREFS);

	str = lapiz_prefs_manager_get_string (GPM_TOOLBAR_BUTTONS_STYLE);

	if (strcmp (str, "LAPIZ_TOOLBAR_ICONS") == 0)
		res = LAPIZ_TOOLBAR_ICONS;
	else
	{
		if (strcmp (str, "LAPIZ_TOOLBAR_ICONS_AND_TEXT") == 0)
			res = LAPIZ_TOOLBAR_ICONS_AND_TEXT;
		else
		{
			if (strcmp (str, "LAPIZ_TOOLBAR_ICONS_BOTH_HORIZ") == 0)
				res = LAPIZ_TOOLBAR_ICONS_BOTH_HORIZ;
			else
				res = LAPIZ_TOOLBAR_SYSTEM;
		}
	}

	g_free (str);

	return res;
}

void
lapiz_prefs_manager_set_toolbar_buttons_style (LapizToolbarSetting tbs)
{
	const gchar * str;

	lapiz_debug (DEBUG_PREFS);

	switch (tbs)
	{
		case LAPIZ_TOOLBAR_ICONS:
			str = "LAPIZ_TOOLBAR_ICONS";
			break;

		case LAPIZ_TOOLBAR_ICONS_AND_TEXT:
			str = "LAPIZ_TOOLBAR_ICONS_AND_TEXT";
			break;

	        case LAPIZ_TOOLBAR_ICONS_BOTH_HORIZ:
			str = "LAPIZ_TOOLBAR_ICONS_BOTH_HORIZ";
			break;
		default: /* LAPIZ_TOOLBAR_SYSTEM */
			str = "LAPIZ_TOOLBAR_SYSTEM";
	}

	lapiz_prefs_manager_set_string (GPM_TOOLBAR_BUTTONS_STYLE,
					str);

}

gboolean
lapiz_prefs_manager_toolbar_buttons_style_can_set (void)
{
	lapiz_debug (DEBUG_PREFS);

	return lapiz_prefs_manager_key_is_writable (GPM_TOOLBAR_BUTTONS_STYLE);

}

/* Statusbar visiblity */
DEFINE_BOOL_PREF (statusbar_visible,
		  GPM_STATUSBAR_VISIBLE)

/* Side Pane visiblity */
DEFINE_BOOL_PREF (side_pane_visible,
		  GPM_SIDE_PANE_VISIBLE)

/* Bottom Panel visiblity */
DEFINE_BOOL_PREF (bottom_panel_visible,
		  GPM_BOTTOM_PANEL_VISIBLE)

/* Print syntax highlighting */
DEFINE_BOOL_PREF (print_syntax_hl,
		  GPM_PRINT_SYNTAX)

/* Print header */
DEFINE_BOOL_PREF (print_header,
		  GPM_PRINT_HEADER)


/* Print Wrap mode */
CtkWrapMode
lapiz_prefs_manager_get_print_wrap_mode (void)
{
	gchar *str;
	CtkWrapMode res;

	lapiz_debug (DEBUG_PREFS);

	str = lapiz_prefs_manager_get_string (GPM_PRINT_WRAP_MODE);

	if (strcmp (str, "CTK_WRAP_NONE") == 0)
		res = CTK_WRAP_NONE;
	else
	{
		if (strcmp (str, "CTK_WRAP_WORD") == 0)
			res = CTK_WRAP_WORD;
		else
			res = CTK_WRAP_CHAR;
	}

	g_free (str);

	return res;
}

void
lapiz_prefs_manager_set_print_wrap_mode (CtkWrapMode pwp)
{
	const gchar *str;

	lapiz_debug (DEBUG_PREFS);

	switch (pwp)
	{
		case CTK_WRAP_NONE:
			str = "CTK_WRAP_NONE";
			break;

		case CTK_WRAP_WORD:
			str = "CTK_WRAP_WORD";
			break;

		default: /* CTK_WRAP_CHAR */
			str = "CTK_WRAP_CHAR";
	}

	lapiz_prefs_manager_set_string (GPM_PRINT_WRAP_MODE, str);
}

gboolean
lapiz_prefs_manager_print_wrap_mode_can_set (void)
{
	lapiz_debug (DEBUG_PREFS);

	return lapiz_prefs_manager_key_is_writable (GPM_PRINT_WRAP_MODE);
}

/* Print line numbers */
DEFINE_INT_PREF (print_line_numbers,
		 GPM_PRINT_LINE_NUMBERS)

/* Printing fonts */
DEFINE_STRING_PREF (print_font_body,
		    GPM_PRINT_FONT_BODY)

static gchar *
lapiz_prefs_manager_get_default_string_value (const gchar *key)
{
	gchar *font = NULL;
	g_settings_delay (lapiz_prefs_manager->settings);
	g_settings_reset (lapiz_prefs_manager->settings, key);
	font = g_settings_get_string (lapiz_prefs_manager->settings, key);
	g_settings_revert (lapiz_prefs_manager->settings);
	return font;
}

gchar *
lapiz_prefs_manager_get_default_print_font_body (void)
{
	return lapiz_prefs_manager_get_default_string_value (GPM_PRINT_FONT_BODY);
}

DEFINE_STRING_PREF (print_font_header,
		    GPM_PRINT_FONT_HEADER)

gchar *
lapiz_prefs_manager_get_default_print_font_header (void)
{
	return lapiz_prefs_manager_get_default_string_value (GPM_PRINT_FONT_HEADER);
}

DEFINE_STRING_PREF (print_font_numbers,
		    GPM_PRINT_FONT_NUMBERS)

gchar *
lapiz_prefs_manager_get_default_print_font_numbers (void)
{
	return lapiz_prefs_manager_get_default_string_value (GPM_PRINT_FONT_NUMBERS);
}

/* Max number of files in "Recent Files" menu.
 * This is configurable only using gsettings, dconf or dconf-editor
 */
gint
lapiz_prefs_manager_get_max_recents (void)
{
	lapiz_debug (DEBUG_PREFS);

	return lapiz_prefs_manager_get_int (GPM_MAX_RECENTS);

}

/* GSettings/GSList utility functions from cafe-panel */

GSList*
lapiz_prefs_manager_get_gslist (GSettings *settings, const gchar *key)
{
    gchar **array;
    GSList *list = NULL;
    gint i;
    array = g_settings_get_strv (settings, key);
    if (array != NULL) {
        for (i = 0; array[i]; i++) {
            list = g_slist_append (list, g_strdup (array[i]));
        }
    }
    g_strfreev (array);
    return list;
}

void
lapiz_prefs_manager_set_gslist (GSettings *settings, const gchar *key, GSList *list)
{
    GArray *array;
    GSList *l;
    array = g_array_new (TRUE, TRUE, sizeof (gchar *));
    for (l = list; l; l = l->next) {
        array = g_array_append_val (array, l->data);
    }
    g_settings_set_strv (settings, key, (const gchar **) array->data);
    g_array_free (array, TRUE);
}


/* Encodings */

static gboolean
data_exists (GSList         *list,
	    const gpointer  data)
{
	while (list != NULL)
	{
      		if (list->data == data)
			return TRUE;

		list = g_slist_next (list);
    	}

  	return FALSE;
}

GSList *
lapiz_prefs_manager_get_auto_detected_encodings (void)
{
	GSList *strings;
	GSList *res = NULL;

	lapiz_debug (DEBUG_PREFS);

	g_return_val_if_fail (lapiz_prefs_manager != NULL, NULL);
	g_return_val_if_fail (lapiz_prefs_manager->settings != NULL, NULL);

	strings = lapiz_prefs_manager_get_gslist (lapiz_prefs_manager->settings, GPM_AUTO_DETECTED_ENCODINGS);

	if (strings != NULL)
	{
		GSList *tmp;
		const LapizEncoding *enc;

		tmp = strings;

		while (tmp)
		{
		      const char *charset = tmp->data;

		      if (strcmp (charset, "CURRENT") == 0)
			      g_get_charset (&charset);

		      g_return_val_if_fail (charset != NULL, NULL);
		      enc = lapiz_encoding_get_from_charset (charset);

		      if (enc != NULL)
		      {
			      if (!data_exists (res, (gpointer)enc))
				      res = g_slist_prepend (res, (gpointer)enc);

		      }

		      tmp = g_slist_next (tmp);
		}

		g_slist_foreach (strings, (GFunc) g_free, NULL);
		g_slist_free (strings);

	 	res = g_slist_reverse (res);
	}

	lapiz_debug_message (DEBUG_PREFS, "Done");

	return res;
}

GSList *
lapiz_prefs_manager_get_shown_in_menu_encodings (void)
{
	GSList *strings;
	GSList *res = NULL;

	lapiz_debug (DEBUG_PREFS);

	g_return_val_if_fail (lapiz_prefs_manager != NULL, NULL);
	g_return_val_if_fail (lapiz_prefs_manager->settings != NULL, NULL);

	strings = lapiz_prefs_manager_get_gslist (lapiz_prefs_manager->settings, GPM_SHOWN_IN_MENU_ENCODINGS);

	if (strings != NULL)
	{
		GSList *tmp;
		const LapizEncoding *enc;

		tmp = strings;

		while (tmp)
		{
		      const char *charset = tmp->data;

		      if (strcmp (charset, "CURRENT") == 0)
			      g_get_charset (&charset);

		      g_return_val_if_fail (charset != NULL, NULL);
		      enc = lapiz_encoding_get_from_charset (charset);

		      if (enc != NULL)
		      {
			      if (!data_exists (res, (gpointer)enc))
				      res = g_slist_prepend (res, (gpointer)enc);
		      }

		      tmp = g_slist_next (tmp);
		}

		g_slist_foreach (strings, (GFunc) g_free, NULL);
		g_slist_free (strings);

	 	res = g_slist_reverse (res);
	}

	return res;
}

void
lapiz_prefs_manager_set_shown_in_menu_encodings (const GSList *encs)
{
	GSList *list = NULL;

	g_return_if_fail (lapiz_prefs_manager != NULL);
	g_return_if_fail (lapiz_prefs_manager->settings != NULL);
	g_return_if_fail (lapiz_prefs_manager_shown_in_menu_encodings_can_set ());

	while (encs != NULL)
	{
		const LapizEncoding *enc;
		const gchar *charset;

		enc = (const LapizEncoding *)encs->data;

		charset = lapiz_encoding_get_charset (enc);
		g_return_if_fail (charset != NULL);

		list = g_slist_prepend (list, (gpointer)charset);

		encs = g_slist_next (encs);
	}

	list = g_slist_reverse (list);

	lapiz_prefs_manager_set_gslist (lapiz_prefs_manager->settings, GPM_SHOWN_IN_MENU_ENCODINGS, list);

	g_slist_free (list);
}

gboolean
lapiz_prefs_manager_shown_in_menu_encodings_can_set (void)
{
	lapiz_debug (DEBUG_PREFS);

	return lapiz_prefs_manager_key_is_writable (GPM_SHOWN_IN_MENU_ENCODINGS);

}

/* Highlight current line */
DEFINE_BOOL_PREF (highlight_current_line,
		  GPM_HIGHLIGHT_CURRENT_LINE)

/* Highlight matching bracket */
DEFINE_BOOL_PREF (bracket_matching,
		  GPM_BRACKET_MATCHING)

/* Display Right Margin */
DEFINE_BOOL_PREF (display_right_margin,
		  GPM_DISPLAY_RIGHT_MARGIN)

/* Right Margin Position */
DEFINE_INT_PREF (right_margin_position,
		 GPM_RIGHT_MARGIN_POSITION)

static CtkSourceSmartHomeEndType
get_smart_home_end_from_string (const gchar *str)
{
	CtkSourceSmartHomeEndType res;

	g_return_val_if_fail (str != NULL, CTK_SOURCE_SMART_HOME_END_AFTER);

	if (strcmp (str, "DISABLED") == 0)
		res = CTK_SOURCE_SMART_HOME_END_DISABLED;
	else if (strcmp (str, "BEFORE") == 0)
		res = CTK_SOURCE_SMART_HOME_END_BEFORE;
	else if (strcmp (str, "ALWAYS") == 0)
		res = CTK_SOURCE_SMART_HOME_END_ALWAYS;
	else
		res = CTK_SOURCE_SMART_HOME_END_AFTER;

	return res;
}

CtkSourceSmartHomeEndType
lapiz_prefs_manager_get_smart_home_end (void)
{
	gchar *str;
	CtkSourceSmartHomeEndType res;

	lapiz_debug (DEBUG_PREFS);

	str = lapiz_prefs_manager_get_string (GPM_SMART_HOME_END);

	res = get_smart_home_end_from_string (str);

	g_free (str);

	return res;
}

void
lapiz_prefs_manager_set_smart_home_end (CtkSourceSmartHomeEndType smart_he)
{
	const gchar *str;

	lapiz_debug (DEBUG_PREFS);

	switch (smart_he)
	{
		case CTK_SOURCE_SMART_HOME_END_DISABLED:
			str = "DISABLED";
			break;

		case CTK_SOURCE_SMART_HOME_END_BEFORE:
			str = "BEFORE";
			break;

		case CTK_SOURCE_SMART_HOME_END_ALWAYS:
			str = "ALWAYS";
			break;

		default: /* CTK_SOURCE_SMART_HOME_END_AFTER */
			str = "AFTER";
	}

	lapiz_prefs_manager_set_string (GPM_WRAP_MODE, str);
}

gboolean
lapiz_prefs_manager_smart_home_end_can_set (void)
{
	lapiz_debug (DEBUG_PREFS);

	return lapiz_prefs_manager_key_is_writable (GPM_SMART_HOME_END);
}

/* Enable syntax highlighting */
DEFINE_BOOL_PREF (enable_syntax_highlighting,
		  GPM_SYNTAX_HL_ENABLE)

/* Enable search highlighting */
DEFINE_BOOL_PREF (enable_search_highlighting,
		  GPM_SEARCH_HIGHLIGHTING_ENABLE)

/* Source style scheme */
DEFINE_STRING_PREF (source_style_scheme,
		    GPM_SOURCE_STYLE_SCHEME)

GSList *
lapiz_prefs_manager_get_writable_vfs_schemes (void)
{
	GSList *strings;

	lapiz_debug (DEBUG_PREFS);

	g_return_val_if_fail (lapiz_prefs_manager != NULL, NULL);
	g_return_val_if_fail (lapiz_prefs_manager->settings != NULL, NULL);

	strings = lapiz_prefs_manager_get_gslist (lapiz_prefs_manager->settings, GPM_WRITABLE_VFS_SCHEMES);

	/* The 'file' scheme is writable by default. */
	strings = g_slist_prepend (strings, g_strdup ("file"));

	lapiz_debug_message (DEBUG_PREFS, "Done");

	return strings;
}

gboolean
lapiz_prefs_manager_get_restore_cursor_position (void)
{
	lapiz_debug (DEBUG_PREFS);

	return lapiz_prefs_manager_get_bool (GPM_RESTORE_CURSOR_POSITION);
}

/* Plugins: we just store/return a list of strings, all the magic has to
 * happen in the plugin engine */

GSList *
lapiz_prefs_manager_get_active_plugins (void)
{
	GSList *plugins;

	lapiz_debug (DEBUG_PREFS);

	g_return_val_if_fail (lapiz_prefs_manager != NULL, NULL);
	g_return_val_if_fail (lapiz_prefs_manager->settings != NULL, NULL);

	plugins = lapiz_prefs_manager_get_gslist (lapiz_prefs_manager->settings, GPM_ACTIVE_PLUGINS);

	return plugins;
}

void
lapiz_prefs_manager_set_active_plugins (const GSList *plugins)
{
	g_return_if_fail (lapiz_prefs_manager != NULL);
	g_return_if_fail (lapiz_prefs_manager->settings != NULL);
	g_return_if_fail (lapiz_prefs_manager_active_plugins_can_set ());

	lapiz_prefs_manager_set_gslist (lapiz_prefs_manager->settings, GPM_ACTIVE_PLUGINS, (GSList *) plugins);
}

gboolean
lapiz_prefs_manager_active_plugins_can_set (void)
{
	lapiz_debug (DEBUG_PREFS);

	return lapiz_prefs_manager_key_is_writable (GPM_ACTIVE_PLUGINS);
}

/* Global Lockdown */

LapizLockdownMask
lapiz_prefs_manager_get_lockdown (void)
{
	guint lockdown = 0;

	if (g_settings_get_boolean (lapiz_prefs_manager->lockdown_settings, GPM_LOCKDOWN_COMMAND_LINE))
		lockdown |= LAPIZ_LOCKDOWN_COMMAND_LINE;

	if (g_settings_get_boolean (lapiz_prefs_manager->lockdown_settings, GPM_LOCKDOWN_PRINTING))
		lockdown |= LAPIZ_LOCKDOWN_PRINTING;

	if (g_settings_get_boolean (lapiz_prefs_manager->lockdown_settings, GPM_LOCKDOWN_PRINT_SETUP))
		lockdown |= LAPIZ_LOCKDOWN_PRINT_SETUP;

	if (g_settings_get_boolean (lapiz_prefs_manager->lockdown_settings, GPM_LOCKDOWN_SAVE_TO_DISK))
		lockdown |= LAPIZ_LOCKDOWN_SAVE_TO_DISK;

	return lockdown;
}

/* enable drawing 'normal' spaces */
DEFINE_ENUM_PREF(draw_spaces,
                 GPM_SPACE_DRAWER_SPACE)

/* enable drawing tabs */
DEFINE_ENUM_PREF(draw_tabs,
                 GPM_SPACE_DRAWER_TAB)

/* enable drawing newline */
DEFINE_BOOL_PREF(draw_newlines,
                 GPM_SPACE_DRAWER_NEWLINE)

/* enable drawing nbsp */
DEFINE_ENUM_PREF(draw_nbsp,
                 GPM_SPACE_DRAWER_NBSP)
