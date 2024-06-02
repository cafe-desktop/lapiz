/*
 * lapiz-document-output-stream.c
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

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include "lapiz-document-output-stream.h"

/* NOTE: never use async methods on this stream, the stream is just
 * a wrapper around CtkTextBuffer api so that we can use GIO Stream
 * methods, but the undelying code operates on a CtkTextBuffer, so
 * there is no I/O involved and should be accessed only by the main
 * thread */

#define MAX_UNICHAR_LEN 6

struct _LapizDocumentOutputStreamPrivate
{
	LapizDocument *doc;
	CtkTextIter    pos;

	gchar *buffer;
	gsize buflen;

	guint is_initialized : 1;
	guint is_closed : 1;
};

enum
{
	PROP_0,
	PROP_DOCUMENT
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizDocumentOutputStream, lapiz_document_output_stream, G_TYPE_OUTPUT_STREAM)

static gssize	lapiz_document_output_stream_write (GOutputStream            *stream,
						    const void               *buffer,
						    gsize                     count,
						    GCancellable             *cancellable,
						    GError                  **error);

static gboolean lapiz_document_output_stream_flush (GOutputStream *stream,
                                                    GCancellable  *cancellable,
                                                    GError        **error);

static gboolean	lapiz_document_output_stream_close (GOutputStream     *stream,
						    GCancellable      *cancellable,
						    GError           **error);

static void
lapiz_document_output_stream_set_property (GObject      *object,
					   guint         prop_id,
					   const GValue *value,
					   GParamSpec   *pspec)
{
	LapizDocumentOutputStream *stream = LAPIZ_DOCUMENT_OUTPUT_STREAM (object);

	switch (prop_id)
	{
		case PROP_DOCUMENT:
			stream->priv->doc = LAPIZ_DOCUMENT (g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_document_output_stream_get_property (GObject    *object,
					   guint       prop_id,
					   GValue     *value,
					   GParamSpec *pspec)
{
	LapizDocumentOutputStream *stream = LAPIZ_DOCUMENT_OUTPUT_STREAM (object);

	switch (prop_id)
	{
		case PROP_DOCUMENT:
			g_value_set_object (value, stream->priv->doc);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_document_output_stream_finalize (GObject *object)
{
	LapizDocumentOutputStream *stream = LAPIZ_DOCUMENT_OUTPUT_STREAM (object);

	g_free (stream->priv->buffer);

	G_OBJECT_CLASS (lapiz_document_output_stream_parent_class)->finalize (object);
}

static void
lapiz_document_output_stream_constructed (GObject *object)
{
	LapizDocumentOutputStream *stream = LAPIZ_DOCUMENT_OUTPUT_STREAM (object);

	if (!stream->priv->doc)
	{
		g_critical ("This should never happen, a problem happened constructing the Document Output Stream!");
		return;
	}

	/* Init the undoable action */
	ctk_source_buffer_begin_not_undoable_action (CTK_SOURCE_BUFFER (stream->priv->doc));
	/* clear the buffer */
	ctk_text_buffer_set_text (CTK_TEXT_BUFFER (stream->priv->doc),
				  "", 0);
	ctk_text_buffer_set_modified (CTK_TEXT_BUFFER (stream->priv->doc),
				      FALSE);

	ctk_source_buffer_end_not_undoable_action (CTK_SOURCE_BUFFER (stream->priv->doc));
}

static void
lapiz_document_output_stream_class_init (LapizDocumentOutputStreamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GOutputStreamClass *stream_class = G_OUTPUT_STREAM_CLASS (klass);

	object_class->get_property = lapiz_document_output_stream_get_property;
	object_class->set_property = lapiz_document_output_stream_set_property;
	object_class->finalize = lapiz_document_output_stream_finalize;
	object_class->constructed = lapiz_document_output_stream_constructed;

	stream_class->write_fn = lapiz_document_output_stream_write;
	stream_class->flush = lapiz_document_output_stream_flush;
	stream_class->close_fn = lapiz_document_output_stream_close;

	g_object_class_install_property (object_class,
					 PROP_DOCUMENT,
					 g_param_spec_object ("document",
							      "Document",
							      "The document which is written",
							      LAPIZ_TYPE_DOCUMENT,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
}

static void
lapiz_document_output_stream_init (LapizDocumentOutputStream *stream)
{
	stream->priv = lapiz_document_output_stream_get_instance_private (stream);

	stream->priv->buffer = NULL;
	stream->priv->buflen = 0;

	stream->priv->is_initialized = FALSE;
	stream->priv->is_closed = FALSE;
}

static LapizDocumentNewlineType
get_newline_type (CtkTextIter *end)
{
	LapizDocumentNewlineType res;
	CtkTextIter copy;
	gunichar c;

	copy = *end;
	c = ctk_text_iter_get_char (&copy);

	if (g_unichar_break_type (c) == G_UNICODE_BREAK_CARRIAGE_RETURN)
	{
		if (ctk_text_iter_forward_char (&copy) &&
		    g_unichar_break_type (ctk_text_iter_get_char (&copy)) == G_UNICODE_BREAK_LINE_FEED)
		{
			res = LAPIZ_DOCUMENT_NEWLINE_TYPE_CR_LF;
		}
		else
		{
			res = LAPIZ_DOCUMENT_NEWLINE_TYPE_CR;
		}
	}
	else
	{
		res = LAPIZ_DOCUMENT_NEWLINE_TYPE_LF;
	}

	return res;
}

GOutputStream *
lapiz_document_output_stream_new (LapizDocument *doc)
{
	return G_OUTPUT_STREAM (g_object_new (LAPIZ_TYPE_DOCUMENT_OUTPUT_STREAM,
					      "document", doc, NULL));
}

LapizDocumentNewlineType
lapiz_document_output_stream_detect_newline_type (LapizDocumentOutputStream *stream)
{
	LapizDocumentNewlineType type;
	CtkTextIter iter;

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT_OUTPUT_STREAM (stream),
			      LAPIZ_DOCUMENT_NEWLINE_TYPE_DEFAULT);

	type = LAPIZ_DOCUMENT_NEWLINE_TYPE_DEFAULT;

	ctk_text_buffer_get_start_iter (CTK_TEXT_BUFFER (stream->priv->doc),
					&iter);

	if (ctk_text_iter_ends_line (&iter) || ctk_text_iter_forward_to_line_end (&iter))
	{
		type = get_newline_type (&iter);
	}

	return type;
}

/* If the last char is a newline, remove it from the buffer (otherwise
   CtkTextView shows it as an empty line). See bug #324942. */
static void
remove_ending_newline (LapizDocumentOutputStream *stream)
{
	CtkTextIter end;
	CtkTextIter start;

	ctk_text_buffer_get_end_iter (CTK_TEXT_BUFFER (stream->priv->doc), &end);
	start = end;

	ctk_text_iter_set_line_offset (&start, 0);

	if (ctk_text_iter_ends_line (&start) &&
	    ctk_text_iter_backward_line (&start))
	{
		if (!ctk_text_iter_ends_line (&start))
		{
			ctk_text_iter_forward_to_line_end (&start);
		}

		/* Delete the empty line which is from 'start' to 'end' */
		ctk_text_buffer_delete (CTK_TEXT_BUFFER (stream->priv->doc),
		                        &start,
		                        &end);
	}
}

static void
end_append_text_to_document (LapizDocumentOutputStream *stream)
{
	remove_ending_newline (stream);

	ctk_text_buffer_set_modified (CTK_TEXT_BUFFER (stream->priv->doc),
				      FALSE);

	ctk_source_buffer_end_not_undoable_action (CTK_SOURCE_BUFFER (stream->priv->doc));
}

static gssize
lapiz_document_output_stream_write (GOutputStream            *stream,
				    const void               *buffer,
				    gsize                     count,
				    GCancellable             *cancellable,
				    GError                  **error)
{
	LapizDocumentOutputStream *ostream;
	gchar *text;
	gsize len;
	gboolean freetext = FALSE;
	const gchar *end;
	gboolean valid;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return -1;

	ostream = LAPIZ_DOCUMENT_OUTPUT_STREAM (stream);

	if (!ostream->priv->is_initialized)
	{
		/* Init the undoable action */
		ctk_source_buffer_begin_not_undoable_action (CTK_SOURCE_BUFFER (ostream->priv->doc));

		ctk_text_buffer_get_start_iter (CTK_TEXT_BUFFER (ostream->priv->doc),
						&ostream->priv->pos);
		ostream->priv->is_initialized = TRUE;
	}

	if (ostream->priv->buflen > 0)
	{
		len = ostream->priv->buflen + count;
		text = g_new (gchar , len + 1);
		memcpy (text, ostream->priv->buffer, ostream->priv->buflen);
		memcpy (text + ostream->priv->buflen, buffer, count);
		text[len] = '\0';
		g_free (ostream->priv->buffer);
		ostream->priv->buffer = NULL;
		ostream->priv->buflen = 0;
		freetext = TRUE;
	}
	else
	{
		text = (gchar *) buffer;
		len = count;
	}

	/* validate */
	valid = g_utf8_validate (text, len, &end);

	/* Avoid keeping a CRLF across two buffers. */
	if (valid && len > 1 && end[-1] == '\r')
	{
		valid = FALSE;
		end--;
	}

	if (!valid)
	{
		gsize nvalid = end - text;
		gsize remainder = len - nvalid;
		gunichar ch;

		if ((remainder < MAX_UNICHAR_LEN) &&
		    ((ch = g_utf8_get_char_validated (text + nvalid, remainder)) == (gunichar)-2 ||
		     ch == (gunichar)'\r'))
		{
			ostream->priv->buffer = g_strndup (end, remainder);
			ostream->priv->buflen = remainder;
			len -= remainder;
		}
		else
		{
			/* TODO: we could escape invalid text and tag it in red
			 * and make the doc readonly.
			 */
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
				     _("Invalid UTF-8 sequence in input"));

			if (freetext)
				g_free (text);

			return -1;
		}
	}

	ctk_text_buffer_insert (CTK_TEXT_BUFFER (ostream->priv->doc),
				&ostream->priv->pos, text, len);

	if (freetext)
		g_free (text);

	return count;
}

static gboolean
lapiz_document_output_stream_flush (GOutputStream *stream,
                                    GCancellable  *cancellable,
                                    GError        **error)
{
	LapizDocumentOutputStream *ostream = LAPIZ_DOCUMENT_OUTPUT_STREAM (stream);

	/* Flush deferred data if some. */
	if (!ostream->priv->is_closed && ostream->priv->is_initialized &&
	    ostream->priv->buflen > 0 &&
	    lapiz_document_output_stream_write (stream, "", 0, cancellable,
						error) == -1)
		return FALSE;

	return TRUE;
}

static gboolean
lapiz_document_output_stream_close (GOutputStream     *stream,
				    GCancellable      *cancellable G_GNUC_UNUSED,
				    GError           **error)
{
	LapizDocumentOutputStream *ostream = LAPIZ_DOCUMENT_OUTPUT_STREAM (stream);

	if (!ostream->priv->is_closed && ostream->priv->is_initialized)
	{
		end_append_text_to_document (ostream);
		ostream->priv->is_closed = TRUE;
	}

	if (ostream->priv->buflen > 0)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			     _("Incomplete UTF-8 sequence in input"));
		return FALSE;
	}

	return TRUE;
}
