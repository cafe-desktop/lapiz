/*
 * lapiz-file-chooser-dialog.h
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

#ifndef __LAPIZ_FILE_CHOOSER_DIALOG_H__
#define __LAPIZ_FILE_CHOOSER_DIALOG_H__

#include <ctk/ctk.h>

#include <lapiz/lapiz-encodings.h>
#include <lapiz/lapiz-enum-types.h>
#include <lapiz/lapiz-document.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_FILE_CHOOSER_DIALOG             (lapiz_file_chooser_dialog_get_type ())
#define LAPIZ_FILE_CHOOSER_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_CHOOSER_DIALOG, LapizFileChooserDialog))
#define LAPIZ_FILE_CHOOSER_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_FILE_CHOOSER_DIALOG, LapizFileChooserDialogClass))
#define LAPIZ_IS_FILE_CHOOSER_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_FILE_CHOOSER_DIALOG))
#define LAPIZ_IS_FILE_CHOOSER_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_FILE_CHOOSER_DIALOG))
#define LAPIZ_FILE_CHOOSER_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_FILE_CHOOSER_DIALOG, LapizFileChooserDialogClass))

typedef struct _LapizFileChooserDialog      LapizFileChooserDialog;
typedef struct _LapizFileChooserDialogClass LapizFileChooserDialogClass;

typedef struct _LapizFileChooserDialogPrivate LapizFileChooserDialogPrivate;

struct _LapizFileChooserDialogClass
{
	CtkFileChooserDialogClass parent_class;
};

struct _LapizFileChooserDialog
{
	CtkFileChooserDialog parent_instance;

	LapizFileChooserDialogPrivate *priv;
};

GType		 lapiz_file_chooser_dialog_get_type	(void) G_GNUC_CONST;

CtkWidget	*lapiz_file_chooser_dialog_new		(const gchar            *title,
							 CtkWindow              *parent,
							 CtkFileChooserAction    action,
							 const LapizEncoding    *encoding,
							 const gchar            *first_button_text,
							 ...);

void		 lapiz_file_chooser_dialog_set_encoding (LapizFileChooserDialog *dialog,
							 const LapizEncoding    *encoding);

const LapizEncoding
		*lapiz_file_chooser_dialog_get_encoding (LapizFileChooserDialog *dialog);

void		 lapiz_file_chooser_dialog_set_newline_type (LapizFileChooserDialog  *dialog,
							     LapizDocumentNewlineType newline_type);

LapizDocumentNewlineType
		 lapiz_file_chooser_dialog_get_newline_type (LapizFileChooserDialog *dialog);

G_END_DECLS

#endif /* __LAPIZ_FILE_CHOOSER_DIALOG_H__ */
