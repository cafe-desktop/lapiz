/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-preferences-dialog.c
 * This file is part of lapiz
 *
 * Copyright (C) 2001-2005 Paolo Maggi
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
 * Modified by the lapiz Team, 2003. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __LAPIZ_PREFERENCES_DIALOG_H__
#define __LAPIZ_PREFERENCES_DIALOG_H__

#include "lapiz-window.h"

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_PREFERENCES_DIALOG              (lapiz_preferences_dialog_get_type())
#define LAPIZ_PREFERENCES_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_PREFERENCES_DIALOG, PlumaPreferencesDialog))
#define LAPIZ_PREFERENCES_DIALOG_CONST(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_PREFERENCES_DIALOG, PlumaPreferencesDialog const))
#define LAPIZ_PREFERENCES_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_PREFERENCES_DIALOG, PlumaPreferencesDialogClass))
#define LAPIZ_IS_PREFERENCES_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_PREFERENCES_DIALOG))
#define LAPIZ_IS_PREFERENCES_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_PREFERENCES_DIALOG))
#define LAPIZ_PREFERENCES_DIALOG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_PREFERENCES_DIALOG, PlumaPreferencesDialogClass))


/* Private structure type */
typedef struct _PlumaPreferencesDialogPrivate PlumaPreferencesDialogPrivate;

/*
 * Main object structure
 */
typedef struct _PlumaPreferencesDialog PlumaPreferencesDialog;

struct _PlumaPreferencesDialog
{
	GtkDialog dialog;

	/*< private > */
	PlumaPreferencesDialogPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _PlumaPreferencesDialogClass PlumaPreferencesDialogClass;

struct _PlumaPreferencesDialogClass
{
	GtkDialogClass parent_class;
};

/*
 * Public methods
 */
GType		 lapiz_preferences_dialog_get_type	(void) G_GNUC_CONST;

void		 lapiz_show_preferences_dialog		(PlumaWindow *parent);

G_END_DECLS

#endif /* __LAPIZ_PREFERENCES_DIALOG_H__ */

