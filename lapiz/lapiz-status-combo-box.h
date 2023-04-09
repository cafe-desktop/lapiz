/*
 * lapiz-status-combo-box.h
 * This file is part of lapiz
 *
 * Copyright (C) 2008 - Jesse van den Kieboom
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

#ifndef __LAPIZ_STATUS_COMBO_BOX_H__
#define __LAPIZ_STATUS_COMBO_BOX_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_STATUS_COMBO_BOX		(lapiz_status_combo_box_get_type ())
#define LAPIZ_STATUS_COMBO_BOX(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_STATUS_COMBO_BOX, PlumaStatusComboBox))
#define LAPIZ_STATUS_COMBO_BOX_CONST(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_STATUS_COMBO_BOX, PlumaStatusComboBox const))
#define LAPIZ_STATUS_COMBO_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_STATUS_COMBO_BOX, PlumaStatusComboBoxClass))
#define LAPIZ_IS_STATUS_COMBO_BOX(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_STATUS_COMBO_BOX))
#define LAPIZ_IS_STATUS_COMBO_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_STATUS_COMBO_BOX))
#define LAPIZ_STATUS_COMBO_BOX_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_STATUS_COMBO_BOX, PlumaStatusComboBoxClass))

typedef struct _PlumaStatusComboBox		PlumaStatusComboBox;
typedef struct _PlumaStatusComboBoxClass	PlumaStatusComboBoxClass;
typedef struct _PlumaStatusComboBoxPrivate	PlumaStatusComboBoxPrivate;

struct _PlumaStatusComboBox {
	GtkEventBox parent;

	PlumaStatusComboBoxPrivate *priv;
};

struct _PlumaStatusComboBoxClass {
	GtkEventBoxClass parent_class;

	void (*changed) (PlumaStatusComboBox *combo,
			 GtkMenuItem         *item);
};

GType lapiz_status_combo_box_get_type 			(void) G_GNUC_CONST;
GtkWidget *lapiz_status_combo_box_new			(const gchar 		*label);

const gchar *lapiz_status_combo_box_get_label 		(PlumaStatusComboBox 	*combo);
void lapiz_status_combo_box_set_label 			(PlumaStatusComboBox 	*combo,
							 const gchar         	*label);

void lapiz_status_combo_box_add_item 			(PlumaStatusComboBox 	*combo,
							 GtkMenuItem         	*item,
							 const gchar         	*text);
void lapiz_status_combo_box_remove_item			(PlumaStatusComboBox    *combo,
							 GtkMenuItem            *item);

GList *lapiz_status_combo_box_get_items			(PlumaStatusComboBox    *combo);
const gchar *lapiz_status_combo_box_get_item_text 	(PlumaStatusComboBox	*combo,
							 GtkMenuItem		*item);
void lapiz_status_combo_box_set_item_text 		(PlumaStatusComboBox	*combo,
							 GtkMenuItem		*item,
							 const gchar            *text);

void lapiz_status_combo_box_set_item			(PlumaStatusComboBox	*combo,
							 GtkMenuItem		*item);

GtkLabel *lapiz_status_combo_box_get_item_label		(PlumaStatusComboBox	*combo);

G_END_DECLS

#endif /* __LAPIZ_STATUS_COMBO_BOX_H__ */
