/*
 * lapiz-smart-charset-converter.c
 * This file is part of lapiz
 *
 * Copyright (C) 2009 - Ignacio Casal Quinteiro
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

#include "lapiz-smart-charset-converter.h"
#include "lapiz-debug.h"
#include "lapiz-document.h"

#include <gio/gio.h>
#include <glib/gi18n.h>

struct _LapizSmartCharsetConverterPrivate
{
	GCharsetConverter *charset_conv;

	GSList *encodings;
	GSList *current_encoding;

	guint is_utf8 : 1;
	guint use_first : 1;
};

static void lapiz_smart_charset_converter_iface_init    (GConverterIface *iface);

G_DEFINE_TYPE_WITH_CODE (LapizSmartCharsetConverter, lapiz_smart_charset_converter,
			 G_TYPE_OBJECT,
			 G_ADD_PRIVATE (LapizSmartCharsetConverter)
			 G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
						lapiz_smart_charset_converter_iface_init))

static void
lapiz_smart_charset_converter_finalize (GObject *object)
{
	LapizSmartCharsetConverter *smart = LAPIZ_SMART_CHARSET_CONVERTER (object);

	g_slist_free (smart->priv->encodings);

	lapiz_debug_message (DEBUG_UTILS, "finalizing smart charset converter");

	G_OBJECT_CLASS (lapiz_smart_charset_converter_parent_class)->finalize (object);
}

static void
lapiz_smart_charset_converter_dispose (GObject *object)
{
	LapizSmartCharsetConverter *smart = LAPIZ_SMART_CHARSET_CONVERTER (object);

	if (smart->priv->charset_conv != NULL)
	{
		g_object_unref (smart->priv->charset_conv);
		smart->priv->charset_conv = NULL;
	}

	lapiz_debug_message (DEBUG_UTILS, "disposing smart charset converter");

	G_OBJECT_CLASS (lapiz_smart_charset_converter_parent_class)->dispose (object);
}

static void
lapiz_smart_charset_converter_class_init (LapizSmartCharsetConverterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_smart_charset_converter_finalize;
	object_class->dispose = lapiz_smart_charset_converter_dispose;
}

static void
lapiz_smart_charset_converter_init (LapizSmartCharsetConverter *smart)
{
	smart->priv = lapiz_smart_charset_converter_get_instance_private (smart);

	smart->priv->charset_conv = NULL;
	smart->priv->encodings = NULL;
	smart->priv->current_encoding = NULL;
	smart->priv->is_utf8 = FALSE;
	smart->priv->use_first = FALSE;

	lapiz_debug_message (DEBUG_UTILS, "initializing smart charset converter");
}

static const LapizEncoding *
get_encoding (LapizSmartCharsetConverter *smart)
{
	if (smart->priv->current_encoding == NULL)
	{
		smart->priv->current_encoding = smart->priv->encodings;
	}
	else
	{
		smart->priv->current_encoding = g_slist_next (smart->priv->current_encoding);
	}

	if (smart->priv->current_encoding != NULL)
		return (const LapizEncoding *)smart->priv->current_encoding->data;

#if 0
	FIXME: uncomment this when using fallback
	/* If we tried all encodings, we return the first encoding */
	smart->priv->use_first = TRUE;
	smart->priv->current_encoding = smart->priv->encodings;

	return (const LapizEncoding *)smart->priv->current_encoding->data;
#endif
	return NULL;
}

static gboolean
try_convert (GCharsetConverter *converter,
             const void        *inbuf,
             gsize              inbuf_size)
{
	GError *err;
	gsize bytes_read, nread;
	gsize bytes_written, nwritten;
	GConverterResult res;
	gchar *out;
	gboolean ret;
	gsize out_size;

	if (inbuf == NULL || inbuf_size == 0)
	{
		return FALSE;
	}

	err = NULL;
	nread = 0;
	nwritten = 0;
	out_size = inbuf_size * 4;
	out = g_malloc (out_size);

	do
	{
		res = g_converter_convert (G_CONVERTER (converter),
		                           (void *) ((gsize) inbuf + nread),
		                           inbuf_size - nread,
		                           out + nwritten,
		                           out_size - nwritten,
		                           G_CONVERTER_INPUT_AT_END,
		                           &bytes_read,
		                           &bytes_written,
		                           &err);

		nread += bytes_read;
		nwritten += bytes_written;
	} while (res != G_CONVERTER_FINISHED && res != G_CONVERTER_ERROR && err == NULL);

	if (err != NULL)
	{
		if (err->code == G_CONVERT_ERROR_PARTIAL_INPUT)
		{
			/* FIXME We can get partial input while guessing the
			   encoding because we just take some amount of text
			   to guess from. */
			ret = TRUE;
		}
		else
		{
			ret = FALSE;
		}

		g_error_free (err);
	}
	else
	{
		ret = TRUE;
	}

	/* FIXME: Check the remainder? */
	if (ret == TRUE && !g_utf8_validate (out, nwritten, NULL))
	{
		ret = FALSE;
	}

	g_free (out);

	return ret;
}

static GCharsetConverter *
guess_encoding (LapizSmartCharsetConverter *smart,
		const void                 *inbuf,
		gsize                       inbuf_size)
{
	GCharsetConverter *conv = NULL;

	if (inbuf == NULL || inbuf_size == 0)
	{
		smart->priv->is_utf8 = TRUE;
		return NULL;
	}

	if (smart->priv->encodings != NULL &&
	    smart->priv->encodings->next == NULL)
		smart->priv->use_first = TRUE;

	/* We just check the first block */
	while (TRUE)
	{
		const LapizEncoding *enc;

		if (conv != NULL)
		{
			g_object_unref (conv);
			conv = NULL;
		}

		/* We get an encoding from the list */
		enc = get_encoding (smart);

		/* if it is NULL we didn't guess anything */
		if (enc == NULL)
		{
			break;
		}

		lapiz_debug_message (DEBUG_UTILS, "trying charset: %s",
				     lapiz_encoding_get_charset (smart->priv->current_encoding->data));

		if (enc == lapiz_encoding_get_utf8 ())
		{
			gsize remainder;
			const gchar *end;

			if (g_utf8_validate (inbuf, inbuf_size, &end) ||
			    smart->priv->use_first)
			{
				smart->priv->is_utf8 = TRUE;
				break;
			}

			/* Check if the end is less than one char */
			remainder = inbuf_size - (end - (gchar *)inbuf);
			if (remainder < 6)
			{
				smart->priv->is_utf8 = TRUE;
				break;
			}

			continue;
		}

		conv = g_charset_converter_new ("UTF-8",
						lapiz_encoding_get_charset (enc),
						NULL);

		/* If we tried all encodings we use the first one */
		if (smart->priv->use_first)
		{
			break;
		}

		/* Try to convert */
		if (try_convert (conv, inbuf, inbuf_size))
		{
			break;
		}
	}

	if (conv != NULL)
	{
		g_converter_reset (G_CONVERTER (conv));

		/* FIXME: uncomment this when we want to use the fallback
		g_charset_converter_set_use_fallback (conv, TRUE);*/
	}

	return conv;
}

static GConverterResult
lapiz_smart_charset_converter_convert (GConverter *converter,
				       const void *inbuf,
				       gsize       inbuf_size,
				       void       *outbuf,
				       gsize       outbuf_size,
				       GConverterFlags flags,
				       gsize      *bytes_read,
				       gsize      *bytes_written,
				       GError    **error)
{
	LapizSmartCharsetConverter *smart = LAPIZ_SMART_CHARSET_CONVERTER (converter);

	/* Guess the encoding if we didn't make it yet */
	if (smart->priv->charset_conv == NULL &&
	    !smart->priv->is_utf8)
	{
		smart->priv->charset_conv = guess_encoding (smart, inbuf, inbuf_size);

		/* If we still have the previous case is that we didn't guess
		   anything */
		if (smart->priv->charset_conv == NULL &&
		    !smart->priv->is_utf8)
		{
			/* FIXME: Add a different domain when we kill lapiz_convert */
			g_set_error_literal (error, LAPIZ_DOCUMENT_ERROR,
					     LAPIZ_DOCUMENT_ERROR_ENCODING_AUTO_DETECTION_FAILED,
					     _("It is not possible to detect the encoding automatically"));
			return G_CONVERTER_ERROR;
		}
	}

	/* Now if the encoding is utf8 just redirect the input to the output */
	if (smart->priv->is_utf8)
	{
		gsize size;
		GConverterResult ret;

		size = MIN (inbuf_size, outbuf_size);

		memcpy (outbuf, inbuf, size);
		*bytes_read = size;
		*bytes_written = size;

		ret = G_CONVERTER_CONVERTED;

		if (flags & G_CONVERTER_INPUT_AT_END)
			ret = G_CONVERTER_FINISHED;
		else if (flags & G_CONVERTER_FLUSH)
			ret = G_CONVERTER_FLUSHED;

		return ret;
	}

	/* If we reached here is because we need to convert the text so, we
	   convert it with the charset converter */
	return g_converter_convert (G_CONVERTER (smart->priv->charset_conv),
				    inbuf,
				    inbuf_size,
				    outbuf,
				    outbuf_size,
				    flags,
				    bytes_read,
				    bytes_written,
				    error);
}

static void
lapiz_smart_charset_converter_reset (GConverter *converter)
{
	LapizSmartCharsetConverter *smart = LAPIZ_SMART_CHARSET_CONVERTER (converter);

	smart->priv->current_encoding = NULL;
	smart->priv->is_utf8 = FALSE;

	if (smart->priv->charset_conv != NULL)
	{
		g_object_unref (smart->priv->charset_conv);
		smart->priv->charset_conv = NULL;
	}
}

static void
lapiz_smart_charset_converter_iface_init (GConverterIface *iface)
{
	iface->convert = lapiz_smart_charset_converter_convert;
	iface->reset = lapiz_smart_charset_converter_reset;
}

LapizSmartCharsetConverter *
lapiz_smart_charset_converter_new (GSList *candidate_encodings)
{
	LapizSmartCharsetConverter *smart;

	g_return_val_if_fail (candidate_encodings != NULL, NULL);

	smart = g_object_new (LAPIZ_TYPE_SMART_CHARSET_CONVERTER, NULL);

	smart->priv->encodings = g_slist_copy (candidate_encodings);

	return smart;
}

const LapizEncoding *
lapiz_smart_charset_converter_get_guessed (LapizSmartCharsetConverter *smart)
{
	g_return_val_if_fail (LAPIZ_IS_SMART_CHARSET_CONVERTER (smart), NULL);

	if (smart->priv->current_encoding != NULL)
	{
		return (const LapizEncoding *)smart->priv->current_encoding->data;
	}
	else if (smart->priv->is_utf8)
	{
		return lapiz_encoding_get_utf8 ();
	}

	return NULL;
}

guint
lapiz_smart_charset_converter_get_num_fallbacks (LapizSmartCharsetConverter *smart)
{
	g_return_val_if_fail (LAPIZ_IS_SMART_CHARSET_CONVERTER (smart), FALSE);

	if (smart->priv->charset_conv == NULL)
		return FALSE;

	return g_charset_converter_get_num_fallbacks (smart->priv->charset_conv) != 0;
}

