/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-spell-checker.h
 * This file is part of lapiz
 *
 * Copyright (C) 2002-2006 Paolo Maggi
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

#ifndef __LAPIZ_SPELL_CHECKER_H__
#define __LAPIZ_SPELL_CHECKER_H__

#include <glib.h>
#include <glib-object.h>

#include "lapiz-spell-checker-language.h"

G_BEGIN_DECLS

#define LAPIZ_TYPE_SPELL_CHECKER            (lapiz_spell_checker_get_type ())
#define LAPIZ_SPELL_CHECKER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_SPELL_CHECKER, LapizSpellChecker))
#define LAPIZ_SPELL_CHECKER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_SPELL_CHECKER, LapizSpellChecker))
#define LAPIZ_IS_SPELL_CHECKER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_SPELL_CHECKER))
#define LAPIZ_IS_SPELL_CHECKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_SPELL_CHECKER))
#define LAPIZ_SPELL_CHECKER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_SPELL_CHECKER, LapizSpellChecker))

typedef struct _LapizSpellChecker LapizSpellChecker;

typedef struct _LapizSpellCheckerClass LapizSpellCheckerClass;

struct _LapizSpellCheckerClass
{
	GObjectClass parent_class;

	/* Signals */
	void (*add_word_to_personal) (LapizSpellChecker               *spell,
				      const gchar                     *word,
				      gint                             len);

	void (*add_word_to_session)  (LapizSpellChecker               *spell,
				      const gchar                     *word,
				      gint                             len);

	void (*set_language)         (LapizSpellChecker               *spell,
				      const LapizSpellCheckerLanguage *lang);

	void (*clear_session)	     (LapizSpellChecker               *spell);
};


GType        		 lapiz_spell_checker_get_type		(void) G_GNUC_CONST;

/* Constructors */
LapizSpellChecker	*lapiz_spell_checker_new		(void);

gboolean		 lapiz_spell_checker_set_language 	(LapizSpellChecker               *spell,
								 const LapizSpellCheckerLanguage *lang);
const LapizSpellCheckerLanguage
			*lapiz_spell_checker_get_language 	(LapizSpellChecker               *spell);

gboolean		 lapiz_spell_checker_check_word 	(LapizSpellChecker               *spell,
								 const gchar                     *word,
								 gssize                           len);

GSList 			*lapiz_spell_checker_get_suggestions 	(LapizSpellChecker               *spell,
								 const gchar                     *word,
								 gssize                           len);

gboolean		 lapiz_spell_checker_add_word_to_personal
								(LapizSpellChecker               *spell,
								 const gchar                     *word,
								 gssize                           len);

gboolean		 lapiz_spell_checker_add_word_to_session
								(LapizSpellChecker               *spell,
								 const gchar                     *word,
								 gssize                           len);

gboolean		 lapiz_spell_checker_clear_session 	(LapizSpellChecker               *spell);

gboolean		 lapiz_spell_checker_set_correction 	(LapizSpellChecker               *spell,
								 const gchar                     *word,
								 gssize                           w_len,
								 const gchar                     *replacement,
								 gssize                           r_len);
G_END_DECLS

#endif  /* __LAPIZ_SPELL_CHECKER_H__ */

