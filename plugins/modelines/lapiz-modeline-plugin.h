/*
 * lapiz-modeline-plugin.h
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

#ifndef __LAPIZ_MODELINE_PLUGIN_H__
#define __LAPIZ_MODELINE_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <libbean/bean-extension-base.h>
#include <libbean/bean-object-module.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_MODELINE_PLUGIN		(lapiz_modeline_plugin_get_type ())
#define LAPIZ_MODELINE_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), LAPIZ_TYPE_MODELINE_PLUGIN, LapizModelinePlugin))
#define LAPIZ_MODELINE_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), LAPIZ_TYPE_MODELINE_PLUGIN, LapizModelinePluginClass))
#define LAPIZ_IS_MODELINE_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LAPIZ_TYPE_MODELINE_PLUGIN))
#define LAPIZ_IS_MODELINE_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LAPIZ_TYPE_MODELINE_PLUGIN))
#define LAPIZ_MODELINE_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LAPIZ_TYPE_MODELINE_PLUGIN, LapizModelinePluginClass))

typedef struct _LapizModelinePlugin         LapizModelinePlugin;
typedef struct _LapizModelinePluginPrivate  LapizModelinePluginPrivate;

struct _LapizModelinePlugin
{
	BeanExtensionBase parent_instance;

	/*< private >*/
	LapizModelinePluginPrivate *priv;
};

typedef struct _LapizModelinePluginClass    LapizModelinePluginClass;

struct _LapizModelinePluginClass
{
	BeanExtensionBaseClass parent_class;
};

GType	lapiz_modeline_plugin_get_type		(void) G_GNUC_CONST;

G_MODULE_EXPORT void bean_register_types (BeanObjectModule *module);

G_END_DECLS

#endif /* __LAPIZ_MODELINE_PLUGIN_H__ */
