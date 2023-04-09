/*
 * lapiz-search-dialog.h
 * This file is part of lapiz
 *
 * Copyright (C) 2005 Paolo Maggi
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

#ifndef __LAPIZ_SEARCH_DIALOG_H__
#define __LAPIZ_SEARCH_DIALOG_H__

#include <ctk/ctk.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_SEARCH_DIALOG              (lapiz_search_dialog_get_type())
#define LAPIZ_SEARCH_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_SEARCH_DIALOG, LapizSearchDialog))
#define LAPIZ_SEARCH_DIALOG_CONST(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_SEARCH_DIALOG, LapizSearchDialog const))
#define LAPIZ_SEARCH_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_SEARCH_DIALOG, LapizSearchDialogClass))
#define LAPIZ_IS_SEARCH_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_SEARCH_DIALOG))
#define LAPIZ_IS_SEARCH_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_SEARCH_DIALOG))
#define LAPIZ_SEARCH_DIALOG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_SEARCH_DIALOG, LapizSearchDialogClass))

/* Private structure type */
typedef struct _LapizSearchDialogPrivate LapizSearchDialogPrivate;

/*
 * Main object structure
 */
typedef struct _LapizSearchDialog LapizSearchDialog;

struct _LapizSearchDialog
{
	GtkDialog dialog;

	/*< private > */
	LapizSearchDialogPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizSearchDialogClass LapizSearchDialogClass;

struct _LapizSearchDialogClass
{
	GtkDialogClass parent_class;

	/* Key bindings */
	gboolean (* show_replace) (LapizSearchDialog *dlg);
};

enum
{
	LAPIZ_SEARCH_DIALOG_FIND_RESPONSE = 100,
	LAPIZ_SEARCH_DIALOG_REPLACE_RESPONSE,
	LAPIZ_SEARCH_DIALOG_REPLACE_ALL_RESPONSE
};

/*
 * Public methods
 */
GType 		 lapiz_search_dialog_get_type 		(void) G_GNUC_CONST;

GtkWidget	*lapiz_search_dialog_new		(GtkWindow         *parent,
							 gboolean           show_replace);

void		 lapiz_search_dialog_present_with_time	(LapizSearchDialog *dialog,
							 guint32 timestamp);

gboolean	 lapiz_search_dialog_get_show_replace	(LapizSearchDialog *dialog);

void		 lapiz_search_dialog_set_show_replace	(LapizSearchDialog *dialog,
							 gboolean           show_replace);


void		 lapiz_search_dialog_set_search_text	(LapizSearchDialog *dialog,
							 const gchar       *text);
const gchar	*lapiz_search_dialog_get_search_text	(LapizSearchDialog *dialog);

void		 lapiz_search_dialog_set_replace_text	(LapizSearchDialog *dialog,
							 const gchar       *text);
const gchar	*lapiz_search_dialog_get_replace_text	(LapizSearchDialog *dialog);

void		 lapiz_search_dialog_set_match_case	(LapizSearchDialog *dialog,
							 gboolean           match_case);
gboolean	 lapiz_search_dialog_get_match_case	(LapizSearchDialog *dialog);

void		 lapiz_search_dialog_set_match_regex (LapizSearchDialog *dialog,
                             gboolean           match_case);
gboolean	 lapiz_search_dialog_get_match_regex (LapizSearchDialog *dialog);

void		 lapiz_search_dialog_set_entire_word	(LapizSearchDialog *dialog,
							 gboolean           entire_word);
gboolean	 lapiz_search_dialog_get_entire_word	(LapizSearchDialog *dialog);

void		 lapiz_search_dialog_set_backwards	(LapizSearchDialog *dialog,
							 gboolean           backwards);
gboolean	 lapiz_search_dialog_get_backwards	(LapizSearchDialog *dialog);

void		 lapiz_search_dialog_set_wrap_around	(LapizSearchDialog *dialog,
							 gboolean           wrap_around);
gboolean	 lapiz_search_dialog_get_wrap_around	(LapizSearchDialog *dialog);


void		lapiz_search_dialog_set_parse_escapes (LapizSearchDialog *dialog,
                                    		       gboolean           parse_escapes);
gboolean	lapiz_search_dialog_get_parse_escapes (LapizSearchDialog *dialog);

G_END_DECLS

#endif  /* __LAPIZ_SEARCH_DIALOG_H__  */
