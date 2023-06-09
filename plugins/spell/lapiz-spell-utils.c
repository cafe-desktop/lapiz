/*
 * lapiz-spell-utils.c
 * This file is part of lapiz
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
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
 * Boston, MA  02110-1301  USA
 */

#include <string.h>

#include "lapiz-spell-utils.h"
#include <ctksourceview/ctksource.h>

gboolean
lapiz_spell_utils_is_digit (const char *text, gssize length)
{
	gunichar c;
	const gchar *p;
	const gchar *end;

	g_return_val_if_fail (text != NULL, FALSE);

	if (length < 0)
		length = strlen (text);

	p = text;
	end = text + length;

	while (p != end) {
		const gchar *next;
		next = g_utf8_next_char (p);

		c = g_utf8_get_char (p);

		if (!g_unichar_isdigit (c) && c != '.' && c != ',')
			return FALSE;

		p = next;
	}

	return TRUE;
}

gboolean
lapiz_spell_utils_skip_no_spell_check (CtkTextIter *start,
                                       CtkTextIter *end)
{
	CtkSourceBuffer *buffer = CTK_SOURCE_BUFFER (ctk_text_iter_get_buffer (start));

	while (ctk_source_buffer_iter_has_context_class (buffer, start, "no-spell-check"))
	{
		CtkTextIter last = *start;

		if (!ctk_source_buffer_iter_forward_to_context_class_toggle (buffer, start, "no-spell-check"))
		{
			return FALSE;
		}

		if (ctk_text_iter_compare (start, &last) <= 0)
		{
			return FALSE;
		}

		ctk_text_iter_forward_word_end (start);
		ctk_text_iter_backward_word_start (start);

		if (ctk_text_iter_compare (start, &last) <= 0)
		{
			return FALSE;
		}

		if (ctk_text_iter_compare (start, end) >= 0)
		{
			return FALSE;
		}
	}

	return TRUE;
}

