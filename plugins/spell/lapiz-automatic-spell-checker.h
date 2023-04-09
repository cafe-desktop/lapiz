/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-automatic-spell-checker.h
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

/* This is a modified version of ctkspell 2.0.2  (ctkspell.sf.net) */
/* ctkspell - a spell-checking addon for CTK's TextView widget
 * Copyright (c) 2002 Evan Martin.
 */

#ifndef __LAPIZ_AUTOMATIC_SPELL_CHECKER_H__
#define __LAPIZ_AUTOMATIC_SPELL_CHECKER_H__

#include <lapiz/lapiz-document.h>
#include <lapiz/lapiz-view.h>

#include "lapiz-spell-checker.h"

typedef struct _LapizAutomaticSpellChecker LapizAutomaticSpellChecker;

LapizAutomaticSpellChecker	*lapiz_automatic_spell_checker_new (
							LapizDocument 			*doc,
							LapizSpellChecker		*checker);

LapizAutomaticSpellChecker	*lapiz_automatic_spell_checker_get_from_document (
							const LapizDocument 		*doc);

void				 lapiz_automatic_spell_checker_free (
							LapizAutomaticSpellChecker 	*spell);

void 				 lapiz_automatic_spell_checker_attach_view (
							LapizAutomaticSpellChecker 	*spell,
							LapizView 			*view);

void 				 lapiz_automatic_spell_checker_detach_view (
							LapizAutomaticSpellChecker 	*spell,
							LapizView 			*view);

void				 lapiz_automatic_spell_checker_recheck_all (
							LapizAutomaticSpellChecker 	*spell);

#endif  /* __LAPIZ_AUTOMATIC_SPELL_CHECKER_H__ */

