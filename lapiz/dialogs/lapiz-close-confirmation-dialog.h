/*
 * lapiz-close-confirmation-dialog.h
 * This file is part of lapiz
 *
 * Copyright (C) 2004-2005 GNOME Foundation
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
 * Modified by the lapiz Team, 2004-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 */

#ifndef __LAPIZ_CLOSE_CONFIRMATION_DIALOG_H__
#define __LAPIZ_CLOSE_CONFIRMATION_DIALOG_H__

#include <glib.h>
#include <gtk/gtk.h>

#include <lapiz/lapiz-document.h>

#define LAPIZ_TYPE_CLOSE_CONFIRMATION_DIALOG		(lapiz_close_confirmation_dialog_get_type ())
#define LAPIZ_CLOSE_CONFIRMATION_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_CLOSE_CONFIRMATION_DIALOG, LapizCloseConfirmationDialog))
#define LAPIZ_CLOSE_CONFIRMATION_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_CLOSE_CONFIRMATION_DIALOG, LapizCloseConfirmationDialogClass))
#define LAPIZ_IS_CLOSE_CONFIRMATION_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_CLOSE_CONFIRMATION_DIALOG))
#define LAPIZ_IS_CLOSE_CONFIRMATION_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_CLOSE_CONFIRMATION_DIALOG))
#define LAPIZ_CLOSE_CONFIRMATION_DIALOG_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj),LAPIZ_TYPE_CLOSE_CONFIRMATION_DIALOG, LapizCloseConfirmationDialogClass))

typedef struct _LapizCloseConfirmationDialog 		LapizCloseConfirmationDialog;
typedef struct _LapizCloseConfirmationDialogClass 	LapizCloseConfirmationDialogClass;
typedef struct _LapizCloseConfirmationDialogPrivate 	LapizCloseConfirmationDialogPrivate;

struct _LapizCloseConfirmationDialog
{
	GtkDialog parent;

	/*< private > */
	LapizCloseConfirmationDialogPrivate *priv;
};

struct _LapizCloseConfirmationDialogClass
{
	GtkDialogClass parent_class;
};

GType 		 lapiz_close_confirmation_dialog_get_type		(void) G_GNUC_CONST;

GtkWidget	*lapiz_close_confirmation_dialog_new			(GtkWindow     *parent,
									 GList         *unsaved_documents,
									 gboolean       logout_mode);
GtkWidget 	*lapiz_close_confirmation_dialog_new_single 		(GtkWindow     *parent,
									 LapizDocument *doc,
 									 gboolean       logout_mode);

const GList	*lapiz_close_confirmation_dialog_get_unsaved_documents  (LapizCloseConfirmationDialog *dlg);

GList		*lapiz_close_confirmation_dialog_get_selected_documents	(LapizCloseConfirmationDialog *dlg);

#endif /* __LAPIZ_CLOSE_CONFIRMATION_DIALOG_H__ */

