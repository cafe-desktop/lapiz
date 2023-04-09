/*
 * lapiz-print-preview.h
 *
 * Copyright (C) 2008 Paolo Borelli
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
 * Modified by the lapiz Team, 1998-2006. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id: lapiz-commands-search.c 5931 2007-09-25 20:05:40Z pborelli $
 */


#ifndef __LAPIZ_PRINT_PREVIEW_H__
#define __LAPIZ_PRINT_PREVIEW_H__

#include <ctk/ctk.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_PRINT_PREVIEW            (lapiz_print_preview_get_type ())
#define LAPIZ_PRINT_PREVIEW(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), LAPIZ_TYPE_PRINT_PREVIEW, LapizPrintPreview))
#define LAPIZ_PRINT_PREVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_PRINT_PREVIEW, LapizPrintPreviewClass))
#define LAPIZ_IS_PRINT_PREVIEW(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), LAPIZ_TYPE_PRINT_PREVIEW))
#define LAPIZ_IS_PRINT_PREVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_PRINT_PREVIEW))
#define LAPIZ_PRINT_PREVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_PRINT_PREVIEW, LapizPrintPreviewClass))

typedef struct _LapizPrintPreview        LapizPrintPreview;
typedef struct _LapizPrintPreviewPrivate LapizPrintPreviewPrivate;
typedef struct _LapizPrintPreviewClass   LapizPrintPreviewClass;

struct _LapizPrintPreview
{
	CtkBox parent;

	LapizPrintPreviewPrivate *priv;
};

struct _LapizPrintPreviewClass
{
	CtkBoxClass parent_class;

	void (* close)		(LapizPrintPreview          *preview);
};


GType		 lapiz_print_preview_get_type	(void) G_GNUC_CONST;

CtkWidget	*lapiz_print_preview_new	(CtkPrintOperation		*op,
						 CtkPrintOperationPreview	*ctk_preview,
						 CtkPrintContext		*context);

G_END_DECLS

#endif /* __LAPIZ_PRINT_PREVIEW_H__ */
