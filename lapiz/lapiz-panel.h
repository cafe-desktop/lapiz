/*
 * lapiz-panel.h
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

#ifndef __LAPIZ_PANEL_H__
#define __LAPIZ_PANEL_H__

#include <ctk/ctk.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_PANEL		(lapiz_panel_get_type())
#define LAPIZ_PANEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_PANEL, LapizPanel))
#define LAPIZ_PANEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_PANEL, LapizPanelClass))
#define LAPIZ_IS_PANEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_PANEL))
#define LAPIZ_IS_PANEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_PANEL))
#define LAPIZ_PANEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_PANEL, LapizPanelClass))

/* Private structure type */
typedef struct _LapizPanelPrivate LapizPanelPrivate;

/*
 * Main object structure
 */
typedef struct _LapizPanel LapizPanel;

struct _LapizPanel
{
	GtkBox vbox;

	/*< private > */
	LapizPanelPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizPanelClass LapizPanelClass;

struct _LapizPanelClass
{
	GtkBoxClass parent_class;

	void (* item_added)     (LapizPanel     *panel,
				 GtkWidget      *item);
	void (* item_removed)   (LapizPanel     *panel,
				 GtkWidget      *item);

	/* Keybinding signals */
	void (* close)          (LapizPanel     *panel);
	void (* focus_document) (LapizPanel     *panel);

	/* Padding for future expansion */
	void (*_lapiz_reserved1) (void);
	void (*_lapiz_reserved2) (void);
	void (*_lapiz_reserved3) (void);
	void (*_lapiz_reserved4) (void);
};

/*
 * Public methods
 */
GType 		 lapiz_panel_get_type 			(void) G_GNUC_CONST;

GtkWidget 	*lapiz_panel_new 			(GtkOrientation	 orientation);

void		 lapiz_panel_add_item			(LapizPanel     *panel,
						      	 GtkWidget      *item,
						      	 const gchar    *name,
							 GtkWidget      *image);

void		 lapiz_panel_add_item_with_icon	(LapizPanel     *panel,
						 GtkWidget      *item,
						 const gchar    *name,
						 const gchar    *icon_name);

gboolean	 lapiz_panel_remove_item	(LapizPanel     *panel,
					  	 GtkWidget      *item);

gboolean	 lapiz_panel_activate_item 	(LapizPanel     *panel,
					    	 GtkWidget      *item);

gboolean	 lapiz_panel_item_is_active 	(LapizPanel     *panel,
					    	 GtkWidget      *item);

GtkOrientation	 lapiz_panel_get_orientation	(LapizPanel	*panel);

gint		 lapiz_panel_get_n_items	(LapizPanel	*panel);


/*
 * Non exported functions
 */
gint		 _lapiz_panel_get_active_item_id	(LapizPanel	*panel);

void		 _lapiz_panel_set_active_item_by_id	(LapizPanel	*panel,
							 gint		 id);

G_END_DECLS

#endif  /* __LAPIZ_PANEL_H__  */
