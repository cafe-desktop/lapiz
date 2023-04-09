/*
 * lapiz-taglist-plugin-panel.h
 * This file is part of lapiz
 *
 * Copyright (C) 2005 - Paolo Maggi
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
 * Modified by the lapiz Team, 2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __LAPIZ_TAGLIST_PLUGIN_PANEL_H__
#define __LAPIZ_TAGLIST_PLUGIN_PANEL_H__

#include <gtk/gtk.h>

#include <lapiz/lapiz-window.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_TAGLIST_PLUGIN_PANEL              (lapiz_taglist_plugin_panel_get_type())
#define LAPIZ_TAGLIST_PLUGIN_PANEL(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_TAGLIST_PLUGIN_PANEL, LapizTaglistPluginPanel))
#define LAPIZ_TAGLIST_PLUGIN_PANEL_CONST(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_TAGLIST_PLUGIN_PANEL, LapizTaglistPluginPanel const))
#define LAPIZ_TAGLIST_PLUGIN_PANEL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_TAGLIST_PLUGIN_PANEL, LapizTaglistPluginPanelClass))
#define LAPIZ_IS_TAGLIST_PLUGIN_PANEL(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_TAGLIST_PLUGIN_PANEL))
#define LAPIZ_IS_TAGLIST_PLUGIN_PANEL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_TAGLIST_PLUGIN_PANEL))
#define LAPIZ_TAGLIST_PLUGIN_PANEL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_TAGLIST_PLUGIN_PANEL, LapizTaglistPluginPanelClass))

/* Private structure type */
typedef struct _LapizTaglistPluginPanelPrivate LapizTaglistPluginPanelPrivate;

/*
 * Main object structure
 */
typedef struct _LapizTaglistPluginPanel LapizTaglistPluginPanel;

struct _LapizTaglistPluginPanel
{
	GtkBox vbox;

	/*< private > */
	LapizTaglistPluginPanelPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizTaglistPluginPanelClass LapizTaglistPluginPanelClass;

struct _LapizTaglistPluginPanelClass
{
	GtkBoxClass parent_class;
};

/*
 * Public methods
 */
void		 _lapiz_taglist_plugin_panel_register_type	(GTypeModule *module);

GType 		 lapiz_taglist_plugin_panel_get_type		(void) G_GNUC_CONST;

GtkWidget	*lapiz_taglist_plugin_panel_new 		(LapizWindow *window,
								 const gchar *data_dir);

G_END_DECLS

#endif  /* __LAPIZ_TAGLIST_PLUGIN_PANEL_H__  */
