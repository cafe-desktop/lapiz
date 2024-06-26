/*
 * lapiz-taglist-plugin.h
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
 */

/*
 * Modified by the lapiz Team, 2002-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "lapiz-taglist-plugin.h"
#include "lapiz-taglist-plugin-panel.h"
#include "lapiz-taglist-plugin-parser.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <libbean/bean-activatable.h>

#include <lapiz/lapiz-window.h>
#include <lapiz/lapiz-debug.h>

struct _LapizTaglistPluginPrivate
{
	CtkWidget *window;

	CtkWidget *taglist_panel;
};

static void bean_activatable_iface_init (BeanActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizTaglistPlugin,
                                lapiz_taglist_plugin,
                                BEAN_TYPE_EXTENSION_BASE,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizTaglistPlugin)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (BEAN_TYPE_ACTIVATABLE,
                                                               bean_activatable_iface_init) \
                                                                                            \
                                _lapiz_taglist_plugin_panel_register_type (type_module);    \
)

enum {
	PROP_0,
	PROP_OBJECT
};

static void
lapiz_taglist_plugin_init (LapizTaglistPlugin *plugin)
{
	plugin->priv = lapiz_taglist_plugin_get_instance_private (plugin);

	lapiz_debug_message (DEBUG_PLUGINS, "LapizTaglistPlugin initializing");
}

static void
lapiz_taglist_plugin_dispose (GObject *object)
{
	LapizTaglistPlugin *plugin = LAPIZ_TAGLIST_PLUGIN (object);

	lapiz_debug_message (DEBUG_PLUGINS, "LapizTaglistPlugin disposing");

	if (plugin->priv->window != NULL)
	{
		g_object_unref (plugin->priv->window);
		plugin->priv->window = NULL;
	}

	G_OBJECT_CLASS (lapiz_taglist_plugin_parent_class)->dispose (object);
}

static void
lapiz_taglist_plugin_finalize (GObject *object)
{
	lapiz_debug_message (DEBUG_PLUGINS, "LapizTaglistPlugin finalizing");

	free_taglist ();

	G_OBJECT_CLASS (lapiz_taglist_plugin_parent_class)->finalize (object);
}

static void
lapiz_taglist_plugin_activate (BeanActivatable *activatable)
{
	LapizTaglistPluginPrivate *priv;
	LapizWindow *window;
	LapizPanel *side_panel;
	gchar *data_dir;

	lapiz_debug (DEBUG_PLUGINS);

	priv = LAPIZ_TAGLIST_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (priv->window);
	side_panel = lapiz_window_get_side_panel (window);

	data_dir = bean_extension_base_get_data_dir (BEAN_EXTENSION_BASE (activatable));
	priv->taglist_panel = lapiz_taglist_plugin_panel_new (window, data_dir);
	g_free (data_dir);

	lapiz_panel_add_item_with_icon (side_panel,
					priv->taglist_panel,
					_("Tags"),
					"list-add");
}

static void
lapiz_taglist_plugin_deactivate (BeanActivatable *activatable)
{
	LapizTaglistPluginPrivate *priv;
	LapizWindow *window;
	LapizPanel *side_panel;

	lapiz_debug (DEBUG_PLUGINS);

	priv = LAPIZ_TAGLIST_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (priv->window);
	side_panel = lapiz_window_get_side_panel (window);

	lapiz_panel_remove_item (side_panel,
				 priv->taglist_panel);
}

static void
lapiz_taglist_plugin_update_state (BeanActivatable *activatable)
{
	LapizTaglistPluginPrivate *priv;
	LapizWindow *window;
	LapizView *view;

	lapiz_debug (DEBUG_PLUGINS);

	priv = LAPIZ_TAGLIST_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (priv->window);
	view = lapiz_window_get_active_view (window);

	ctk_widget_set_sensitive (priv->taglist_panel,
				  (view != NULL) &&
				  ctk_text_view_get_editable (CTK_TEXT_VIEW (view)));
}

static void
lapiz_taglist_plugin_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	LapizTaglistPlugin *plugin = LAPIZ_TAGLIST_PLUGIN (object);

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
lapiz_taglist_plugin_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	LapizTaglistPlugin *plugin = LAPIZ_TAGLIST_PLUGIN (object);

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
lapiz_taglist_plugin_class_init (LapizTaglistPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_taglist_plugin_finalize;
	object_class->dispose = lapiz_taglist_plugin_dispose;
	object_class->set_property = lapiz_taglist_plugin_set_property;
	object_class->get_property = lapiz_taglist_plugin_get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");
}

static void
lapiz_taglist_plugin_class_finalize (LapizTaglistPluginClass *klass G_GNUC_UNUSED)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
bean_activatable_iface_init (BeanActivatableInterface *iface)
{
	iface->activate = lapiz_taglist_plugin_activate;
	iface->deactivate = lapiz_taglist_plugin_deactivate;
	iface->update_state = lapiz_taglist_plugin_update_state;
}

G_MODULE_EXPORT void
bean_register_types (BeanObjectModule *module)
{
	lapiz_taglist_plugin_register_type (G_TYPE_MODULE (module));

	bean_object_module_register_extension_type (module,
	                                            BEAN_TYPE_ACTIVATABLE,
	                                            LAPIZ_TYPE_TAGLIST_PLUGIN);
}
