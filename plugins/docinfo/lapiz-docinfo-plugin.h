/*
 * lapiz-docinfo-plugin.h
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

#ifndef __LAPIZ_DOCINFO_PLUGIN_H__
#define __LAPIZ_DOCINFO_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <libbean/bean-extension-base.h>
#include <libbean/bean-object-module.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_DOCINFO_PLUGIN		(lapiz_docinfo_plugin_get_type ())
#define LAPIZ_DOCINFO_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), LAPIZ_TYPE_DOCINFO_PLUGIN, LapizDocInfoPlugin))
#define LAPIZ_DOCINFO_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), LAPIZ_TYPE_DOCINFO_PLUGIN, LapizDocInfoPluginClass))
#define LAPIZ_IS_DOCINFO_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LAPIZ_TYPE_DOCINFO_PLUGIN))
#define LAPIZ_IS_DOCINFO_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LAPIZ_TYPE_DOCINFO_PLUGIN))
#define LAPIZ_DOCINFO_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LAPIZ_TYPE_DOCINFO_PLUGIN, LapizDocInfoPluginClass))

/* Private structure type */
typedef struct _LapizDocInfoPluginPrivate	LapizDocInfoPluginPrivate;

/*
 * Main object structure
 */
typedef struct _LapizDocInfoPlugin		LapizDocInfoPlugin;

struct _LapizDocInfoPlugin
{
	BeanExtensionBase parent_instance;

	/*< private >*/
	LapizDocInfoPluginPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizDocInfoPluginClass	LapizDocInfoPluginClass;

struct _LapizDocInfoPluginClass
{
	BeanExtensionBaseClass parent_class;
};

/*
 * Public methods
 */
GType	lapiz_docinfo_plugin_get_type		(void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT void bean_register_types (BeanObjectModule *module);

G_END_DECLS

#endif /* __LAPIZ_DOCINFO_PLUGIN_H__ */
