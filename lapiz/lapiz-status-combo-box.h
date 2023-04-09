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

#include <ctk/ctk.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_STATUS_COMBO_BOX		(lapiz_status_combo_box_get_type ())
#define LAPIZ_STATUS_COMBO_BOX(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_STATUS_COMBO_BOX, LapizStatusComboBox))
#define LAPIZ_STATUS_COMBO_BOX_CONST(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_STATUS_COMBO_BOX, LapizStatusComboBox const))
#define LAPIZ_STATUS_COMBO_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_STATUS_COMBO_BOX, LapizStatusComboBoxClass))
#define LAPIZ_IS_STATUS_COMBO_BOX(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_STATUS_COMBO_BOX))
#define LAPIZ_IS_STATUS_COMBO_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_STATUS_COMBO_BOX))
#define LAPIZ_STATUS_COMBO_BOX_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_STATUS_COMBO_BOX, LapizStatusComboBoxClass))

typedef struct _LapizStatusComboBox		LapizStatusComboBox;
typedef struct _LapizStatusComboBoxClass	LapizStatusComboBoxClass;
typedef struct _LapizStatusComboBoxPrivate	LapizStatusComboBoxPrivate;

struct _LapizStatusComboBox {
	CtkEventBox parent;

	LapizStatusComboBoxPrivate *priv;
};

struct _LapizStatusComboBoxClass {
	CtkEventBoxClass parent_class;

	void (*changed) (LapizStatusComboBox *combo,
			 CtkMenuItem         *item);
};

GType lapiz_status_combo_box_get_type 			(void) G_GNUC_CONST;
CtkWidget *lapiz_status_combo_box_new			(const gchar 		*label);

const gchar *lapiz_status_combo_box_get_label 		(LapizStatusComboBox 	*combo);
void lapiz_status_combo_box_set_label 			(LapizStatusComboBox 	*combo,
							 const gchar         	*label);

void lapiz_status_combo_box_add_item 			(LapizStatusComboBox 	*combo,
							 CtkMenuItem         	*item,
							 const gchar         	*text);
void lapiz_status_combo_box_remove_item			(LapizStatusComboBox    *combo,
							 CtkMenuItem            *item);

GList *lapiz_status_combo_box_get_items			(LapizStatusComboBox    *combo);
const gchar *lapiz_status_combo_box_get_item_text 	(LapizStatusComboBox	*combo,
							 CtkMenuItem		*item);
void lapiz_status_combo_box_set_item_text 		(LapizStatusComboBox	*combo,
							 CtkMenuItem		*item,
							 const gchar            *text);

void lapiz_status_combo_box_set_item			(LapizStatusComboBox	*combo,
							 CtkMenuItem		*item);

CtkLabel *lapiz_status_combo_box_get_item_label		(LapizStatusComboBox	*combo);

G_END_DECLS

#endif /* __LAPIZ_STATUS_COMBO_BOX_H__ */
