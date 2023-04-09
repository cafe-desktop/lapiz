/*
 * lapiz-encodings.h
 * This file is part of lapiz
 *
 * Copyright (C) 2002-2005 Paolo Maggi
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
 * Modified by the lapiz Team, 2002-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __LAPIZ_ENCODINGS_H__
#define __LAPIZ_ENCODINGS_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _PlumaEncoding PlumaEncoding;

#define LAPIZ_TYPE_ENCODING     (lapiz_encoding_get_type ())

GType              	 lapiz_encoding_get_type (void) G_GNUC_CONST;

const PlumaEncoding	*lapiz_encoding_get_from_charset (const gchar         *charset);
const PlumaEncoding	*lapiz_encoding_get_from_index	 (gint                 index);

gchar 			*lapiz_encoding_to_string	 (const PlumaEncoding *enc);

const gchar		*lapiz_encoding_get_name	 (const PlumaEncoding *enc);
const gchar		*lapiz_encoding_get_charset	 (const PlumaEncoding *enc);

const PlumaEncoding 	*lapiz_encoding_get_utf8	 (void);
const PlumaEncoding 	*lapiz_encoding_get_current	 (void);

/* These should not be used, they are just to make python bindings happy */
PlumaEncoding		*lapiz_encoding_copy		 (const PlumaEncoding *enc);
void               	 lapiz_encoding_free		 (PlumaEncoding       *enc);

G_END_DECLS

#endif  /* __LAPIZ_ENCODINGS_H__ */
