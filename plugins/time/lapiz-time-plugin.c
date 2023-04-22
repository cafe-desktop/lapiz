/*
 * lapiz-time-plugin.c
 *
 * Copyright (C) 2002-2005 - Paolo Maggi
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
 *
 * $Id$
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
#include <time.h>

#include "lapiz-time-plugin.h"
#include <lapiz/lapiz-help.h>

#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gmodule.h>
#include <gio/gio.h>
#include <libbean/bean-activatable.h>
#include <libbean-ctk/bean-ctk-configurable.h>

#include <lapiz/lapiz-window.h>
#include <lapiz/lapiz-debug.h>
#include <lapiz/lapiz-utils.h>

#define MENU_PATH "/MenuBar/EditMenu/EditOps_4"

/* GSettings keys */
#define TIME_SCHEMA			"org.cafe.lapiz.plugins.time"
#define PROMPT_TYPE_KEY		"prompt-type"
#define SELECTED_FORMAT_KEY	"selected-format"
#define CUSTOM_FORMAT_KEY	"custom-format"

#define DEFAULT_CUSTOM_FORMAT "%d/%m/%Y %H:%M:%S"

static const gchar *formats[] =
{
	"%c",
	"%x",
	"%X",
	"%x %X",
	"%Y-%m-%d %H:%M:%S",
	"%a %b %d %H:%M:%S %Z %Y",
	"%a %b %d %H:%M:%S %Y",
	"%a %d %b %Y %H:%M:%S %Z",
	"%a %d %b %Y %H:%M:%S",
	"%d/%m/%Y",
	"%d/%m/%y",
	"%D",
	"%A %d %B %Y",
	"%A %B %d %Y",
	"%Y-%m-%d",
	"%d %B %Y",
	"%B %d, %Y",
	"%A %b %d",
	"%H:%M:%S",
	"%H:%M",
	"%I:%M:%S %p",
	"%I:%M %p",
	"%H.%M.%S",
	"%H.%M",
	"%I.%M.%S %p",
	"%I.%M %p",
	"%d/%m/%Y %H:%M:%S",
	"%d/%m/%y %H:%M:%S",
#if __GLIBC__ >= 2
	"%a, %d %b %Y %H:%M:%S %z",
#endif
	NULL
};

enum
{
	COLUMN_FORMATS = 0,
	COLUMN_INDEX,
	NUM_COLUMNS
};

typedef struct _TimeConfigureDialog TimeConfigureDialog;

struct _TimeConfigureDialog
{
	CtkWidget *content;

	CtkWidget *list;

        /* Radio buttons to indicate what should be done */
        CtkWidget *prompt;
        CtkWidget *use_list;
        CtkWidget *custom;

	CtkWidget *custom_entry;
	CtkWidget *custom_format_example;

	GSettings *settings;
};

typedef struct _ChooseFormatDialog ChooseFormatDialog;

struct _ChooseFormatDialog
{
	CtkWidget *dialog;

	CtkWidget *list;

        /* Radio buttons to indicate what should be done */
        CtkWidget *use_list;
        CtkWidget *custom;

        CtkWidget *custom_entry;
	CtkWidget *custom_format_example;

	/* Info needed for the response handler */
	CtkTextBuffer   *buffer;

	GSettings *settings;
};

typedef enum
{
	PROMPT_SELECTED_FORMAT = 0,	/* Popup dialog with list preselected */
	PROMPT_CUSTOM_FORMAT,		/* Popup dialog with entry preselected */
	USE_SELECTED_FORMAT,		/* Use selected format directly */
	USE_CUSTOM_FORMAT		/* Use custom format directly */
} LapizTimePluginPromptType;

struct _LapizTimePluginPrivate
{
	CtkWidget *window;

	GSettings *settings;

	CtkActionGroup *action_group;
	guint           ui_id;
};

enum {
	PROP_0,
	PROP_OBJECT
};

static void bean_activatable_iface_init (PeasActivatableInterface *iface);
static void bean_ctk_configurable_iface_init (PeasCtkConfigurableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizTimePlugin,
                                lapiz_time_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizTimePlugin)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_TYPE_ACTIVATABLE,
                                                               bean_activatable_iface_init)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_CTK_TYPE_CONFIGURABLE,
                                                               bean_ctk_configurable_iface_init))

static void time_cb (CtkAction *action, LapizTimePlugin *plugin);

static const CtkActionEntry action_entries[] =
{
	{
		"InsertDateAndTime",
		NULL,
		N_("In_sert Date and Time..."),
		NULL,
		N_("Insert current date and time at the cursor position"),
		G_CALLBACK (time_cb)
	},
};

static void
lapiz_time_plugin_init (LapizTimePlugin *plugin)
{
	lapiz_debug_message (DEBUG_PLUGINS, "LapizTimePlugin initializing");

	plugin->priv = lapiz_time_plugin_get_instance_private (plugin);

	plugin->priv->settings = g_settings_new (TIME_SCHEMA);
}

static void
lapiz_time_plugin_finalize (GObject *object)
{
	LapizTimePlugin *plugin = LAPIZ_TIME_PLUGIN (object);

	lapiz_debug_message (DEBUG_PLUGINS, "LapizTimePlugin finalizing");

	g_object_unref (G_OBJECT (plugin->priv->settings));

	G_OBJECT_CLASS (lapiz_time_plugin_parent_class)->finalize (object);
}

static void
lapiz_time_plugin_dispose (GObject *object)
{
	LapizTimePlugin *plugin = LAPIZ_TIME_PLUGIN (object);

	lapiz_debug_message (DEBUG_PLUGINS, "LapizTimePlugin disposing");

	if (plugin->priv->window != NULL)
	{
		g_object_unref (plugin->priv->window);
		plugin->priv->window = NULL;
	}

	if (plugin->priv->action_group)
	{
		g_object_unref (plugin->priv->action_group);
		plugin->priv->action_group = NULL;
	}

	G_OBJECT_CLASS (lapiz_time_plugin_parent_class)->dispose (object);
}

static void
update_ui (LapizTimePluginPrivate *data)
{
	LapizWindow *window;
	LapizView *view;
	CtkAction *action;

	lapiz_debug (DEBUG_PLUGINS);

	window = LAPIZ_WINDOW (data->window);
	view = lapiz_window_get_active_view (window);

	lapiz_debug_message (DEBUG_PLUGINS, "View: %p", view);

	action = ctk_action_group_get_action (data->action_group,
					      "InsertDateAndTime");
	ctk_action_set_sensitive (action,
				  (view != NULL) &&
				  ctk_text_view_get_editable (CTK_TEXT_VIEW (view)));
}

static void
lapiz_time_plugin_activate (PeasActivatable *activatable)
{
	LapizTimePlugin *plugin;
	LapizTimePluginPrivate *data;
	LapizWindow *window;
	CtkUIManager *manager;

	lapiz_debug (DEBUG_PLUGINS);

	plugin = LAPIZ_TIME_PLUGIN (activatable);
	data = plugin->priv;
	window = LAPIZ_WINDOW (data->window);

	manager = lapiz_window_get_ui_manager (window);

	data->action_group = ctk_action_group_new ("LapizTimePluginActions");
	ctk_action_group_set_translation_domain (data->action_group,
						 GETTEXT_PACKAGE);
	ctk_action_group_add_actions (data->action_group,
				      	   action_entries,
				      	   G_N_ELEMENTS (action_entries),
				           plugin);

	ctk_ui_manager_insert_action_group (manager, data->action_group, -1);

	data->ui_id = ctk_ui_manager_new_merge_id (manager);

	ctk_ui_manager_add_ui (manager,
			       data->ui_id,
			       MENU_PATH,
			       "InsertDateAndTime",
			       "InsertDateAndTime",
			       CTK_UI_MANAGER_MENUITEM,
			       FALSE);

	update_ui (data);
}

static void
lapiz_time_plugin_deactivate (PeasActivatable *activatable)
{
	LapizTimePluginPrivate *data;
	LapizWindow *window;
	CtkUIManager *manager;

	lapiz_debug (DEBUG_PLUGINS);

	data = LAPIZ_TIME_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (data->window);

	manager = lapiz_window_get_ui_manager (window);

	ctk_ui_manager_remove_ui (manager, data->ui_id);
	ctk_ui_manager_remove_action_group (manager, data->action_group);
}

static void
lapiz_time_plugin_update_state (PeasActivatable *activatable)
{
	lapiz_debug (DEBUG_PLUGINS);

	update_ui (LAPIZ_TIME_PLUGIN (activatable)->priv);
}

/* whether we should prompt the user or use the specified format */
static LapizTimePluginPromptType
get_prompt_type (LapizTimePlugin *plugin)
{
	LapizTimePluginPromptType prompt_type;

	prompt_type = g_settings_get_enum (plugin->priv->settings,
			        	       PROMPT_TYPE_KEY);

	return prompt_type;
}

static void
set_prompt_type (GSettings *settings,
		 LapizTimePluginPromptType  prompt_type)
{
	if (!g_settings_is_writable (settings,
					   PROMPT_TYPE_KEY))
	{
		return;
	}

	g_settings_set_enum (settings,
				 PROMPT_TYPE_KEY,
				 prompt_type);
}

/* The selected format in the list */
static gchar *
get_selected_format (LapizTimePlugin *plugin)
{
	gchar *sel_format;

	sel_format = g_settings_get_string (plugin->priv->settings,
					      SELECTED_FORMAT_KEY);

	return sel_format ? sel_format : g_strdup (formats [0]);
}

static void
set_selected_format (GSettings *settings,
		     const gchar     *format)
{
	g_return_if_fail (format != NULL);

	if (!g_settings_is_writable (settings,
					   SELECTED_FORMAT_KEY))
	{
		return;
	}

	g_settings_set_string (settings,
				 SELECTED_FORMAT_KEY,
		       		 format);
}

/* the custom format in the entry */
static gchar *
get_custom_format (LapizTimePlugin *plugin)
{
	gchar *format;

	format = g_settings_get_string (plugin->priv->settings,
					  CUSTOM_FORMAT_KEY);

	return format ? format : g_strdup (DEFAULT_CUSTOM_FORMAT);
}

static void
set_custom_format (GSettings *settings,
		   const gchar     *format)
{
	g_return_if_fail (format != NULL);

	if (!g_settings_is_writable (settings,
					   CUSTOM_FORMAT_KEY))
		return;

	g_settings_set_string (settings,
				 CUSTOM_FORMAT_KEY,
		       		 format);
}

static gchar *
get_time (const gchar* format)
{
  	gchar *out = NULL;
	gchar *out_utf8 = NULL;
  	time_t clock;
  	struct tm *now;
  	size_t out_length = 0;
	gchar *locale_format;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (format != NULL, NULL);

	if (strlen (format) == 0)
		return g_strdup (" ");

	locale_format = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
	if (locale_format == NULL)
		return g_strdup (" ");

  	clock = time (NULL);
  	now = localtime (&clock);

	do
	{
		out_length += 255;
		out = g_realloc (out, out_length);
	}
  	while (strftime (out, out_length, locale_format, now) == 0);

	g_free (locale_format);

	if (g_utf8_validate (out, -1, NULL))
	{
		out_utf8 = out;
	}
	else
	{
		out_utf8 = g_locale_to_utf8 (out, -1, NULL, NULL, NULL);
		g_free (out);

		if (out_utf8 == NULL)
			out_utf8 = g_strdup (" ");
	}

  	return out_utf8;
}

static void
configure_dialog_destroyed (CtkWidget *widget,
                            gpointer   data)
{
	TimeConfigureDialog *dialog = (TimeConfigureDialog *) data;

	lapiz_debug (DEBUG_PLUGINS);

	g_object_unref (dialog->settings);
	g_slice_free (TimeConfigureDialog, data);
}

static void
choose_format_dialog_destroyed (CtkWidget *widget,
                                gpointer   data)
{
	lapiz_debug (DEBUG_PLUGINS);

	g_slice_free (ChooseFormatDialog, data);
}

static CtkTreeModel *
create_model (CtkWidget       *listview,
	      const gchar     *sel_format,
	      LapizTimePlugin *plugin)
{
	gint i = 0;
	CtkListStore *store;
	CtkTreeSelection *selection;
	CtkTreeIter iter;

	lapiz_debug (DEBUG_PLUGINS);

	/* create list store */
	store = ctk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

	/* Set tree view model*/
	ctk_tree_view_set_model (CTK_TREE_VIEW (listview),
				 CTK_TREE_MODEL (store));
	g_object_unref (G_OBJECT (store));

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (listview));
	g_return_val_if_fail (selection != NULL, CTK_TREE_MODEL (store));

	/* there should always be one line selected */
	ctk_tree_selection_set_mode (selection, CTK_SELECTION_BROWSE);

	/* add data to the list store */
	while (formats[i] != NULL)
	{
		gchar *str;

		str = get_time (formats[i]);

		lapiz_debug_message (DEBUG_PLUGINS, "%d : %s", i, str);
		ctk_list_store_append (store, &iter);
		ctk_list_store_set (store, &iter,
				    COLUMN_FORMATS, str,
				    COLUMN_INDEX, i,
				    -1);
		g_free (str);

		if (sel_format && strcmp (formats[i], sel_format) == 0)
			ctk_tree_selection_select_iter (selection, &iter);

		++i;
	}

	/* fall back to select the first iter */
	if (!ctk_tree_selection_get_selected (selection, NULL, NULL))
	{
		ctk_tree_model_get_iter_first (CTK_TREE_MODEL (store), &iter);
		ctk_tree_selection_select_iter (selection, &iter);
	}

	return CTK_TREE_MODEL (store);
}

static void
scroll_to_selected (CtkTreeView *tree_view)
{
	CtkTreeModel *model;
	CtkTreeSelection *selection;
	CtkTreeIter iter;

	lapiz_debug (DEBUG_PLUGINS);

	model = ctk_tree_view_get_model (tree_view);
	g_return_if_fail (model != NULL);

	/* Scroll to selected */
	selection = ctk_tree_view_get_selection (tree_view);
	g_return_if_fail (selection != NULL);

	if (ctk_tree_selection_get_selected (selection, NULL, &iter))
	{
		CtkTreePath* path;

		path = ctk_tree_model_get_path (model, &iter);
		g_return_if_fail (path != NULL);

		ctk_tree_view_scroll_to_cell (tree_view,
					      path, NULL, TRUE, 1.0, 0.0);
		ctk_tree_path_free (path);
	}
}

static void
create_formats_list (CtkWidget       *listview,
		     const gchar     *sel_format,
		     LapizTimePlugin *plugin)
{
	CtkTreeViewColumn *column;
	CtkCellRenderer *cell;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_if_fail (listview != NULL);
	g_return_if_fail (sel_format != NULL);

	/* the Available formats column */
	cell = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (
			_("Available formats"),
			cell,
			"text", COLUMN_FORMATS,
			NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (listview), column);

	/* Create model, it also add model to the tree view */
	create_model (listview, sel_format, plugin);

	g_signal_connect (listview,
			  "realize",
			  G_CALLBACK (scroll_to_selected),
			  NULL);

	ctk_widget_show (listview);
}

static void
updated_custom_format_example (CtkEntry *format_entry,
			       CtkLabel *format_example)
{
	const gchar *format;
	gchar *time;
	gchar *str;
	gchar *escaped_time;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_if_fail (CTK_IS_ENTRY (format_entry));
	g_return_if_fail (CTK_IS_LABEL (format_example));

	format = ctk_entry_get_text (format_entry);

	time = get_time (format);
	escaped_time = g_markup_escape_text (time, -1);

	str = g_strdup_printf ("<span size=\"small\">%s</span>", escaped_time);

	ctk_label_set_markup (format_example, str);

	g_free (escaped_time);
	g_free (time);
	g_free (str);
}

static void
choose_format_dialog_button_toggled (CtkToggleButton *button,
				     ChooseFormatDialog *dialog)
{
	lapiz_debug (DEBUG_PLUGINS);

	if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->custom)))
	{
		ctk_widget_set_sensitive (dialog->list, FALSE);
		ctk_widget_set_sensitive (dialog->custom_entry, TRUE);
		ctk_widget_set_sensitive (dialog->custom_format_example, TRUE);

		return;
	}

	if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->use_list)))
	{
		ctk_widget_set_sensitive (dialog->list, TRUE);
		ctk_widget_set_sensitive (dialog->custom_entry, FALSE);
		ctk_widget_set_sensitive (dialog->custom_format_example, FALSE);

		return;
	}
}

static void
configure_dialog_button_toggled (CtkToggleButton *button, TimeConfigureDialog *dialog)
{
	lapiz_debug (DEBUG_PLUGINS);

	if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->custom)))
	{
		ctk_widget_set_sensitive (dialog->list, FALSE);
		ctk_widget_set_sensitive (dialog->custom_entry, TRUE);
		ctk_widget_set_sensitive (dialog->custom_format_example, TRUE);

		set_prompt_type (dialog->settings, USE_CUSTOM_FORMAT);
		return;
	}

	if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->use_list)))
	{
		ctk_widget_set_sensitive (dialog->list, TRUE);
		ctk_widget_set_sensitive (dialog->custom_entry, FALSE);
		ctk_widget_set_sensitive (dialog->custom_format_example, FALSE);

		set_prompt_type (dialog->settings, USE_SELECTED_FORMAT);
		return;
	}

	if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->prompt)))
	{
		ctk_widget_set_sensitive (dialog->list, FALSE);
		ctk_widget_set_sensitive (dialog->custom_entry, FALSE);
		ctk_widget_set_sensitive (dialog->custom_format_example, FALSE);

		set_prompt_type (dialog->settings, PROMPT_SELECTED_FORMAT);
		return;
	}
}

static gint
get_format_from_list (CtkWidget *listview)
{
	CtkTreeModel *model;
	CtkTreeSelection *selection;
	CtkTreeIter iter;

	lapiz_debug (DEBUG_PLUGINS);

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (listview));
	g_return_val_if_fail (model != NULL, 0);

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (listview));
	g_return_val_if_fail (selection != NULL, 0);

	if (ctk_tree_selection_get_selected (selection, NULL, &iter))
	{
	        gint selected_value;

		ctk_tree_model_get (model, &iter, COLUMN_INDEX, &selected_value, -1);

		lapiz_debug_message (DEBUG_PLUGINS, "Sel value: %d", selected_value);

	        return selected_value;
	}

	g_return_val_if_reached (0);
}

static void
configure_dialog_selection_changed (CtkTreeSelection *selection,
                                    TimeConfigureDialog *dialog)
{
	gint sel_format;

	sel_format = get_format_from_list (dialog->list);
	set_selected_format (dialog->settings, formats[sel_format]);
}

static TimeConfigureDialog *
get_configure_dialog (LapizTimePlugin *plugin)
{
	TimeConfigureDialog *dialog = NULL;
	CtkTreeSelection *selection;
	gchar *data_dir;
	gchar *ui_file;
	CtkWidget *viewport;
	LapizTimePluginPromptType prompt_type;
	gchar *sf;
	CtkWidget *error_widget;
	gboolean ret;
	gchar *root_objects[] = {
		"time_dialog_content",
		NULL
	};

	lapiz_debug (DEBUG_PLUGINS);

	dialog = g_slice_new (TimeConfigureDialog);
	dialog->settings = g_object_ref (plugin->priv->settings);

	data_dir = bean_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	ui_file = g_build_filename (data_dir, "lapiz-time-setup-dialog.ui", NULL);
	ret = lapiz_utils_get_ui_objects (ui_file,
					  root_objects,
					  &error_widget,
					  "time_dialog_content", &dialog->content,
					  "formats_viewport", &viewport,
					  "formats_tree", &dialog->list,
					  "always_prompt", &dialog->prompt,
					  "never_prompt", &dialog->use_list,
					  "use_custom", &dialog->custom,
					  "custom_entry", &dialog->custom_entry,
					  "custom_format_example", &dialog->custom_format_example,
					  NULL);

	g_free (data_dir);
	g_free (ui_file);

	if (!ret)
	{
		return NULL;
	}

	sf = get_selected_format (plugin);
	create_formats_list (dialog->list, sf, plugin);
	g_free (sf);

	prompt_type = get_prompt_type (plugin);

	g_settings_bind (dialog->settings,
	                 CUSTOM_FORMAT_KEY,
	                 dialog->custom_entry,
	                 "text",
	                 G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

        if (prompt_type == USE_CUSTOM_FORMAT)
        {
	        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->custom), TRUE);

		ctk_widget_set_sensitive (dialog->list, FALSE);
		ctk_widget_set_sensitive (dialog->custom_entry, TRUE);
		ctk_widget_set_sensitive (dialog->custom_format_example, TRUE);
        }
        else if (prompt_type == USE_SELECTED_FORMAT)
        {
	        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->use_list), TRUE);

		ctk_widget_set_sensitive (dialog->list, TRUE);
		ctk_widget_set_sensitive (dialog->custom_entry, FALSE);
		ctk_widget_set_sensitive (dialog->custom_format_example, FALSE);
        }
        else
        {
	        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->prompt), TRUE);

		ctk_widget_set_sensitive (dialog->list, FALSE);
		ctk_widget_set_sensitive (dialog->custom_entry, FALSE);
		ctk_widget_set_sensitive (dialog->custom_format_example, FALSE);
        }

	updated_custom_format_example (CTK_ENTRY (dialog->custom_entry),
			CTK_LABEL (dialog->custom_format_example));

	/* setup a window of a sane size. */
	ctk_widget_set_size_request (CTK_WIDGET (viewport), 10, 200);

	g_signal_connect (dialog->custom,
			  "toggled",
			  G_CALLBACK (configure_dialog_button_toggled),
			  dialog);
   	g_signal_connect (dialog->prompt,
			  "toggled",
			  G_CALLBACK (configure_dialog_button_toggled),
			  dialog);
	g_signal_connect (dialog->use_list,
			  "toggled",
			  G_CALLBACK (configure_dialog_button_toggled),
			  dialog);
	g_signal_connect (dialog->content,
			  "destroy",
			  G_CALLBACK (configure_dialog_destroyed),
			  dialog);
	g_signal_connect (dialog->custom_entry,
			  "changed",
			  G_CALLBACK (updated_custom_format_example),
			  dialog->custom_format_example);

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dialog->list));
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (configure_dialog_selection_changed),
			  dialog);

	return dialog;
}

static void
real_insert_time (CtkTextBuffer *buffer,
		  const gchar   *the_time)
{
	lapiz_debug_message (DEBUG_PLUGINS, "Insert: %s", the_time);

	ctk_text_buffer_begin_user_action (buffer);

	ctk_text_buffer_insert_at_cursor (buffer, the_time, -1);
	ctk_text_buffer_insert_at_cursor (buffer, " ", -1);

	ctk_text_buffer_end_user_action (buffer);
}

static void
choose_format_dialog_row_activated (CtkTreeView        *list,
				    CtkTreePath        *path,
				    CtkTreeViewColumn  *column,
				    ChooseFormatDialog *dialog)
{
	gint sel_format;
	gchar *the_time;

	sel_format = get_format_from_list (dialog->list);
	the_time = get_time (formats[sel_format]);

	set_prompt_type (dialog->settings, PROMPT_SELECTED_FORMAT);
	set_selected_format (dialog->settings, formats[sel_format]);

	g_return_if_fail (the_time != NULL);

	real_insert_time (dialog->buffer, the_time);

	g_free (the_time);
}

static ChooseFormatDialog *
get_choose_format_dialog (CtkWindow                 *parent,
			  LapizTimePluginPromptType  prompt_type,
			  LapizTimePlugin           *plugin)
{
	ChooseFormatDialog *dialog;
	gchar *data_dir;
	gchar *ui_file;
	CtkWidget *error_widget;
	gboolean ret;
	gchar *sf, *cf;
	CtkWindowGroup *wg = NULL;

	if (parent != NULL)
		wg = ctk_window_get_group (parent);

	dialog = g_slice_new (ChooseFormatDialog);
	dialog->settings = plugin->priv->settings;

	data_dir = bean_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	ui_file = g_build_filename (data_dir, "lapiz-time-dialog.ui", NULL);
	ret = lapiz_utils_get_ui_objects (ui_file,
					  NULL,
					  &error_widget,
					  "choose_format_dialog", &dialog->dialog,
					  "choice_list", &dialog->list,
					  "use_sel_format_radiobutton", &dialog->use_list,
					  "use_custom_radiobutton", &dialog->custom,
					  "custom_entry", &dialog->custom_entry,
					  "custom_format_example", &dialog->custom_format_example,
					  NULL);

	g_free (data_dir);
	g_free (ui_file);

	if (!ret)
	{
		CtkWidget *err_dialog;

		err_dialog = ctk_dialog_new ();
		ctk_window_set_transient_for (CTK_WINDOW (err_dialog), parent);
		ctk_window_set_modal (CTK_WINDOW (err_dialog), TRUE);
		ctk_window_set_destroy_with_parent (CTK_WINDOW (err_dialog), TRUE);
		lapiz_dialog_add_button (CTK_DIALOG (err_dialog), _("_OK"), "ctk-ok", CTK_RESPONSE_ACCEPT);

		if (wg != NULL)
			ctk_window_group_add_window (wg, CTK_WINDOW (err_dialog));

		ctk_window_set_resizable (CTK_WINDOW (err_dialog), FALSE);
		ctk_dialog_set_default_response (CTK_DIALOG (err_dialog), CTK_RESPONSE_OK);

		ctk_container_add (CTK_CONTAINER (ctk_dialog_get_content_area (CTK_DIALOG (err_dialog))),
				   error_widget);

		g_signal_connect (G_OBJECT (err_dialog),
				  "response",
				  G_CALLBACK (ctk_widget_destroy),
				  NULL);

		ctk_widget_show_all (err_dialog);

		return NULL;
	}

	ctk_window_group_add_window (wg,
			     	     CTK_WINDOW (dialog->dialog));
	ctk_window_set_transient_for (CTK_WINDOW (dialog->dialog), parent);
	ctk_window_set_modal (CTK_WINDOW (dialog->dialog), TRUE);

	sf = get_selected_format (plugin);
	create_formats_list (dialog->list, sf, plugin);
	g_free (sf);

	cf = get_custom_format (plugin);
     	ctk_entry_set_text (CTK_ENTRY(dialog->custom_entry), cf);
	g_free (cf);

	updated_custom_format_example (CTK_ENTRY (dialog->custom_entry),
				       CTK_LABEL (dialog->custom_format_example));

	if (prompt_type == PROMPT_CUSTOM_FORMAT)
	{
        	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->custom), TRUE);

		ctk_widget_set_sensitive (dialog->list, FALSE);
		ctk_widget_set_sensitive (dialog->custom_entry, TRUE);
		ctk_widget_set_sensitive (dialog->custom_format_example, TRUE);
	}
	else if (prompt_type == PROMPT_SELECTED_FORMAT)
	{
		ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->use_list), TRUE);

		ctk_widget_set_sensitive (dialog->list, TRUE);
		ctk_widget_set_sensitive (dialog->custom_entry, FALSE);
		ctk_widget_set_sensitive (dialog->custom_format_example, FALSE);
	}
	else
	{
		g_return_val_if_reached (NULL);
	}

	/* setup a window of a sane size. */
	ctk_widget_set_size_request (dialog->list, 10, 200);

	ctk_dialog_set_default_response (CTK_DIALOG (dialog->dialog),
					 CTK_RESPONSE_OK);

	g_signal_connect (dialog->custom,
			  "toggled",
			  G_CALLBACK (choose_format_dialog_button_toggled),
			  dialog);
	g_signal_connect (dialog->use_list,
			  "toggled",
			  G_CALLBACK (choose_format_dialog_button_toggled),
			  dialog);
	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (choose_format_dialog_destroyed),
			  dialog);
	g_signal_connect (dialog->custom_entry,
			  "changed",
			  G_CALLBACK (updated_custom_format_example),
			  dialog->custom_format_example);
	g_signal_connect (dialog->list,
			  "row_activated",
			  G_CALLBACK (choose_format_dialog_row_activated),
			  dialog);

	ctk_window_set_resizable (CTK_WINDOW (dialog->dialog), FALSE);

	return dialog;
}

static void
choose_format_dialog_response_cb (CtkWidget          *widget,
				  gint                response,
				  ChooseFormatDialog *dialog)
{
	switch (response)
	{
		case CTK_RESPONSE_HELP:
		{
			lapiz_debug_message (DEBUG_PLUGINS, "CTK_RESPONSE_HELP");
			lapiz_help_display (CTK_WINDOW (widget),
					    NULL,
					    "lapiz-insert-date-time-plugin");
			break;
		}
		case CTK_RESPONSE_OK:
		{
			gchar *the_time;

			lapiz_debug_message (DEBUG_PLUGINS, "CTK_RESPONSE_OK");

			/* Get the user's chosen format */
			if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->use_list)))
			{
				gint sel_format;

				sel_format = get_format_from_list (dialog->list);
				the_time = get_time (formats[sel_format]);

				set_prompt_type (dialog->settings, PROMPT_SELECTED_FORMAT);
				set_selected_format (dialog->settings, formats[sel_format]);
			}
			else
			{
				const gchar *format;

				format = ctk_entry_get_text (CTK_ENTRY (dialog->custom_entry));
				the_time = get_time (format);

				set_prompt_type (dialog->settings, PROMPT_CUSTOM_FORMAT);
				set_custom_format (dialog->settings, format);
			}

			g_return_if_fail (the_time != NULL);

			real_insert_time (dialog->buffer, the_time);
			g_free (the_time);

			ctk_widget_destroy (dialog->dialog);
			break;
		}
		case CTK_RESPONSE_CANCEL:
			lapiz_debug_message (DEBUG_PLUGINS, "CTK_RESPONSE_CANCEL");
			ctk_widget_destroy (dialog->dialog);
	}
}

static void
time_cb (CtkAction  *action,
	 LapizTimePlugin *plugin)
{
	LapizWindow *window;
	CtkTextBuffer *buffer;
	gchar *the_time = NULL;
	LapizTimePluginPromptType prompt_type;

	lapiz_debug (DEBUG_PLUGINS);

	window = LAPIZ_WINDOW (plugin->priv->window);
	buffer = CTK_TEXT_BUFFER (lapiz_window_get_active_document (window));
	g_return_if_fail (buffer != NULL);

	prompt_type = get_prompt_type (plugin);

        if (prompt_type == USE_CUSTOM_FORMAT)
        {
		gchar *cf = get_custom_format (plugin);
	        the_time = get_time (cf);
		g_free (cf);
	}
        else if (prompt_type == USE_SELECTED_FORMAT)
        {
		gchar *sf = get_selected_format (plugin);
	        the_time = get_time (sf);
		g_free (sf);
	}
        else
        {
		ChooseFormatDialog *dialog;

		dialog = get_choose_format_dialog (CTK_WINDOW (window),
						   prompt_type,
						   plugin);
		if (dialog != NULL)
		{
			dialog->buffer = buffer;
			dialog->settings = plugin->priv->settings;

			g_signal_connect (dialog->dialog,
					  "response",
					  G_CALLBACK (choose_format_dialog_response_cb),
					  dialog);

			ctk_widget_show (CTK_WIDGET (dialog->dialog));
		}

		return;
	}

	g_return_if_fail (the_time != NULL);

	real_insert_time (buffer, the_time);

	g_free (the_time);
}

static CtkWidget *
lapiz_time_plugin_create_configure_widget (PeasCtkConfigurable *configurable)
{
	TimeConfigureDialog *dialog;

	dialog = get_configure_dialog (LAPIZ_TIME_PLUGIN (configurable));

	return dialog->content;
}

static void
lapiz_time_plugin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	LapizTimePlugin *plugin = LAPIZ_TIME_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_OBJECT:
			plugin->priv->window = CTK_WIDGET (g_value_dup_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_time_plugin_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	LapizTimePlugin *plugin = LAPIZ_TIME_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_OBJECT:
			g_value_set_object (value, plugin->priv->window);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_time_plugin_class_init (LapizTimePluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_time_plugin_finalize;
	object_class->dispose = lapiz_time_plugin_dispose;
	object_class->set_property = lapiz_time_plugin_set_property;
	object_class->get_property = lapiz_time_plugin_get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");
}

static void
lapiz_time_plugin_class_finalize (LapizTimePluginClass *klass)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
bean_activatable_iface_init (PeasActivatableInterface *iface)
{
	iface->activate = lapiz_time_plugin_activate;
	iface->deactivate = lapiz_time_plugin_deactivate;
	iface->update_state = lapiz_time_plugin_update_state;
}

static void
bean_ctk_configurable_iface_init (PeasCtkConfigurableInterface *iface)
{
	iface->create_configure_widget = lapiz_time_plugin_create_configure_widget;
}

G_MODULE_EXPORT void
bean_register_types (PeasObjectModule *module)
{
	lapiz_time_plugin_register_type (G_TYPE_MODULE (module));

	bean_object_module_register_extension_type (module,
	                                            PEAS_TYPE_ACTIVATABLE,
	                                            LAPIZ_TYPE_TIME_PLUGIN);

	bean_object_module_register_extension_type (module,
	                                            PEAS_CTK_TYPE_CONFIGURABLE,
	                                            LAPIZ_TYPE_TIME_PLUGIN);
}
