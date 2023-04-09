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
#define LAPIZ_DOCUMENT_OUTPUT_STREAM(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM, LapizDocumentOutputStream))
#define LAPIZ_DOCUMENT_OUTPUT_STREAM_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM, LapizDocumentOutputStream const))
#define LAPIZ_DOCUMENT_OUTPUT_STREAM_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM, LapizDocumentOutputStreamClass))
#define LAPIZ_IS_DOCUMENT_OUTPUT_STREAM(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM))
#define LAPIZ_IS_DOCUMENT_OUTPUT_STREAM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM))
#define LAPIZ_DOCUMENT_OUTPUT_STREAM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM, LapizDocumentOutputStreamClass))

typedef struct _LapizDocumentOutputStream		LapizDocumentOutputStream;
typedef struct _LapizDocumentOutputStreamClass		LapizDocumentOutputStreamClass;
typedef struct _LapizDocumentOutputStreamPrivate	LapizDocumentOutputStreamPrivate;

struct _LapizDocumentOutputStream
{
	GOutputStream parent;

	LapizDocumentOutputStreamPrivate *priv;
};

struct _LapizDocumentOutputStreamClass
{
	GOutputStreamClass parent_class;
};

GType			 lapiz_document_output_stream_get_type		(void) G_GNUC_CONST;

GOutputStream		*lapiz_document_output_stream_new		(LapizDocument *doc);

LapizDocumentNewlineType lapiz_document_output_stream_detect_newline_type (LapizDocumentOutputStream *stream);

G_END_DECLS

#endif /* __LAPIZ_DOCUMENT_OUTPUT_STREAM_H__ */
