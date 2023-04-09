/*
 * lapiz-file-browser-plugin.h - Pluma plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
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

#ifndef __LAPIZ_FILE_BROWSER_PLUGIN_H__
#define __LAPIZ_FILE_BROWSER_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_FILE_BROWSER_PLUGIN		(lapiz_file_browser_plugin_get_type ())
#define LAPIZ_FILE_BROWSER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), LAPIZ_TYPE_FILE_BROWSER_PLUGIN, PlumaFileBrowserPlugin))
#define LAPIZ_FILE_BROWSER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), LAPIZ_TYPE_FILE_BROWSER_PLUGIN, PlumaFileBrowserPluginClass))
#define LAPIZ_IS_FILE_BROWSER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LAPIZ_TYPE_FILE_BROWSER_PLUGIN))
#define LAPIZ_IS_FILE_BROWSER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LAPIZ_TYPE_FILE_BROWSER_PLUGIN))
#define LAPIZ_FILE_BROWSER_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), LAPIZ_TYPE_FILE_BROWSER_PLUGIN, PlumaFileBrowserPluginClass))

/* Private structure type */
typedef struct _PlumaFileBrowserPluginPrivate PlumaFileBrowserPluginPrivate;
typedef struct _PlumaFileBrowserPlugin        PlumaFileBrowserPlugin;
typedef struct _PlumaFileBrowserPluginClass   PlumaFileBrowserPluginClass;

struct _PlumaFileBrowserPlugin
{
	PeasExtensionBase parent_instance;

	/*< private > */
	PlumaFileBrowserPluginPrivate *priv;
};

struct _PlumaFileBrowserPluginClass
{
	PeasExtensionBaseClass parent_class;
};

/*
 * Public methods
 */
GType lapiz_file_browser_plugin_get_type              (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT void peas_register_types (PeasObjectModule *module);

G_END_DECLS

#endif /* __LAPIZ_FILE_BROWSER_PLUGIN_H__ */

// ex:ts=8:noet:
