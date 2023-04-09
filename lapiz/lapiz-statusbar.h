/*
 * lapiz-statusbar.h
 * This file is part of lapiz
 *
 * Copyright (C) 2005 - Paolo Borelli
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

#ifndef LAPIZ_STATUSBAR_H
#define LAPIZ_STATUSBAR_H

#include <ctk/ctk.h>
#include <lapiz/lapiz-window.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_STATUSBAR		(lapiz_statusbar_get_type ())
#define LAPIZ_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), LAPIZ_TYPE_STATUSBAR, LapizStatusbar))
#define LAPIZ_STATUSBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), LAPIZ_TYPE_STATUSBAR, LapizStatusbarClass))
#define LAPIZ_IS_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LAPIZ_TYPE_STATUSBAR))
#define LAPIZ_IS_STATUSBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LAPIZ_TYPE_STATUSBAR))
#define LAPIZ_STATUSBAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LAPIZ_TYPE_STATUSBAR, LapizStatusbarClass))

typedef struct _LapizStatusbar		LapizStatusbar;
typedef struct _LapizStatusbarPrivate	LapizStatusbarPrivate;
typedef struct _LapizStatusbarClass	LapizStatusbarClass;

struct _LapizStatusbar
{
        GtkStatusbar parent;

	/* <private/> */
        LapizStatusbarPrivate *priv;
};

struct _LapizStatusbarClass
{
        GtkStatusbarClass parent_class;
};

GType		 lapiz_statusbar_get_type		(void) G_GNUC_CONST;

GtkWidget	*lapiz_statusbar_new			(void);

void		 lapiz_statusbar_set_window_state	(LapizStatusbar   *statusbar,
							 LapizWindowState  state,
							 gint              num_of_errors);

void		 lapiz_statusbar_set_overwrite		(LapizStatusbar   *statusbar,
							 gboolean          overwrite);

void		 lapiz_statusbar_set_cursor_position	(LapizStatusbar   *statusbar,
							 gint              line,
							 gint              col);

void		 lapiz_statusbar_clear_overwrite 	(LapizStatusbar   *statusbar);

void		 lapiz_statusbar_flash_message		(LapizStatusbar   *statusbar,
							 guint             context_id,
							 const gchar      *format,
							 ...) G_GNUC_PRINTF(3, 4);

G_END_DECLS

#endif
