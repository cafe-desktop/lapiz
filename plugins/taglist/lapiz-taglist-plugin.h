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

#ifndef __LAPIZ_TAGLIST_PLUGIN_H__
#define __LAPIZ_TAGLIST_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_TAGLIST_PLUGIN		(lapiz_taglist_plugin_get_type ())
#define LAPIZ_TAGLIST_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), LAPIZ_TYPE_TAGLIST_PLUGIN, PlumaTaglistPlugin))
#define LAPIZ_TAGLIST_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), LAPIZ_TYPE_TAGLIST_PLUGIN, PlumaTaglistPluginClass))
#define LAPIZ_IS_TAGLIST_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LAPIZ_TYPE_TAGLIST_PLUGIN))
#define LAPIZ_IS_TAGLIST_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LAPIZ_TYPE_TAGLIST_PLUGIN))
#define LAPIZ_TAGLIST_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LAPIZ_TYPE_TAGLIST_PLUGIN, PlumaTaglistPluginClass))

/* Private structure type */
typedef struct _PlumaTaglistPluginPrivate	PlumaTaglistPluginPrivate;

/*
 * Main object structure
 */
typedef struct _PlumaTaglistPlugin		PlumaTaglistPlugin;

struct _PlumaTaglistPlugin
{
	PeasExtensionBase parent_instance;

	/*< private >*/
	PlumaTaglistPluginPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _PlumaTaglistPluginClass	PlumaTaglistPluginClass;

struct _PlumaTaglistPluginClass
{
	PeasExtensionBaseClass parent_class;
};

/*
 * Public methods
 */
GType	lapiz_taglist_plugin_get_type		(void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT void peas_register_types (PeasObjectModule *module);

G_END_DECLS

#endif /* __LAPIZ_TAGLIST_PLUGIN_H__ */
