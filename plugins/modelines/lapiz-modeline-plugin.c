/*
 * lapiz-modeline-plugin.c
 * Emacs, Kate and Vim-style modelines support for lapiz.
 *
 * Copyright (C) 2005-2007 - Steve Frécinaux <code@istique.net>
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
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <libbean/bean-activatable.h>
#include "lapiz-modeline-plugin.h"
#include "modeline-parser.h"

#include <lapiz/lapiz-window.h>
#include <lapiz/lapiz-debug.h>

#define DOCUMENT_DATA_KEY "LapizModelinePluginDocumentData"

struct _LapizModelinePluginPrivate
{
	CtkWidget *window;

	gulong tab_added_handler_id;
	gulong tab_removed_handler_id;
};

typedef struct
{
	gulong document_loaded_handler_id;
	gulong document_saved_handler_id;
} DocumentData;

enum {
	PROP_0,
	PROP_OBJECT
};

static void bean_activatable_iface_init (BeanActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizModelinePlugin,
                                lapiz_modeline_plugin,
                                BEAN_TYPE_EXTENSION_BASE,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizModelinePlugin)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (BEAN_TYPE_ACTIVATABLE,
                                                               bean_activatable_iface_init))

static void
document_data_free (DocumentData *ddata)
{
	g_slice_free (DocumentData, ddata);
}

static void
lapiz_modeline_plugin_constructed (GObject *object)
{
	gchar *data_dir;

	data_dir = bean_extension_base_get_data_dir (BEAN_EXTENSION_BASE (object));

	modeline_parser_init (data_dir);

	g_free (data_dir);

	G_OBJECT_CLASS (lapiz_modeline_plugin_parent_class)->constructed (object);
}

static void
lapiz_modeline_plugin_init (LapizModelinePlugin *plugin)
{
	lapiz_debug_message (DEBUG_PLUGINS, "LapizModelinePlugin initializing");

	plugin->priv = lapiz_modeline_plugin_get_instance_private (plugin);
}

static void
lapiz_modeline_plugin_finalize (GObject *object)
{
	lapiz_debug_message (DEBUG_PLUGINS, "LapizModelinePlugin finalizing");

	modeline_parser_shutdown ();

	G_OBJECT_CLASS (lapiz_modeline_plugin_parent_class)->finalize (object);
}

static void
lapiz_modeline_plugin_dispose (GObject *object)
{
	LapizModelinePlugin *plugin = LAPIZ_MODELINE_PLUGIN (object);

	lapiz_debug_message (DEBUG_PLUGINS, "LapizModelinePlugin disposing");

	if (plugin->priv->window != NULL)
	{
		g_object_unref (plugin->priv->window);
		plugin->priv->window = NULL;
	}

	G_OBJECT_CLASS (lapiz_modeline_plugin_parent_class)->dispose (object);
}

static void
lapiz_modeline_plugin_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	LapizModelinePlugin *plugin = LAPIZ_MODELINE_PLUGIN (object);

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
lapiz_modeline_plugin_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	LapizModelinePlugin *plugin = LAPIZ_MODELINE_PLUGIN (object);

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
on_document_loaded_or_saved (LapizDocument *document G_GNUC_UNUSED,
			     const GError  *error G_GNUC_UNUSED,
			     CtkSourceView *view)
{
	modeline_parser_apply_modeline (view);
}

static void
connect_handlers (LapizView *view)
{
	DocumentData *data;
        CtkTextBuffer *doc;

        doc = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));

        data = g_slice_new (DocumentData);

	data->document_loaded_handler_id =
		g_signal_connect (doc, "loaded",
				  G_CALLBACK (on_document_loaded_or_saved),
				  view);
	data->document_saved_handler_id =
		g_signal_connect (doc, "saved",
				  G_CALLBACK (on_document_loaded_or_saved),
				  view);

	g_object_set_data_full (G_OBJECT (doc), DOCUMENT_DATA_KEY,
				data, (GDestroyNotify) document_data_free);
}

static void
disconnect_handlers (LapizView *view)
{
	DocumentData *data;
	CtkTextBuffer *doc;

	doc = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));

	data = g_object_steal_data (G_OBJECT (doc), DOCUMENT_DATA_KEY);

	if (data)
	{
		g_signal_handler_disconnect (doc, data->document_loaded_handler_id);
		g_signal_handler_disconnect (doc, data->document_saved_handler_id);

		document_data_free (data);
	}
	else
	{
		g_warning ("Modeline handlers not found");
	}
}

static void
on_window_tab_added (LapizWindow *window G_GNUC_UNUSED,
		     LapizTab *tab,
		     gpointer user_data G_GNUC_UNUSED)
{
	connect_handlers (lapiz_tab_get_view (tab));
}

static void
on_window_tab_removed (LapizWindow *window G_GNUC_UNUSED,
		       LapizTab *tab,
		       gpointer user_data G_GNUC_UNUSED)
{
	disconnect_handlers (lapiz_tab_get_view (tab));
}

static void
lapiz_modeline_plugin_activate (BeanActivatable *activatable)
{
	LapizModelinePluginPrivate *data;
	LapizWindow *window;
	GList *views;
	GList *l;

	lapiz_debug (DEBUG_PLUGINS);

	data = LAPIZ_MODELINE_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (data->window);

	views = lapiz_window_get_views (window);
	for (l = views; l != NULL; l = l->next)
	{
		connect_handlers (LAPIZ_VIEW (l->data));
		modeline_parser_apply_modeline (CTK_SOURCE_VIEW (l->data));
	}
	g_list_free (views);

	data->tab_added_handler_id =
		g_signal_connect (window, "tab-added",
				  G_CALLBACK (on_window_tab_added), NULL);

	data->tab_removed_handler_id =
		g_signal_connect (window, "tab-removed",
				  G_CALLBACK (on_window_tab_removed), NULL);
}

static void
lapiz_modeline_plugin_deactivate (BeanActivatable *activatable)
{
	LapizModelinePluginPrivate *data;
	LapizWindow *window;
	GList *views;
	GList *l;

	lapiz_debug (DEBUG_PLUGINS);

	data = LAPIZ_MODELINE_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (data->window);

	g_signal_handler_disconnect (window, data->tab_added_handler_id);
	g_signal_handler_disconnect (window, data->tab_removed_handler_id);

	views = lapiz_window_get_views (window);

	for (l = views; l != NULL; l = l->next)
	{
		disconnect_handlers (LAPIZ_VIEW (l->data));

		modeline_parser_deactivate (CTK_SOURCE_VIEW (l->data));
	}

	g_list_free (views);
}

static void
lapiz_modeline_plugin_class_init (LapizModelinePluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructed = lapiz_modeline_plugin_constructed;
	object_class->finalize = lapiz_modeline_plugin_finalize;
	object_class->dispose = lapiz_modeline_plugin_dispose;
	object_class->set_property = lapiz_modeline_plugin_set_property;
	object_class->get_property = lapiz_modeline_plugin_get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");
}

static void
lapiz_modeline_plugin_class_finalize (LapizModelinePluginClass *klass G_GNUC_UNUSED)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
bean_activatable_iface_init (BeanActivatableInterface *iface)
{
	iface->activate = lapiz_modeline_plugin_activate;
	iface->deactivate = lapiz_modeline_plugin_deactivate;
}

G_MODULE_EXPORT void
bean_register_types (BeanObjectModule *module)
{
	lapiz_modeline_plugin_register_type (G_TYPE_MODULE (module));

	bean_object_module_register_extension_type (module,
	                                            BEAN_TYPE_ACTIVATABLE,
	                                            LAPIZ_TYPE_MODELINE_PLUGIN);
}
