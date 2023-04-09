/*
 * lapiz-view.h
 * This file is part of lapiz
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
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
 * Modified by the lapiz Team, 1998-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 */

#ifndef __LAPIZ_VIEW_H__
#define __LAPIZ_VIEW_H__

#include <gtk/gtk.h>

#include <lapiz/lapiz-document.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_VIEW            (lapiz_view_get_type ())
#define LAPIZ_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_VIEW, LapizView))
#define LAPIZ_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_VIEW, LapizViewClass))
#define LAPIZ_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_VIEW))
#define LAPIZ_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_VIEW))
#define LAPIZ_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_VIEW, LapizViewClass))

/* Private structure type */
typedef struct _LapizViewPrivate	LapizViewPrivate;

/*
 * Main object structure
 */
typedef struct _LapizView		LapizView;

struct _LapizView
{
	GtkSourceView view;

	/*< private > */
	LapizViewPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizViewClass		LapizViewClass;

struct _LapizViewClass
{
	GtkSourceViewClass parent_class;

	/* FIXME: Do we need placeholders ? */

	/* Key bindings */
	gboolean (* start_interactive_search)	(LapizView       *view);
	gboolean (* start_interactive_goto_line)(LapizView       *view);
	gboolean (* reset_searched_text)	(LapizView       *view);

	void	 (* drop_uris)			(LapizView	 *view,
						 gchar          **uri_list);
};

/*
 * Public methods
 */
GType		 lapiz_view_get_type     	(void) G_GNUC_CONST;

GtkWidget	*lapiz_view_new			(LapizDocument   *doc);

void		 lapiz_view_cut_clipboard 	(LapizView       *view);
void		 lapiz_view_copy_clipboard 	(LapizView       *view);
void		 lapiz_view_paste_clipboard	(LapizView       *view);
void		 lapiz_view_delete_selection	(LapizView       *view);
void		 lapiz_view_select_all		(LapizView       *view);

void		 lapiz_view_scroll_to_cursor 	(LapizView       *view);

void		 lapiz_override_font		(const gchar          *item,
						 GtkWidget            *widget,
						 PangoFontDescription *font);

void 		 lapiz_view_set_font		(LapizView       *view,
						 gboolean         def,
						 const gchar     *font_name);

#ifdef GTK_SOURCE_VERSION_3_24
void
lapiz_set_source_space_drawer_by_level (GtkSourceView          *view,
                                        gint                    level,
                                        GtkSourceSpaceTypeFlags type);
#endif


G_END_DECLS

#endif /* __LAPIZ_VIEW_H__ */
