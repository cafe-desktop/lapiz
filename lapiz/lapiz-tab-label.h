/*
 * lapiz-tab-label.h
 * This file is part of lapiz
 *
 * Copyright (C) 2010 - Paolo Borelli
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

#ifndef __LAPIZ_TAB_LABEL_H__
#define __LAPIZ_TAB_LABEL_H__

#include <gtk/gtk.h>
#include <lapiz/lapiz-tab.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_TAB_LABEL		(lapiz_tab_label_get_type ())
#define LAPIZ_TAB_LABEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_TAB_LABEL, PlumaTabLabel))
#define LAPIZ_TAB_LABEL_CONST(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_TAB_LABEL, PlumaTabLabel const))
#define LAPIZ_TAB_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_TAB_LABEL, PlumaTabLabelClass))
#define LAPIZ_IS_TAB_LABEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_TAB_LABEL))
#define LAPIZ_IS_TAB_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_TAB_LABEL))
#define LAPIZ_TAB_LABEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_TAB_LABEL, PlumaTabLabelClass))

typedef struct _PlumaTabLabel		PlumaTabLabel;
typedef struct _PlumaTabLabelClass	PlumaTabLabelClass;
typedef struct _PlumaTabLabelPrivate	PlumaTabLabelPrivate;

struct _PlumaTabLabel {
	GtkBox parent;

	PlumaTabLabelPrivate *priv;
};

struct _PlumaTabLabelClass {
	GtkBoxClass parent_class;

	void (* close_clicked)  (PlumaTabLabel *tab_label);
};

GType		 lapiz_tab_label_get_type (void) G_GNUC_CONST;

GtkWidget 	*lapiz_tab_label_new (PlumaTab *tab);

PlumaTab	*lapiz_tab_label_get_tab (PlumaTabLabel *tab_label);

void		lapiz_tab_label_set_close_button_sensitive (PlumaTabLabel *tab_label,
							    gboolean       sensitive);

G_END_DECLS

#endif /* __LAPIZ_TAB_LABEL_H__ */
