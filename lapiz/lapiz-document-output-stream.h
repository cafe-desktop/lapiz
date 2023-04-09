/*
 * lapiz-document-output-stream.h
 * This file is part of lapiz
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
 *
 * lapiz is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * lapiz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lapiz; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */


#ifndef __LAPIZ_DOCUMENT_OUTPUT_STREAM_H__
#define __LAPIZ_DOCUMENT_OUTPUT_STREAM_H__

#include <gio/gio.h>
#include "lapiz-document.h"

G_BEGIN_DECLS

#define LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM		(lapiz_document_output_stream_get_type ())
#define LAPIZ_DOCUMENT_OUTPUT_STREAM(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM, PlumaDocumentOutputStream))
#define LAPIZ_DOCUMENT_OUTPUT_STREAM_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM, PlumaDocumentOutputStream const))
#define LAPIZ_DOCUMENT_OUTPUT_STREAM_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM, PlumaDocumentOutputStreamClass))
#define LAPIZ_IS_DOCUMENT_OUTPUT_STREAM(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM))
#define LAPIZ_IS_DOCUMENT_OUTPUT_STREAM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM))
#define LAPIZ_DOCUMENT_OUTPUT_STREAM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM, PlumaDocumentOutputStreamClass))

typedef struct _PlumaDocumentOutputStream		PlumaDocumentOutputStream;
typedef struct _PlumaDocumentOutputStreamClass		PlumaDocumentOutputStreamClass;
typedef struct _PlumaDocumentOutputStreamPrivate	PlumaDocumentOutputStreamPrivate;

struct _PlumaDocumentOutputStream
{
	GOutputStream parent;

	PlumaDocumentOutputStreamPrivate *priv;
};

struct _PlumaDocumentOutputStreamClass
{
	GOutputStreamClass parent_class;
};

GType			 lapiz_document_output_stream_get_type		(void) G_GNUC_CONST;

GOutputStream		*lapiz_document_output_stream_new		(PlumaDocument *doc);

PlumaDocumentNewlineType lapiz_document_output_stream_detect_newline_type (PlumaDocumentOutputStream *stream);

G_END_DECLS

#endif /* __LAPIZ_DOCUMENT_OUTPUT_STREAM_H__ */
