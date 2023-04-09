/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * lapiztextregion.h - CtkTextMark based region utility functions
 *
 * This file is part of the CtkSourceView widget
 *
 * Copyright (C) 2002 Gustavo Giráldez <gustavo.giraldez@gmx.net>
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

#ifndef __LAPIZ_TEXT_REGION_H__
#define __LAPIZ_TEXT_REGION_H__

#include <ctk/ctk.h>

G_BEGIN_DECLS

typedef struct _LapizTextRegion		LapizTextRegion;
typedef struct _LapizTextRegionIterator	LapizTextRegionIterator;

struct _LapizTextRegionIterator {
	/* LapizTextRegionIterator is an opaque datatype; ignore all these fields.
	 * Initialize the iter with lapiz_text_region_get_iterator
	 * function
	 */
	/*< private >*/
	gpointer dummy1;
	guint32  dummy2;
	gpointer dummy3;
};

LapizTextRegion *lapiz_text_region_new                          (CtkTextBuffer *buffer);
void           lapiz_text_region_destroy                      (LapizTextRegion *region,
							     gboolean       delete_marks);

CtkTextBuffer *lapiz_text_region_get_buffer                   (LapizTextRegion *region);

void           lapiz_text_region_add                          (LapizTextRegion     *region,
							     const CtkTextIter *_start,
							     const CtkTextIter *_end);

void           lapiz_text_region_subtract                     (LapizTextRegion     *region,
							     const CtkTextIter *_start,
							     const CtkTextIter *_end);

gint           lapiz_text_region_subregions                   (LapizTextRegion *region);

gboolean       lapiz_text_region_nth_subregion                (LapizTextRegion *region,
							     guint          subregion,
							     CtkTextIter   *start,
							     CtkTextIter   *end);

LapizTextRegion *lapiz_text_region_intersect                    (LapizTextRegion     *region,
							     const CtkTextIter *_start,
							     const CtkTextIter *_end);

void           lapiz_text_region_get_iterator                 (LapizTextRegion         *region,
                                                             LapizTextRegionIterator *iter,
                                                             guint                  start);

gboolean       lapiz_text_region_iterator_is_end              (LapizTextRegionIterator *iter);

/* Returns FALSE if iterator is the end iterator */
gboolean       lapiz_text_region_iterator_next	            (LapizTextRegionIterator *iter);

void           lapiz_text_region_iterator_get_subregion       (LapizTextRegionIterator *iter,
							     CtkTextIter           *start,
							     CtkTextIter           *end);

void           lapiz_text_region_debug_print                  (LapizTextRegion *region);

G_END_DECLS

#endif /* __LAPIZ_TEXT_REGION_H__ */
