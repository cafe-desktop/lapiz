/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-spell-language-dialog.h
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

#ifndef __LAPIZ_SPELL_LANGUAGE_DIALOG_H__
#define __LAPIZ_SPELL_LANGUAGE_DIALOG_H__

#include <gtk/gtk.h>
#include "lapiz-spell-checker-language.h"

G_BEGIN_DECLS

#define LAPIZ_TYPE_SPELL_LANGUAGE_DIALOG              (lapiz_spell_language_dialog_get_type())
#define LAPIZ_SPELL_LANGUAGE_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_SPELL_LANGUAGE_DIALOG, LapizSpellLanguageDialog))
#define LAPIZ_SPELL_LANGUAGE_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_SPELL_LANGUAGE_DIALOG, LapizSpellLanguageDialogClass))
#define LAPIZ_IS_SPELL_LANGUAGE_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_SPELL_LANGUAGE_DIALOG))
#define LAPIZ_IS_SPELL_LANGUAGE_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_SPELL_LANGUAGE_DIALOG))
#define LAPIZ_SPELL_LANGUAGE_DIALOG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_SPELL_LANGUAGE_DIALOG, LapizSpellLanguageDialogClass))


typedef struct _LapizSpellLanguageDialog LapizSpellLanguageDialog;

typedef struct _LapizSpellLanguageDialogClass LapizSpellLanguageDialogClass;

struct _LapizSpellLanguageDialogClass
{
	GtkDialogClass parent_class;
};

GType		 lapiz_spell_language_dialog_get_type		(void) G_GNUC_CONST;

GtkWidget	*lapiz_spell_language_dialog_new		(GtkWindow			 *parent,
								 const LapizSpellCheckerLanguage *cur_lang,
								 const gchar *data_dir);

const LapizSpellCheckerLanguage *
		 lapiz_spell_language_get_selected_language	(LapizSpellLanguageDialog *dlg);

G_END_DECLS

#endif  /* __LAPIZ_SPELL_LANGUAGE_DIALOG_H__ */

