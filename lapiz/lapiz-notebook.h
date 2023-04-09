/*
 * lapiz-notebook.h
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
 */

/* This file is a modified version of the epiphany file ephy-notebook.h
 * Here the relevant copyright:
 *
 *  Copyright (C) 2002 Christophe Fergeau
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *
 */

#ifndef LAPIZ_NOTEBOOK_H
#define LAPIZ_NOTEBOOK_H

#include <lapiz/lapiz-tab.h>

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_NOTEBOOK		(lapiz_notebook_get_type ())
#define LAPIZ_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), LAPIZ_TYPE_NOTEBOOK, PlumaNotebook))
#define LAPIZ_NOTEBOOK_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), LAPIZ_TYPE_NOTEBOOK, PlumaNotebookClass))
#define LAPIZ_IS_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LAPIZ_TYPE_NOTEBOOK))
#define LAPIZ_IS_NOTEBOOK_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LAPIZ_TYPE_NOTEBOOK))
#define LAPIZ_NOTEBOOK_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LAPIZ_TYPE_NOTEBOOK, PlumaNotebookClass))

/* Private structure type */
typedef struct _PlumaNotebookPrivate	PlumaNotebookPrivate;

/*
 * Main object structure
 */
typedef struct _PlumaNotebook		PlumaNotebook;

struct _PlumaNotebook
{
	GtkNotebook notebook;

	/*< private >*/
        PlumaNotebookPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _PlumaNotebookClass	PlumaNotebookClass;

struct _PlumaNotebookClass
{
        GtkNotebookClass parent_class;

	/* Signals */
	void	 (* tab_added)      (PlumaNotebook *notebook,
				     PlumaTab      *tab);
	void	 (* tab_removed)    (PlumaNotebook *notebook,
				     PlumaTab      *tab);
	void	 (* tab_detached)   (PlumaNotebook *notebook,
				     PlumaTab      *tab);
	void	 (* tabs_reordered) (PlumaNotebook *notebook);
	void	 (* tab_close_request)
				    (PlumaNotebook *notebook,
				     PlumaTab      *tab);
};

/*
 * Public methods
 */
GType		lapiz_notebook_get_type		(void) G_GNUC_CONST;

GtkWidget      *lapiz_notebook_new		(void);

void		lapiz_notebook_add_tab		(PlumaNotebook *nb,
						 PlumaTab      *tab,
						 gint           position,
						 gboolean       jump_to);

void		lapiz_notebook_remove_tab	(PlumaNotebook *nb,
						 PlumaTab      *tab);

void		lapiz_notebook_remove_all_tabs 	(PlumaNotebook *nb);

void		lapiz_notebook_reorder_tab	(PlumaNotebook *src,
			    			 PlumaTab      *tab,
			    			 gint           dest_position);

void            lapiz_notebook_move_tab		(PlumaNotebook *src,
						 PlumaNotebook *dest,
						 PlumaTab      *tab,
						 gint           dest_position);

void		lapiz_notebook_set_close_buttons_sensitive
						(PlumaNotebook *nb,
						 gboolean       sensitive);

gboolean	lapiz_notebook_get_close_buttons_sensitive
						(PlumaNotebook *nb);

void		lapiz_notebook_set_tab_drag_and_drop_enabled
						(PlumaNotebook *nb,
						 gboolean       enable);

gboolean	lapiz_notebook_get_tab_drag_and_drop_enabled
						(PlumaNotebook *nb);

G_END_DECLS

#endif /* LAPIZ_NOTEBOOK_H */
