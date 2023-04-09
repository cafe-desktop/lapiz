/*
 * lapiz-encodings-dialog.h
 * This file is part of lapiz
 *
 * Copyright (C) 2003-2005 Paolo Maggi
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
 * $Id$
 */

#ifndef __LAPIZ_ENCODINGS_DIALOG_H__
#define __LAPIZ_ENCODINGS_DIALOG_H__

#include <ctk/ctk.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_ENCODINGS_DIALOG              (lapiz_encodings_dialog_get_type())
#define LAPIZ_ENCODINGS_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_ENCODINGS_DIALOG, LapizEncodingsDialog))
#define LAPIZ_ENCODINGS_DIALOG_CONST(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_ENCODINGS_DIALOG, LapizEncodingsDialog const))
#define LAPIZ_ENCODINGS_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_ENCODINGS_DIALOG, LapizEncodingsDialogClass))
#define LAPIZ_IS_ENCODINGS_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_ENCODINGS_DIALOG))
#define LAPIZ_IS_ENCODINGS_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_ENCODINGS_DIALOG))
#define LAPIZ_ENCODINGS_DIALOG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_ENCODINGS_DIALOG, LapizEncodingsDialogClass))


/* Private structure type */
typedef struct _LapizEncodingsDialogPrivate LapizEncodingsDialogPrivate;

/*
 * Main object structure
 */
typedef struct _LapizEncodingsDialog LapizEncodingsDialog;

struct _LapizEncodingsDialog
{
	CtkDialog dialog;

	/*< private > */
	LapizEncodingsDialogPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizEncodingsDialogClass LapizEncodingsDialogClass;

struct _LapizEncodingsDialogClass
{
	CtkDialogClass parent_class;
};

/*
 * Public methods
 */
GType		 lapiz_encodings_dialog_get_type	(void) G_GNUC_CONST;

CtkWidget	*lapiz_encodings_dialog_new		(void);

G_END_DECLS

#endif /* __LAPIZ_ENCODINGS_DIALOG_H__ */

