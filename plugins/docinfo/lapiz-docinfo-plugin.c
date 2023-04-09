/*
 * lapiz-docinfo-plugin.c
 *
 * Copyright (C) 2002-2005 Paolo Maggi
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

#include "lapiz-docinfo-plugin.h"

#include <string.h> /* For strlen (...) */

#include <glib/gi18n-lib.h>
#include <pango/pango-break.h>
#include <gmodule.h>
#include <libpeas/peas-activatable.h>

#include <lapiz/lapiz-window.h>
#include <lapiz/lapiz-debug.h>
#include <lapiz/lapiz-utils.h>

#define MENU_PATH "/MenuBar/ToolsMenu/ToolsOps_2"

static void peas_activatable_iface_init (PeasActivatableInterface *iface);

typedef struct
{
	CtkWidget *dialog;
	CtkWidget *file_name_label;
	CtkWidget *lines_label;
	CtkWidget *words_label;
	CtkWidget *chars_label;
	CtkWidget *chars_ns_label;
	CtkWidget *bytes_label;
	CtkWidget *selection_vbox;
	CtkWidget *selected_lines_label;
	CtkWidget *selected_words_label;
	CtkWidget *selected_chars_label;
	CtkWidget *selected_chars_ns_label;
	CtkWidget *selected_bytes_label;
} DocInfoDialog;

struct _LapizDocInfoPluginPrivate
{
	CtkWidget *window;

	CtkActionGroup *ui_action_group;
	guint ui_id;

	DocInfoDialog *dialog;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizDocInfoPlugin,
                                lapiz_docinfo_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizDocInfoPlugin)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_TYPE_ACTIVATABLE,
                                                               peas_activatable_iface_init))

enum {
	PROP_0,
	PROP_OBJECT
};

static void docinfo_dialog_response_cb (CtkDialog   *widget,
					gint	    res_id,
					LapizDocInfoPluginPrivate *data);

static void
docinfo_dialog_destroy_cb (GObject  *obj,
			   LapizDocInfoPluginPrivate *data)
{
	lapiz_debug (DEBUG_PLUGINS);

	if (data != NULL)
	{
		g_free (data->dialog);
		data->dialog = NULL;
	}
}

static DocInfoDialog *
get_docinfo_dialog (LapizDocInfoPlugin *plugin)
{
	LapizDocInfoPluginPrivate *data;
	LapizWindow *window;
	DocInfoDialog *dialog;
	gchar *data_dir;
	gchar *ui_file;
	CtkWidget *content;
	CtkWidget *error_widget;
	gboolean ret;

	lapiz_debug (DEBUG_PLUGINS);

	data = plugin->priv;
	window = LAPIZ_WINDOW (data->window);

	dialog = g_new (DocInfoDialog, 1);

	data_dir = peas_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	ui_file = g_build_filename (data_dir, "docinfo.ui", NULL);
	ret = lapiz_utils_get_ui_objects (ui_file,
					  NULL,
					  &error_widget,
					  "dialog", &dialog->dialog,
					  "docinfo_dialog_content", &content,
					  "file_name_label", &dialog->file_name_label,
					  "words_label", &dialog->words_label,
					  "bytes_label", &dialog->bytes_label,
					  "lines_label", &dialog->lines_label,
					  "chars_label", &dialog->chars_label,
					  "chars_ns_label", &dialog->chars_ns_label,
					  "selection_vbox", &dialog->selection_vbox,
					  "selected_words_label", &dialog->selected_words_label,
					  "selected_bytes_label", &dialog->selected_bytes_label,
					  "selected_lines_label", &dialog->selected_lines_label,
					  "selected_chars_label", &dialog->selected_chars_label,
					  "selected_chars_ns_label", &dialog->selected_chars_ns_label,
					  NULL);

	g_free (data_dir);
	g_free (ui_file);

	if (!ret)
	{
		const gchar *err_message;

		err_message = ctk_label_get_label (CTK_LABEL (error_widget));
		lapiz_warning (CTK_WINDOW (window), "%s", err_message);

		g_free (dialog);
		ctk_widget_destroy (error_widget);

		return NULL;
	}

	ctk_dialog_set_default_response (CTK_DIALOG (dialog->dialog),
					 CTK_RESPONSE_OK);
	ctk_window_set_transient_for (CTK_WINDOW (dialog->dialog),
				      CTK_WINDOW (window));

	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (docinfo_dialog_destroy_cb),
			  data);

	g_signal_connect (dialog->dialog,
			  "response",
			  G_CALLBACK (docinfo_dialog_response_cb),
			  data);

	return dialog;
}

static void
calculate_info (LapizDocument *doc,
		CtkTextIter   *start,
		CtkTextIter   *end,
		gint          *chars,
		gint          *words,
		gint          *white_chars,
		gint          *bytes)
{
	gchar *text;

	lapiz_debug (DEBUG_PLUGINS);

	text = ctk_text_buffer_get_slice (CTK_TEXT_BUFFER (doc),
					  start,
					  end,
					  TRUE);

	*chars = g_utf8_strlen (text, -1);
	*bytes = strlen (text);

	if (*chars > 0)
	{
		PangoLogAttr *attrs;
		gint i;

		attrs = g_new0 (PangoLogAttr, *chars + 1);

		pango_get_log_attrs (text,
				     -1,
				     0,
				     pango_language_from_string ("C"),
				     attrs,
				     *chars + 1);

		for (i = 0; i < (*chars); i++)
		{
			if (attrs[i].is_white)
				++(*white_chars);

			if (attrs[i].is_word_start)
				++(*words);
		}

		g_free (attrs);
	}
	else
	{
		*white_chars = 0;
		*words = 0;
	}

	g_free (text);
}

static void
docinfo_real (LapizDocument *doc,
	      DocInfoDialog *dialog)
{
	CtkTextIter start, end;
	gint words = 0;
	gint chars = 0;
	gint white_chars = 0;
	gint lines = 0;
	gint bytes = 0;
	gchar *tmp_str;
	gchar *doc_name;

	lapiz_debug (DEBUG_PLUGINS);

	ctk_text_buffer_get_bounds (CTK_TEXT_BUFFER (doc),
				    &start,
				    &end);

	lines = ctk_text_buffer_get_line_count (CTK_TEXT_BUFFER (doc));

	calculate_info (doc,
			&start, &end,
			&chars, &words, &white_chars, &bytes);

	if (chars == 0)
		lines = 0;

	lapiz_debug_message (DEBUG_PLUGINS, "Chars: %d", chars);
	lapiz_debug_message (DEBUG_PLUGINS, "Lines: %d", lines);
	lapiz_debug_message (DEBUG_PLUGINS, "Words: %d", words);
	lapiz_debug_message (DEBUG_PLUGINS, "Chars non-space: %d", chars - white_chars);
	lapiz_debug_message (DEBUG_PLUGINS, "Bytes: %d", bytes);

	doc_name = lapiz_document_get_short_name_for_display (doc);
	tmp_str = g_strdup_printf ("<span weight=\"bold\">%s</span>", doc_name);
	ctk_label_set_markup (CTK_LABEL (dialog->file_name_label), tmp_str);
	g_free (doc_name);
	g_free (tmp_str);

	tmp_str = g_strdup_printf("%d", lines);
	ctk_label_set_text (CTK_LABEL (dialog->lines_label), tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf("%d", words);
	ctk_label_set_text (CTK_LABEL (dialog->words_label), tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf("%d", chars);
	ctk_label_set_text (CTK_LABEL (dialog->chars_label), tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf("%d", chars - white_chars);
	ctk_label_set_text (CTK_LABEL (dialog->chars_ns_label), tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf("%d", bytes);
	ctk_label_set_text (CTK_LABEL (dialog->bytes_label), tmp_str);
	g_free (tmp_str);
}

static void
selectioninfo_real (LapizDocument *doc,
		    DocInfoDialog *dialog)
{
	gboolean sel;
	CtkTextIter start, end;
	gint words = 0;
	gint chars = 0;
	gint white_chars = 0;
	gint lines = 0;
	gint bytes = 0;
	gchar *tmp_str;

	lapiz_debug (DEBUG_PLUGINS);

	sel = ctk_text_buffer_get_selection_bounds (CTK_TEXT_BUFFER (doc),
						    &start,
						    &end);

	if (sel)
	{
		lines = ctk_text_iter_get_line (&end) - ctk_text_iter_get_line (&start) + 1;

		calculate_info (doc,
				&start, &end,
				&chars, &words, &white_chars, &bytes);

		lapiz_debug_message (DEBUG_PLUGINS, "Selected chars: %d", chars);
		lapiz_debug_message (DEBUG_PLUGINS, "Selected lines: %d", lines);
		lapiz_debug_message (DEBUG_PLUGINS, "Selected words: %d", words);
		lapiz_debug_message (DEBUG_PLUGINS, "Selected chars non-space: %d", chars - white_chars);
		lapiz_debug_message (DEBUG_PLUGINS, "Selected bytes: %d", bytes);

		ctk_widget_set_sensitive (dialog->selection_vbox, TRUE);
	}
	else
	{
		ctk_widget_set_sensitive (dialog->selection_vbox, FALSE);

		lapiz_debug_message (DEBUG_PLUGINS, "Selection empty");
	}

	if (chars == 0)
		lines = 0;

	tmp_str = g_strdup_printf("%d", lines);
	ctk_label_set_text (CTK_LABEL (dialog->selected_lines_label), tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf("%d", words);
	ctk_label_set_text (CTK_LABEL (dialog->selected_words_label), tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf("%d", chars);
	ctk_label_set_text (CTK_LABEL (dialog->selected_chars_label), tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf("%d", chars - white_chars);
	ctk_label_set_text (CTK_LABEL (dialog->selected_chars_ns_label), tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf("%d", bytes);
	ctk_label_set_text (CTK_LABEL (dialog->selected_bytes_label), tmp_str);
	g_free (tmp_str);
}

static void
docinfo_cb (CtkAction	*action,
	    LapizDocInfoPlugin *plugin)
{
	LapizDocInfoPluginPrivate *data;
	LapizWindow *window;
	LapizDocument *doc;

	lapiz_debug (DEBUG_PLUGINS);

	data = plugin->priv;
	window = LAPIZ_WINDOW (data->window);
	doc = lapiz_window_get_active_document (window);
	g_return_if_fail (doc != NULL);

	if (data->dialog != NULL)
	{
		ctk_window_present (CTK_WINDOW (data->dialog->dialog));
		ctk_widget_grab_focus (CTK_WIDGET (data->dialog->dialog));
	}
	else
	{
		DocInfoDialog *dialog;

		dialog = get_docinfo_dialog (plugin);
		g_return_if_fail (dialog != NULL);

		data->dialog = dialog;

		ctk_widget_show (CTK_WIDGET (dialog->dialog));
	}

	docinfo_real (doc,
		      data->dialog);
	selectioninfo_real (doc,
			    data->dialog);
}

static void
docinfo_dialog_response_cb (CtkDialog	*widget,
			    gint	res_id,
			    LapizDocInfoPluginPrivate *data)
{
	LapizWindow *window;

	lapiz_debug (DEBUG_PLUGINS);

	window = LAPIZ_WINDOW (data->window);

	switch (res_id)
	{
		case CTK_RESPONSE_CLOSE:
		{
			lapiz_debug_message (DEBUG_PLUGINS, "CTK_RESPONSE_CLOSE");
			ctk_widget_destroy (data->dialog->dialog);

			break;
		}

		case CTK_RESPONSE_OK:
		{
			LapizDocument *doc;

			lapiz_debug_message (DEBUG_PLUGINS, "CTK_RESPONSE_OK");

			doc = lapiz_window_get_active_document (window);
			g_return_if_fail (doc != NULL);

			docinfo_real (doc,
				      data->dialog);

			selectioninfo_real (doc,
					    data->dialog);

			break;
		}
	}
}

static const CtkActionEntry action_entries[] =
{
	{ "DocumentStatistics",
	  NULL,
	  N_("_Document Statistics"),
	  NULL,
	  N_("Get statistical information on the current document"),
	  G_CALLBACK (docinfo_cb) }
};

static void
update_ui (LapizDocInfoPluginPrivate *data)
{
	LapizWindow *window;
	LapizView *view;

	lapiz_debug (DEBUG_PLUGINS);

	window = LAPIZ_WINDOW (data->window);
	view = lapiz_window_get_active_view (window);

	ctk_action_group_set_sensitive (data->ui_action_group,
					(view != NULL));

	if (data->dialog != NULL)
	{
		ctk_dialog_set_response_sensitive (CTK_DIALOG (data->dialog->dialog),
						   CTK_RESPONSE_OK,
						   (view != NULL));
	}
}

static void
lapiz_docinfo_plugin_init (LapizDocInfoPlugin *plugin)
{
	lapiz_debug_message (DEBUG_PLUGINS, "LapizDocInfoPlugin initializing");

	plugin->priv = lapiz_docinfo_plugin_get_instance_private (plugin);
}

static void
lapiz_docinfo_plugin_dispose (GObject *object)
{
	LapizDocInfoPlugin *plugin = LAPIZ_DOCINFO_PLUGIN (object);

	lapiz_debug_message (DEBUG_PLUGINS, "LapizDocInfoPlugin disposing");

	if (plugin->priv->window != NULL)
	{
		g_object_unref (plugin->priv->window);
		plugin->priv->window = NULL;
	}

	if (plugin->priv->ui_action_group != NULL)
	{
		g_object_unref (plugin->priv->ui_action_group);
		plugin->priv->ui_action_group = NULL;
	}

	G_OBJECT_CLASS (lapiz_docinfo_plugin_parent_class)->dispose (object);
}

static void
lapiz_docinfo_plugin_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	LapizDocInfoPlugin *plugin = LAPIZ_DOCINFO_PLUGIN (object);

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
lapiz_docinfo_plugin_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	LapizDocInfoPlugin *plugin = LAPIZ_DOCINFO_PLUGIN (object);

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
lapiz_docinfo_plugin_activate (PeasActivatable *activatable)
{
	LapizDocInfoPlugin *plugin;
	LapizDocInfoPluginPrivate *data;
	LapizWindow *window;
	CtkUIManager *manager;

	lapiz_debug (DEBUG_PLUGINS);

	plugin = LAPIZ_DOCINFO_PLUGIN (activatable);
	data = plugin->priv;
	window = LAPIZ_WINDOW (data->window);

	data->dialog = NULL;
	data->ui_action_group = ctk_action_group_new ("LapizDocInfoPluginActions");

	ctk_action_group_set_translation_domain (data->ui_action_group,
						 GETTEXT_PACKAGE);
	ctk_action_group_add_actions (data->ui_action_group,
				      action_entries,
				      G_N_ELEMENTS (action_entries),
				      plugin);

	manager = lapiz_window_get_ui_manager (window);
	ctk_ui_manager_insert_action_group (manager,
					    data->ui_action_group,
					    -1);

	data->ui_id = ctk_ui_manager_new_merge_id (manager);

	ctk_ui_manager_add_ui (manager,
			       data->ui_id,
			       MENU_PATH,
			       "DocumentStatistics",
			       "DocumentStatistics",
			       CTK_UI_MANAGER_MENUITEM,
			       FALSE);

	update_ui (data);
}

static void
lapiz_docinfo_plugin_deactivate (PeasActivatable *activatable)
{
	LapizDocInfoPluginPrivate *data;
	LapizWindow *window;
	CtkUIManager *manager;

	lapiz_debug (DEBUG_PLUGINS);

	data = LAPIZ_DOCINFO_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (data->window);

	manager = lapiz_window_get_ui_manager (window);

	ctk_ui_manager_remove_ui (manager,
				  data->ui_id);
	ctk_ui_manager_remove_action_group (manager,
					    data->ui_action_group);
}

static void
lapiz_docinfo_plugin_update_state (PeasActivatable *activatable)
{
	lapiz_debug (DEBUG_PLUGINS);

	update_ui (LAPIZ_DOCINFO_PLUGIN (activatable)->priv);
}

static void
lapiz_docinfo_plugin_class_init (LapizDocInfoPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = lapiz_docinfo_plugin_dispose;
	object_class->set_property = lapiz_docinfo_plugin_set_property;
	object_class->get_property = lapiz_docinfo_plugin_get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");
}

static void
lapiz_docinfo_plugin_class_finalize (LapizDocInfoPluginClass *klass)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
peas_activatable_iface_init (PeasActivatableInterface *iface)
{
	iface->activate = lapiz_docinfo_plugin_activate;
	iface->deactivate = lapiz_docinfo_plugin_deactivate;
	iface->update_state = lapiz_docinfo_plugin_update_state;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	lapiz_docinfo_plugin_register_type (G_TYPE_MODULE (module));

	peas_object_module_register_extension_type (module,
	                                            PEAS_TYPE_ACTIVATABLE,
	                                            LAPIZ_TYPE_DOCINFO_PLUGIN);
}
