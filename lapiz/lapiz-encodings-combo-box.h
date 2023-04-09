/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-encodings-combo-box.h
 * This file is part of lapiz
 *
 * Copyright (C) 2003-2005 - Paolo Maggi
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
 * Modified by the lapiz Team, 2003-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id: lapiz-encodings-option-menu.h 4429 2005-12-12 17:28:04Z pborelli $
 */

#ifndef __LAPIZ_ENCODINGS_COMBO_BOX_H__
#define __LAPIZ_ENCODINGS_COMBO_BOX_H__

#include <gtk/gtk.h>
#include <lapiz/lapiz-encodings.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_ENCODINGS_COMBO_BOX             (lapiz_encodings_combo_box_get_type ())
#define LAPIZ_ENCODINGS_COMBO_BOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_ENCODINGS_COMBO_BOX, LapizEncodingsComboBox))
#define LAPIZ_ENCODINGS_COMBO_BOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_ENCODINGS_COMBO_BOX, LapizEncodingsComboBoxClass))
#define LAPIZ_IS_ENCODINGS_COMBO_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_ENCODINGS_COMBO_BOX))
#define LAPIZ_IS_ENCODINGS_COMBO_BOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_ENCODINGS_COMBO_BOX))
#define LAPIZ_ENCODINGS_COMBO_BOX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_ENCODINGS_COMBO_BOX, LapizEncodingsComboBoxClass))


typedef struct _LapizEncodingsComboBox 	LapizEncodingsComboBox;
typedef struct _LapizEncodingsComboBoxClass 	LapizEncodingsComboBoxClass;

typedef struct _LapizEncodingsComboBoxPrivate	LapizEncodingsComboBoxPrivate;

struct _LapizEncodingsComboBox
{
	GtkComboBox			 parent;

	LapizEncodingsComboBoxPrivate	*priv;
};

struct _LapizEncodingsComboBoxClass
{
	GtkComboBoxClass		 parent_class;
};

GType		     lapiz_encodings_combo_box_get_type		(void) G_GNUC_CONST;

/* Constructor */
GtkWidget 	    *lapiz_encodings_combo_box_new 			(gboolean save_mode);

const LapizEncoding *lapiz_encodings_combo_box_get_selected_encoding	(LapizEncodingsComboBox *menu);
void		     lapiz_encodings_combo_box_set_selected_encoding	(LapizEncodingsComboBox *menu,
									 const LapizEncoding      *encoding);

G_END_DECLS

#endif /* __LAPIZ_ENCODINGS_COMBO_BOX_H__ */


