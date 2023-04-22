/*
 * lapiz-sort-plugin.c
 *
 * Original author: Carlo Borreo <borreo@softhome.net>
 * Ported to Lapiz2 by Lee Mallabone <cafe@fonicmonkey.net>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "lapiz-sort-plugin.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <libbean/bean-activatable.h>

#include <lapiz/lapiz-window.h>
#include <lapiz/lapiz-debug.h>
#include <lapiz/lapiz-utils.h>
#include <lapiz/lapiz-help.h>

#define MENU_PATH "/MenuBar/EditMenu/EditOps_6"

static void bean_activatable_iface_init (PeasActivatableInterface *iface);

enum {
	PROP_0,
	PROP_OBJECT
};

typedef struct
{
	CtkWidget *dialog;
	CtkWidget *col_num_spinbutton;
	CtkWidget *reverse_order_checkbutton;
	CtkWidget *ignore_case_checkbutton;
	CtkWidget *remove_dups_checkbutton;

	LapizDocument *doc;

	CtkTextIter start, end; /* selection */
} SortDialog;

struct _LapizSortPluginPrivate
{
	CtkWidget *window;

	CtkActionGroup *ui_action_group;
	guint ui_id;
};

typedef struct
{
	gboolean ignore_case;
	gboolean reverse_order;
	gboolean remove_duplicates;
	gint starting_column;
} SortInfo;

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizSortPlugin,
                                lapiz_sort_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizSortPlugin)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_TYPE_ACTIVATABLE,
                                                               bean_activatable_iface_init))

static void sort_cb (CtkAction *action, LapizSortPlugin *plugin);
static void sort_real (SortDialog *dialog);

static const CtkActionEntry action_entries[] =
{
	{ "Sort",
	  "view-sort-ascending",
	  N_("S_ort..."),
	  NULL,
	  N_("Sort the current document or selection"),
	  G_CALLBACK (sort_cb) }
};

static void
sort_dialog_destroy (GObject *obj,
		     gpointer  dialog_pointer)
{
	lapiz_debug (DEBUG_PLUGINS);

	g_slice_free (SortDialog, dialog_pointer);
}

static void
sort_dialog_response_handler (CtkDialog  *widget,
			      gint       res_id,
			      SortDialog *dialog)
{
	lapiz_debug (DEBUG_PLUGINS);

	switch (res_id)
	{
		case CTK_RESPONSE_OK:
			sort_real (dialog);
			ctk_widget_destroy (dialog->dialog);
			break;

		case CTK_RESPONSE_HELP:
			lapiz_help_display (CTK_WINDOW (widget),
					    NULL,
					    "lapiz-sort-plugin");
			break;

		case CTK_RESPONSE_CANCEL:
			ctk_widget_destroy (dialog->dialog);
			break;
	}
}

/* NOTE: we store the current selection in the dialog since focusing
 * the text field (like the combo box) looses the documnent selection.
 * Storing the selection ONLY works because the dialog is modal */
static void
get_current_selection (LapizWindow *window, SortDialog *dialog)
{
	LapizDocument *doc;

	lapiz_debug (DEBUG_PLUGINS);

	doc = lapiz_window_get_active_document (window);

	if (!ctk_text_buffer_get_selection_bounds (CTK_TEXT_BUFFER (doc),
						   &dialog->start,
						   &dialog->end))
	{
		/* No selection, get the whole document. */
		ctk_text_buffer_get_bounds (CTK_TEXT_BUFFER (doc),
					    &dialog->start,
					    &dialog->end);
	}
}

static SortDialog *
get_sort_dialog (LapizSortPlugin *plugin)
{
	LapizWindow *window;
	SortDialog *dialog;
	CtkWidget *error_widget;
	gboolean ret;
	gchar *data_dir;
	gchar *ui_file;

	lapiz_debug (DEBUG_PLUGINS);

	window = LAPIZ_WINDOW (plugin->priv->window);

	dialog = g_slice_new (SortDialog);

	data_dir = bean_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	ui_file = g_build_filename (data_dir, "sort.ui", NULL);
	g_free (data_dir);
	ret = lapiz_utils_get_ui_objects (ui_file,
					  NULL,
					  &error_widget,
					  "sort_dialog", &dialog->dialog,
					  "reverse_order_checkbutton", &dialog->reverse_order_checkbutton,
					  "col_num_spinbutton", &dialog->col_num_spinbutton,
					  "ignore_case_checkbutton", &dialog->ignore_case_checkbutton,
					  "remove_dups_checkbutton", &dialog->remove_dups_checkbutton,
					  NULL);
	g_free (ui_file);

	if (!ret)
	{
		const gchar *err_message;

		err_message = ctk_label_get_label (CTK_LABEL (error_widget));
		lapiz_warning (CTK_WINDOW (window),
			       "%s", err_message);

		g_slice_free (SortDialog, dialog);
		ctk_widget_destroy (error_widget);

		return NULL;
	}

	ctk_dialog_set_default_response (CTK_DIALOG (dialog->dialog),
					 CTK_RESPONSE_OK);

	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (sort_dialog_destroy),
			  dialog);

	g_signal_connect (dialog->dialog,
			  "response",
			  G_CALLBACK (sort_dialog_response_handler),
			  dialog);

	get_current_selection (window, dialog);

	return dialog;
}

static void
sort_cb (CtkAction  *action,
	 LapizSortPlugin *plugin)
{
	LapizWindow *window;
	LapizDocument *doc;
	CtkWindowGroup *wg;
	SortDialog *dialog;

	lapiz_debug (DEBUG_PLUGINS);

	window = LAPIZ_WINDOW (plugin->priv->window);

	doc = lapiz_window_get_active_document (window);
	g_return_if_fail (doc != NULL);

	dialog = get_sort_dialog (plugin);
	g_return_if_fail (dialog != NULL);

	wg = lapiz_window_get_group (window);
	ctk_window_group_add_window (wg,
				     CTK_WINDOW (dialog->dialog));

	dialog->doc = doc;

	ctk_window_set_transient_for (CTK_WINDOW (dialog->dialog),
				      CTK_WINDOW (window));

	ctk_window_set_modal (CTK_WINDOW (dialog->dialog),
			      TRUE);

	ctk_widget_show (CTK_WIDGET (dialog->dialog));
}

/* Compares two strings for the sorting algorithm. Uses the UTF-8 processing
 * functions in GLib to be as correct as possible.*/
static gint
compare_algorithm (gconstpointer s1,
		   gconstpointer s2,
		   gpointer	 data)
{
	gint length1, length2;
	gint ret;
	gchar *string1, *string2;
	gchar *substring1, *substring2;
	gchar *key1, *key2;
	SortInfo *sort_info;

	lapiz_debug (DEBUG_PLUGINS);

	sort_info = (SortInfo *) data;
	g_return_val_if_fail (sort_info != NULL, -1);

	if (!sort_info->ignore_case)
	{
		string1 = *((gchar **) s1);
		string2 = *((gchar **) s2);
	}
	else
	{
		string1 = g_utf8_casefold (*((gchar **) s1), -1);
		string2 = g_utf8_casefold (*((gchar **) s2), -1);
	}

	length1 = g_utf8_strlen (string1, -1);
	length2 = g_utf8_strlen (string2, -1);

	if ((length1 < sort_info->starting_column) &&
	    (length2 < sort_info->starting_column))
	{
		ret = 0;
	}
	else if (length1 < sort_info->starting_column)
	{
		ret = -1;
	}
	else if (length2 < sort_info->starting_column)
	{
		ret = 1;
	}
	else if (sort_info->starting_column < 1)
	{
		key1 = g_utf8_collate_key (string1, -1);
		key2 = g_utf8_collate_key (string2, -1);
		ret = strcmp (key1, key2);

		g_free (key1);
		g_free (key2);
	}
	else
	{
		/* A character column offset is required, so figure out
		 * the correct offset into the UTF-8 string. */
		substring1 = g_utf8_offset_to_pointer (string1, sort_info->starting_column);
		substring2 = g_utf8_offset_to_pointer (string2, sort_info->starting_column);

		key1 = g_utf8_collate_key (substring1, -1);
		key2 = g_utf8_collate_key (substring2, -1);
		ret = strcmp (key1, key2);

		g_free (key1);
		g_free (key2);
	}

	/* Do the necessary cleanup. */
	if (sort_info->ignore_case)
	{
		g_free (string1);
		g_free (string2);
	}

	if (sort_info->reverse_order)
	{
		ret = -1 * ret;
	}

	return ret;
}

static gchar *
get_line_slice (CtkTextBuffer *buf,
		gint           line)
{
	CtkTextIter start, end;
	char *ret;

	ctk_text_buffer_get_iter_at_line (buf, &start, line);
	end = start;

	if (!ctk_text_iter_ends_line (&start))
		ctk_text_iter_forward_to_line_end (&end);

	ret= ctk_text_buffer_get_slice (buf,
					  &start,
					  &end,
					  TRUE);

	g_assert (ret != NULL);

	return ret;
}

static void
sort_real (SortDialog *dialog)
{
	LapizDocument *doc;
	CtkTextIter start, end;
	gint start_line, end_line;
	gint i;
	gchar *last_row = NULL;
	gint num_lines;
	gchar **lines;
	SortInfo *sort_info;

	lapiz_debug (DEBUG_PLUGINS);

	doc = dialog->doc;
	g_return_if_fail (doc != NULL);

	sort_info = g_new0 (SortInfo, 1);
	sort_info->ignore_case = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->ignore_case_checkbutton));
	sort_info->reverse_order = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->reverse_order_checkbutton));
	sort_info->remove_duplicates = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->remove_dups_checkbutton));
	sort_info->starting_column = ctk_spin_button_get_value_as_int (CTK_SPIN_BUTTON (dialog->col_num_spinbutton)) - 1;

	start = dialog->start;
	end = dialog->end;
	start_line = ctk_text_iter_get_line (&start);
	end_line = ctk_text_iter_get_line (&end);

	/* if we are at line start our last line is the previus one.
	 * Otherwise the last line is the current one but we try to
	 * move the iter after the line terminator */
	if (ctk_text_iter_get_line_offset (&end) == 0)
		end_line = MAX (start_line, end_line - 1);
	else
		ctk_text_iter_forward_line (&end);

	num_lines = end_line - start_line + 1;
	lines = g_new0 (gchar *, num_lines + 1);

	lapiz_debug_message (DEBUG_PLUGINS, "Building list...");

	for (i = 0; i < num_lines; i++)
	{
		lines[i] = get_line_slice (CTK_TEXT_BUFFER (doc), start_line + i);
	}

	lines[num_lines] = NULL;

	lapiz_debug_message (DEBUG_PLUGINS, "Sort list...");

	g_qsort_with_data (lines,
			   num_lines,
			   sizeof (gpointer),
			   compare_algorithm,
			   sort_info);

	lapiz_debug_message (DEBUG_PLUGINS, "Rebuilding document...");

	ctk_source_buffer_begin_not_undoable_action (CTK_SOURCE_BUFFER (doc));

	ctk_text_buffer_delete (CTK_TEXT_BUFFER (doc),
				&start,
				&end);

	for (i = 0; i < num_lines; i++)
	{
		if (sort_info->remove_duplicates &&
		    last_row != NULL &&
		    (strcmp (last_row, lines[i]) == 0))
			continue;

		ctk_text_buffer_insert (CTK_TEXT_BUFFER (doc),
					&start,
					lines[i],
					-1);

		if (i < (num_lines - 1))
			ctk_text_buffer_insert (CTK_TEXT_BUFFER (doc),
						&start,
						"\n",
						-1);

		last_row = lines[i];
	}

	ctk_source_buffer_end_not_undoable_action (CTK_SOURCE_BUFFER (doc));

	g_strfreev (lines);
	g_free (sort_info);

	lapiz_debug_message (DEBUG_PLUGINS, "Done.");
}

static void
lapiz_sort_plugin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	LapizSortPlugin *plugin = LAPIZ_SORT_PLUGIN (object);

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
lapiz_sort_plugin_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	LapizSortPlugin *plugin = LAPIZ_SORT_PLUGIN (object);

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
update_ui (LapizSortPluginPrivate *data)
{
	LapizWindow *window;
	LapizView *view;

	lapiz_debug (DEBUG_PLUGINS);

	window = LAPIZ_WINDOW (data->window);
	view = lapiz_window_get_active_view (window);

	ctk_action_group_set_sensitive (data->ui_action_group,
					(view != NULL) &&
					ctk_text_view_get_editable (CTK_TEXT_VIEW (view)));
}

static void
lapiz_sort_plugin_activate (PeasActivatable *activatable)
{
	LapizSortPlugin *plugin;
	LapizSortPluginPrivate *data;
	LapizWindow *window;
	CtkUIManager *manager;

	lapiz_debug (DEBUG_PLUGINS);

	plugin = LAPIZ_SORT_PLUGIN (activatable);
	data = plugin->priv;
	window = LAPIZ_WINDOW (data->window);

	manager = lapiz_window_get_ui_manager (window);

	data->ui_action_group = ctk_action_group_new ("LapizSortPluginActions");
	ctk_action_group_set_translation_domain (data->ui_action_group,
						 GETTEXT_PACKAGE);
	ctk_action_group_add_actions (data->ui_action_group,
					   action_entries,
					   G_N_ELEMENTS (action_entries),
					   plugin);

	ctk_ui_manager_insert_action_group (manager,
					    data->ui_action_group,
					    -1);

	data->ui_id = ctk_ui_manager_new_merge_id (manager);

	ctk_ui_manager_add_ui (manager,
			       data->ui_id,
			       MENU_PATH,
			       "Sort",
			       "Sort",
			       CTK_UI_MANAGER_MENUITEM,
			       FALSE);

	update_ui (data);
}

static void
lapiz_sort_plugin_deactivate (PeasActivatable *activatable)
{
	LapizSortPluginPrivate *data;
	LapizWindow *window;
	CtkUIManager *manager;

	lapiz_debug (DEBUG_PLUGINS);

	data = LAPIZ_SORT_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (data->window);

	manager = lapiz_window_get_ui_manager (window);

	ctk_ui_manager_remove_ui (manager,
				  data->ui_id);
	ctk_ui_manager_remove_action_group (manager,
					    data->ui_action_group);
}

static void
lapiz_sort_plugin_update_state (PeasActivatable *activatable)
{
	lapiz_debug (DEBUG_PLUGINS);

	update_ui (LAPIZ_SORT_PLUGIN (activatable)->priv);
}

static void
lapiz_sort_plugin_init (LapizSortPlugin *plugin)
{
	lapiz_debug_message (DEBUG_PLUGINS, "LapizSortPlugin initializing");

	plugin->priv = lapiz_sort_plugin_get_instance_private (plugin);
}

static void
lapiz_sort_plugin_dispose (GObject *object)
{
	LapizSortPlugin *plugin = LAPIZ_SORT_PLUGIN (object);

	lapiz_debug_message (DEBUG_PLUGINS, "LapizSortPlugin disposing");

	if (plugin->priv->window != NULL)
	{
		g_object_unref (plugin->priv->window);
		plugin->priv->window = NULL;
	}

	if (plugin->priv->ui_action_group)
	{
		g_object_unref (plugin->priv->ui_action_group);
		plugin->priv->ui_action_group = NULL;
	}

	G_OBJECT_CLASS (lapiz_sort_plugin_parent_class)->dispose (object);
}

static void
lapiz_sort_plugin_class_init (LapizSortPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = lapiz_sort_plugin_dispose;
	object_class->set_property = lapiz_sort_plugin_set_property;
	object_class->get_property = lapiz_sort_plugin_get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");
}

static void
lapiz_sort_plugin_class_finalize (LapizSortPluginClass *klass)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
bean_activatable_iface_init (PeasActivatableInterface *iface)
{
	iface->activate = lapiz_sort_plugin_activate;
	iface->deactivate = lapiz_sort_plugin_deactivate;
	iface->update_state = lapiz_sort_plugin_update_state;
}

G_MODULE_EXPORT void
bean_register_types (PeasObjectModule *module)
{
	lapiz_sort_plugin_register_type (G_TYPE_MODULE (module));

	bean_object_module_register_extension_type (module,
	                                            PEAS_TYPE_ACTIVATABLE,
	                                            LAPIZ_TYPE_SORT_PLUGIN);
}
