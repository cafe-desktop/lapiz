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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "lapiztextregion.h"


#undef ENABLE_DEBUG
/*
#define ENABLE_DEBUG
*/

#ifdef ENABLE_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

typedef struct _Subregion {
	CtkTextMark *start;
	CtkTextMark *end;
} Subregion;

struct _LapizTextRegion {
	CtkTextBuffer *buffer;
	GList         *subregions;
	guint32        time_stamp;
};

typedef struct _LapizTextRegionIteratorReal LapizTextRegionIteratorReal;

struct _LapizTextRegionIteratorReal {
	LapizTextRegion *region;
	guint32        region_time_stamp;

	GList         *subregions;
};


/* ----------------------------------------------------------------------
   Private interface
   ---------------------------------------------------------------------- */

/* Find and return a subregion node which contains the given text
   iter.  If left_side is TRUE, return the subregion which contains
   the text iter or which is the leftmost; else return the rightmost
   subregion */
static GList *
find_nearest_subregion (LapizTextRegion     *region,
			const CtkTextIter *iter,
			GList             *begin,
			gboolean           leftmost,
			gboolean           include_edges)
{
	GList *l, *retval;

	g_return_val_if_fail (region != NULL && iter != NULL, NULL);

	if (!begin)
		begin = region->subregions;

	if (begin)
		retval = begin->prev;
	else
		retval = NULL;

	for (l = begin; l; l = l->next) {
		CtkTextIter sr_iter;
		Subregion *sr = l->data;
		gint cmp;

		if (!leftmost) {
			ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_iter, sr->end);
			cmp = ctk_text_iter_compare (iter, &sr_iter);
			if (cmp < 0 || (cmp == 0 && include_edges)) {
				retval = l;
				break;
			}

		} else {
			ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_iter, sr->start);
			cmp = ctk_text_iter_compare (iter, &sr_iter);
			if (cmp > 0 || (cmp == 0 && include_edges))
				retval = l;
			else
				break;
		}
	}
	return retval;
}

/* ----------------------------------------------------------------------
   Public interface
   ---------------------------------------------------------------------- */

LapizTextRegion *
lapiz_text_region_new (CtkTextBuffer *buffer)
{
	LapizTextRegion *region;

	g_return_val_if_fail (buffer != NULL, NULL);

	region = g_new (LapizTextRegion, 1);
	region->buffer = buffer;
	region->subregions = NULL;
	region->time_stamp = 0;

	return region;
}

void
lapiz_text_region_destroy (LapizTextRegion *region, gboolean delete_marks)
{
	g_return_if_fail (region != NULL);

	while (region->subregions) {
		Subregion *sr = region->subregions->data;
		if (delete_marks) {
			ctk_text_buffer_delete_mark (region->buffer, sr->start);
			ctk_text_buffer_delete_mark (region->buffer, sr->end);
		}
		g_free (sr);
		region->subregions = g_list_delete_link (region->subregions,
							 region->subregions);
	}
	region->buffer = NULL;
	region->time_stamp = 0;

	g_free (region);
}

CtkTextBuffer *
lapiz_text_region_get_buffer (LapizTextRegion *region)
{
	g_return_val_if_fail (region != NULL, NULL);

	return region->buffer;
}

static void
lapiz_text_region_clear_zero_length_subregions (LapizTextRegion *region)
{
	CtkTextIter start, end;
	GList *node;

	g_return_if_fail (region != NULL);

	for (node = region->subregions; node; ) {
		Subregion *sr = node->data;
		ctk_text_buffer_get_iter_at_mark (region->buffer, &start, sr->start);
		ctk_text_buffer_get_iter_at_mark (region->buffer, &end, sr->end);
		if (ctk_text_iter_equal (&start, &end)) {
			ctk_text_buffer_delete_mark (region->buffer, sr->start);
			ctk_text_buffer_delete_mark (region->buffer, sr->end);
			g_free (sr);
			if (node == region->subregions)
				region->subregions = node = g_list_delete_link (node, node);
			else
				node = g_list_delete_link (node, node);

			++region->time_stamp;

		} else {
			node = node->next;
		}
	}
}

void
lapiz_text_region_add (LapizTextRegion     *region,
		     const CtkTextIter *_start,
		     const CtkTextIter *_end)
{
	GList *start_node, *end_node;
	CtkTextIter start, end;

	g_return_if_fail (region != NULL && _start != NULL && _end != NULL);

	start = *_start;
	end = *_end;

	DEBUG (g_print ("---\n"));
	DEBUG (lapiz_text_region_debug_print (region));
	DEBUG (g_message ("region_add (%d, %d)",
			  ctk_text_iter_get_offset (&start),
			  ctk_text_iter_get_offset (&end)));

	ctk_text_iter_order (&start, &end);

	/* don't add zero-length regions */
	if (ctk_text_iter_equal (&start, &end))
		return;

	/* find bounding subregions */
	start_node = find_nearest_subregion (region, &start, NULL, FALSE, TRUE);
	end_node = find_nearest_subregion (region, &end, start_node, TRUE, TRUE);

	if (start_node == NULL || end_node == NULL || end_node == start_node->prev) {
		/* create the new subregion */
		Subregion *sr = g_new0 (Subregion, 1);
		sr->start = ctk_text_buffer_create_mark (region->buffer, NULL, &start, TRUE);
		sr->end = ctk_text_buffer_create_mark (region->buffer, NULL, &end, FALSE);

		if (start_node == NULL) {
			/* append the new region */
			region->subregions = g_list_append (region->subregions, sr);

		} else if (end_node == NULL) {
			/* prepend the new region */
			region->subregions = g_list_prepend (region->subregions, sr);

		} else {
			/* we are in the middle of two subregions */
			region->subregions = g_list_insert_before (region->subregions,
								   start_node, sr);
		}
	}
	else {
		CtkTextIter iter;
		Subregion *sr = start_node->data;
		if (start_node != end_node) {
			/* we need to merge some subregions */
			GList *l = start_node->next;
			Subregion *q;

			ctk_text_buffer_delete_mark (region->buffer, sr->end);
			while (l != end_node) {
				q = l->data;
				ctk_text_buffer_delete_mark (region->buffer, q->start);
				ctk_text_buffer_delete_mark (region->buffer, q->end);
				g_free (q);
				l = g_list_delete_link (l, l);
			}
			q = l->data;
			ctk_text_buffer_delete_mark (region->buffer, q->start);
			sr->end = q->end;
			g_free (q);
			l = g_list_delete_link (l, l);
		}
		/* now move marks if that action expands the region */
		ctk_text_buffer_get_iter_at_mark (region->buffer, &iter, sr->start);
		if (ctk_text_iter_compare (&iter, &start) > 0)
			ctk_text_buffer_move_mark (region->buffer, sr->start, &start);
		ctk_text_buffer_get_iter_at_mark (region->buffer, &iter, sr->end);
		if (ctk_text_iter_compare (&iter, &end) < 0)
			ctk_text_buffer_move_mark (region->buffer, sr->end, &end);
	}

	++region->time_stamp;

	DEBUG (lapiz_text_region_debug_print (region));
}

void
lapiz_text_region_subtract (LapizTextRegion     *region,
			  const CtkTextIter *_start,
			  const CtkTextIter *_end)
{
	GList *start_node, *end_node, *node;
	CtkTextIter sr_start_iter, sr_end_iter;
	gboolean done;
	gboolean start_is_outside, end_is_outside;
	Subregion *sr;
	CtkTextIter start, end;

	g_return_if_fail (region != NULL && _start != NULL && _end != NULL);

	start = *_start;
	end = *_end;

	DEBUG (g_print ("---\n"));
	DEBUG (lapiz_text_region_debug_print (region));
	DEBUG (g_message ("region_substract (%d, %d)",
			  ctk_text_iter_get_offset (&start),
			  ctk_text_iter_get_offset (&end)));

	ctk_text_iter_order (&start, &end);

	/* find bounding subregions */
	start_node = find_nearest_subregion (region, &start, NULL, FALSE, FALSE);
	end_node = find_nearest_subregion (region, &end, start_node, TRUE, FALSE);

	/* easy case first */
	if (start_node == NULL || end_node == NULL || end_node == start_node->prev)
		return;

	/* deal with the start point */
	start_is_outside = end_is_outside = FALSE;

	sr = start_node->data;
	ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter, sr->start);
	ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);

	if (ctk_text_iter_in_range (&start, &sr_start_iter, &sr_end_iter) &&
	    !ctk_text_iter_equal (&start, &sr_start_iter)) {
		/* the starting point is inside the first subregion */
		if (ctk_text_iter_in_range (&end, &sr_start_iter, &sr_end_iter) &&
		    !ctk_text_iter_equal (&end, &sr_end_iter)) {
			/* the ending point is also inside the first
                           subregion: we need to split */
			Subregion *new_sr = g_new0 (Subregion, 1);
			new_sr->end = sr->end;
			new_sr->start = ctk_text_buffer_create_mark (region->buffer,
								     NULL, &end, TRUE);
			start_node = g_list_insert_before (start_node, start_node->next, new_sr);

			sr->end = ctk_text_buffer_create_mark (region->buffer,
							       NULL, &start, FALSE);

			/* no further processing needed */
			DEBUG (g_message ("subregion splitted"));

			return;
		} else {
			/* the ending point is outside, so just move
                           the end of the subregion to the starting point */
			ctk_text_buffer_move_mark (region->buffer, sr->end, &start);
		}
	} else {
		/* the starting point is outside (and so to the left)
                   of the first subregion */
		DEBUG (g_message ("start is outside"));

		start_is_outside = TRUE;
	}

	/* deal with the end point */
	if (start_node != end_node) {
		sr = end_node->data;
		ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter, sr->start);
		ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);
	}

	if (ctk_text_iter_in_range (&end, &sr_start_iter, &sr_end_iter) &&
	    !ctk_text_iter_equal (&end, &sr_end_iter)) {
		/* ending point is inside, move the start mark */
		ctk_text_buffer_move_mark (region->buffer, sr->start, &end);
	} else {
		end_is_outside = TRUE;
		DEBUG (g_message ("end is outside"));

	}

	/* finally remove any intermediate subregions */
	done = FALSE;
	node = start_node;

	while (!done) {
		if (node == end_node)
			/* we are done, exit in the next iteration */
			done = TRUE;

		if ((node == start_node && !start_is_outside) ||
		    (node == end_node && !end_is_outside)) {
			/* skip starting or ending node */
			node = node->next;
		} else {
			GList *l = node->next;
			sr = node->data;
			ctk_text_buffer_delete_mark (region->buffer, sr->start);
			ctk_text_buffer_delete_mark (region->buffer, sr->end);
			g_free (sr);
			region->subregions = g_list_delete_link (region->subregions,
								 node);
			node = l;
		}
	}

	++region->time_stamp;

	DEBUG (lapiz_text_region_debug_print (region));

	/* now get rid of empty subregions */
	lapiz_text_region_clear_zero_length_subregions (region);

	DEBUG (lapiz_text_region_debug_print (region));
}

gint
lapiz_text_region_subregions (LapizTextRegion *region)
{
	g_return_val_if_fail (region != NULL, 0);

	return g_list_length (region->subregions);
}

gboolean
lapiz_text_region_nth_subregion (LapizTextRegion *region,
			       guint          subregion,
			       CtkTextIter   *start,
			       CtkTextIter   *end)
{
	Subregion *sr;

	g_return_val_if_fail (region != NULL, FALSE);

	sr = g_list_nth_data (region->subregions, subregion);
	if (sr == NULL)
		return FALSE;

	if (start)
		ctk_text_buffer_get_iter_at_mark (region->buffer, start, sr->start);
	if (end)
		ctk_text_buffer_get_iter_at_mark (region->buffer, end, sr->end);

	return TRUE;
}

LapizTextRegion *
lapiz_text_region_intersect (LapizTextRegion     *region,
			   const CtkTextIter *_start,
			   const CtkTextIter *_end)
{
	GList *start_node, *end_node, *node;
	CtkTextIter sr_start_iter, sr_end_iter;
	Subregion *sr, *new_sr;
	gboolean done;
	LapizTextRegion *new_region;
	CtkTextIter start, end;

	g_return_val_if_fail (region != NULL && _start != NULL && _end != NULL, NULL);

	start = *_start;
	end = *_end;

	ctk_text_iter_order (&start, &end);

	/* find bounding subregions */
	start_node = find_nearest_subregion (region, &start, NULL, FALSE, FALSE);
	end_node = find_nearest_subregion (region, &end, start_node, TRUE, FALSE);

	/* easy case first */
	if (start_node == NULL || end_node == NULL || end_node == start_node->prev)
		return NULL;

	new_region = lapiz_text_region_new (region->buffer);
	done = FALSE;

	sr = start_node->data;
	ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter, sr->start);
	ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);

	/* starting node */
	if (ctk_text_iter_in_range (&start, &sr_start_iter, &sr_end_iter)) {
		new_sr = g_new0 (Subregion, 1);
		new_region->subregions = g_list_prepend (new_region->subregions, new_sr);

		new_sr->start = ctk_text_buffer_create_mark (new_region->buffer, NULL,
							     &start, TRUE);
		if (start_node == end_node) {
			/* things will finish shortly */
			done = TRUE;
			if (ctk_text_iter_in_range (&end, &sr_start_iter, &sr_end_iter))
				new_sr->end = ctk_text_buffer_create_mark (new_region->buffer,
									   NULL, &end, FALSE);
			else
				new_sr->end = ctk_text_buffer_create_mark (new_region->buffer,
									   NULL, &sr_end_iter,
									   FALSE);
		} else {
			new_sr->end = ctk_text_buffer_create_mark (new_region->buffer, NULL,
								   &sr_end_iter, FALSE);
		}
		node = start_node->next;
	} else {
		/* start should be the same as the subregion, so copy it in the loop */
		node = start_node;
	}

	if (!done) {
		while (node != end_node) {
			/* copy intermediate subregions verbatim */
			sr = node->data;
			ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter,
							  sr->start);
			ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);

			new_sr = g_new0 (Subregion, 1);
			new_region->subregions = g_list_prepend (new_region->subregions, new_sr);
			new_sr->start = ctk_text_buffer_create_mark (new_region->buffer, NULL,
								     &sr_start_iter, TRUE);
			new_sr->end = ctk_text_buffer_create_mark (new_region->buffer, NULL,
								   &sr_end_iter, FALSE);
			/* next node */
			node = node->next;
		}

		/* ending node */
		sr = node->data;
		ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter, sr->start);
		ctk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);

		new_sr = g_new0 (Subregion, 1);
		new_region->subregions = g_list_prepend (new_region->subregions, new_sr);

		new_sr->start = ctk_text_buffer_create_mark (new_region->buffer, NULL,
							     &sr_start_iter, TRUE);

		if (ctk_text_iter_in_range (&end, &sr_start_iter, &sr_end_iter))
			new_sr->end = ctk_text_buffer_create_mark (new_region->buffer, NULL,
								   &end, FALSE);
		else
			new_sr->end = ctk_text_buffer_create_mark (new_region->buffer, NULL,
								   &sr_end_iter, FALSE);
	}

	new_region->subregions = g_list_reverse (new_region->subregions);
	return new_region;
}

static gboolean
check_iterator (LapizTextRegionIteratorReal *real)
{
	if ((real->region == NULL) ||
	    (real->region_time_stamp != real->region->time_stamp))
	{
		g_warning("Invalid iterator: either the iterator "
                	  "is uninitialized, or the region "
                 	  "has been modified since the iterator "
                 	  "was created.");

                return FALSE;
	}

	return TRUE;
}

void
lapiz_text_region_get_iterator (LapizTextRegion         *region,
                              LapizTextRegionIterator *iter,
                              guint                  start)
{
	LapizTextRegionIteratorReal *real;

	g_return_if_fail (region != NULL);
	g_return_if_fail (iter != NULL);

	real = (LapizTextRegionIteratorReal *)iter;

	/* region->subregions may be NULL, -> end iter */

	real->region = region;
	real->subregions = g_list_nth (region->subregions, start);
	real->region_time_stamp = region->time_stamp;
}

gboolean
lapiz_text_region_iterator_is_end (LapizTextRegionIterator *iter)
{
	LapizTextRegionIteratorReal *real;

	g_return_val_if_fail (iter != NULL, FALSE);

	real = (LapizTextRegionIteratorReal *)iter;
	g_return_val_if_fail (check_iterator (real), FALSE);

	return (real->subregions == NULL);
}

gboolean
lapiz_text_region_iterator_next (LapizTextRegionIterator *iter)
{
	LapizTextRegionIteratorReal *real;

	g_return_val_if_fail (iter != NULL, FALSE);

	real = (LapizTextRegionIteratorReal *)iter;
	g_return_val_if_fail (check_iterator (real), FALSE);

	if (real->subregions != NULL) {
		real->subregions = g_list_next (real->subregions);
		return TRUE;
	}
	else
		return FALSE;
}

void
lapiz_text_region_iterator_get_subregion (LapizTextRegionIterator *iter,
					CtkTextIter           *start,
					CtkTextIter           *end)
{
	LapizTextRegionIteratorReal *real;
	Subregion *sr;

	g_return_if_fail (iter != NULL);

	real = (LapizTextRegionIteratorReal *)iter;
	g_return_if_fail (check_iterator (real));
	g_return_if_fail (real->subregions != NULL);

	sr = (Subregion*)real->subregions->data;
	g_return_if_fail (sr != NULL);

	if (start)
		ctk_text_buffer_get_iter_at_mark (real->region->buffer, start, sr->start);
	if (end)
		ctk_text_buffer_get_iter_at_mark (real->region->buffer, end, sr->end);
}

void
lapiz_text_region_debug_print (LapizTextRegion *region)
{
	GList *l;

	g_return_if_fail (region != NULL);

	g_print ("Subregions: ");
	l = region->subregions;
	while (l) {
		Subregion *sr = l->data;
		CtkTextIter iter1, iter2;
		ctk_text_buffer_get_iter_at_mark (region->buffer, &iter1, sr->start);
		ctk_text_buffer_get_iter_at_mark (region->buffer, &iter2, sr->end);
		g_print ("%d-%d ", ctk_text_iter_get_offset (&iter1),
			 ctk_text_iter_get_offset (&iter2));
		l = l->next;
	}
	g_print ("\n");
}
