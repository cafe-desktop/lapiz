/*
 * lapiz-plugins-engine.h
 * This file is part of lapiz
 *
 * Copyright (C) 2002-2005 - Paolo Maggi
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
 * Modified by the lapiz Team, 2002-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __LAPIZ_PLUGINS_ENGINE_H__
#define __LAPIZ_PLUGINS_ENGINE_H__

#include <glib.h>
#include <libpeas/peas-engine.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_PLUGINS_ENGINE              (lapiz_plugins_engine_get_type ())
#define LAPIZ_PLUGINS_ENGINE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_PLUGINS_ENGINE, LapizPluginsEngine))
#define LAPIZ_PLUGINS_ENGINE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_PLUGINS_ENGINE, LapizPluginsEngineClass))
#define LAPIZ_IS_PLUGINS_ENGINE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_PLUGINS_ENGINE))
#define LAPIZ_IS_PLUGINS_ENGINE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_PLUGINS_ENGINE))
#define LAPIZ_PLUGINS_ENGINE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_PLUGINS_ENGINE, LapizPluginsEngineClass))

typedef struct _LapizPluginsEngine		LapizPluginsEngine;
typedef struct _LapizPluginsEnginePrivate	LapizPluginsEnginePrivate;

struct _LapizPluginsEngine
{
	PeasEngine parent;
	LapizPluginsEnginePrivate *priv;
};

typedef struct _LapizPluginsEngineClass		LapizPluginsEngineClass;

struct _LapizPluginsEngineClass
{
	PeasEngineClass parent_class;
};

GType			 lapiz_plugins_engine_get_type		(void) G_GNUC_CONST;

LapizPluginsEngine	*lapiz_plugins_engine_get_default	(void);

G_END_DECLS

#endif  /* __LAPIZ_PLUGINS_ENGINE_H__ */
