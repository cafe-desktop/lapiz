/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-spell-checker-dialog.h
 * This file is part of lapiz
 *
 * Copyright (C) 2002 Paolo Maggi
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
 * Modified by the lapiz Team, 2002. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 */

#ifndef __LAPIZ_SPELL_CHECKER_DIALOG_H__
#define __LAPIZ_SPELL_CHECKER_DIALOG_H__

#include <ctk/ctk.h>
#include "lapiz-spell-checker.h"

G_BEGIN_DECLS

#define LAPIZ_TYPE_SPELL_CHECKER_DIALOG            (lapiz_spell_checker_dialog_get_type ())
#define LAPIZ_SPELL_CHECKER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_SPELL_CHECKER_DIALOG, LapizSpellCheckerDialog))
#define LAPIZ_SPELL_CHECKER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_SPELL_CHECKER_DIALOG, LapizSpellCheckerDialog))
#define LAPIZ_IS_SPELL_CHECKER_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_SPELL_CHECKER_DIALOG))
#define LAPIZ_IS_SPELL_CHECKER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_SPELL_CHECKER_DIALOG))
#define LAPIZ_SPELL_CHECKER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_SPELL_CHECKER_DIALOG, LapizSpellCheckerDialog))


typedef struct _LapizSpellCheckerDialog LapizSpellCheckerDialog;

typedef struct _LapizSpellCheckerDialogClass LapizSpellCheckerDialogClass;

struct _LapizSpellCheckerDialogClass
{
	GtkWindowClass parent_class;

	/* Signals */
	void		(*ignore)		(LapizSpellCheckerDialog *dlg,
						 const gchar *word);
	void		(*ignore_all)		(LapizSpellCheckerDialog *dlg,
						 const gchar *word);
	void		(*change)		(LapizSpellCheckerDialog *dlg,
						 const gchar *word,
						 const gchar *change_to);
	void		(*change_all)		(LapizSpellCheckerDialog *dlg,
						 const gchar *word,
						 const gchar *change_to);
	void		(*add_word_to_personal)	(LapizSpellCheckerDialog *dlg,
						 const gchar *word);

};

GType        		 lapiz_spell_checker_dialog_get_type	(void) G_GNUC_CONST;

/* Constructors */
GtkWidget		*lapiz_spell_checker_dialog_new		(const gchar *data_dir);
GtkWidget		*lapiz_spell_checker_dialog_new_from_spell_checker
								(LapizSpellChecker *spell,
								 const gchar *data_dir);

void 			 lapiz_spell_checker_dialog_set_spell_checker
								(LapizSpellCheckerDialog *dlg,
								 LapizSpellChecker *spell);
void			 lapiz_spell_checker_dialog_set_misspelled_word
								(LapizSpellCheckerDialog *dlg,
								 const gchar* word,
								 gint len);

void 			 lapiz_spell_checker_dialog_set_completed
								(LapizSpellCheckerDialog *dlg);

G_END_DECLS

#endif  /* __LAPIZ_SPELL_CHECKER_DIALOG_H__ */

