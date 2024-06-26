/*
 * lapiz-gio-document-loader.c
 * This file is part of lapiz
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2007 - Paolo Maggi, Steve Frécinaux
 * Copyright (C) 2008 - Jesse van den Kieboom
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
 * Modified by the lapiz Team, 2005-2008. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "lapiz-gio-document-loader.h"
#include "lapiz-document-output-stream.h"
#include "lapiz-smart-charset-converter.h"
#include "lapiz-prefs-manager.h"
#include "lapiz-debug.h"
#include "lapiz-utils.h"

#ifndef ENABLE_GVFS_METADATA
#include "lapiz-metadata-manager.h"
#endif

typedef struct
{
	LapizGioDocumentLoader *loader;
	GCancellable 	       *cancellable;

	gssize			read;
	gboolean		tried_mount;
} AsyncData;

#define READ_CHUNK_SIZE 8192
#define REMOTE_QUERY_ATTRIBUTES G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
				G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
				G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
				G_FILE_ATTRIBUTE_STANDARD_SIZE "," \
				G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE "," \
				LAPIZ_METADATA_ATTRIBUTE_ENCODING

static void	    lapiz_gio_document_loader_load		(LapizDocumentLoader *loader);
static gboolean     lapiz_gio_document_loader_cancel		(LapizDocumentLoader *loader);
static goffset      lapiz_gio_document_loader_get_bytes_read	(LapizDocumentLoader *loader);

static void open_async_read (AsyncData *async);

struct _LapizGioDocumentLoaderPrivate
{
	/* Info on the current file */
	GFile            *gfile;

	goffset           bytes_read;

	/* Handle for remote files */
	GCancellable 	 *cancellable;
	GInputStream	 *stream;
	GOutputStream    *output;
	LapizSmartCharsetConverter *converter;

	gchar             buffer[READ_CHUNK_SIZE];

	GError           *error;
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizGioDocumentLoader, lapiz_gio_document_loader, LAPIZ_TYPE_DOCUMENT_LOADER)

static void
lapiz_gio_document_loader_dispose (GObject *object)
{
	LapizGioDocumentLoaderPrivate *priv;

	priv = LAPIZ_GIO_DOCUMENT_LOADER (object)->priv;

	if (priv->cancellable != NULL)
	{
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	if (priv->stream != NULL)
	{
		g_object_unref (priv->stream);
		priv->stream = NULL;
	}

	if (priv->output != NULL)
	{
		g_object_unref (priv->output);
		priv->output = NULL;
	}

	if (priv->converter != NULL)
	{
		g_object_unref (priv->converter);
		priv->converter = NULL;
	}

	if (priv->gfile != NULL)
	{
		g_object_unref (priv->gfile);
		priv->gfile = NULL;
	}

	if (priv->error != NULL)
	{
		g_error_free (priv->error);
		priv->error = NULL;
	}

	G_OBJECT_CLASS (lapiz_gio_document_loader_parent_class)->dispose (object);
}

static void
lapiz_gio_document_loader_class_init (LapizGioDocumentLoaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	LapizDocumentLoaderClass *loader_class = LAPIZ_DOCUMENT_LOADER_CLASS (klass);

	object_class->dispose = lapiz_gio_document_loader_dispose;

	loader_class->load = lapiz_gio_document_loader_load;
	loader_class->cancel = lapiz_gio_document_loader_cancel;
	loader_class->get_bytes_read = lapiz_gio_document_loader_get_bytes_read;
}

static void
lapiz_gio_document_loader_init (LapizGioDocumentLoader *gvloader)
{
	gvloader->priv = lapiz_gio_document_loader_get_instance_private (gvloader);

	gvloader->priv->converter = NULL;
	gvloader->priv->error = NULL;
}

static AsyncData *
async_data_new (LapizGioDocumentLoader *gvloader)
{
	AsyncData *async;

	async = g_slice_new (AsyncData);
	async->loader = gvloader;
	async->cancellable = g_object_ref (gvloader->priv->cancellable);
	async->tried_mount = FALSE;

	return async;
}

static void
async_data_free (AsyncData *async)
{
	g_object_unref (async->cancellable);
	g_slice_free (AsyncData, async);
}

static const LapizEncoding *
get_metadata_encoding (LapizDocumentLoader *loader)
{
	const LapizEncoding *enc = NULL;

#ifndef ENABLE_GVFS_METADATA
	gchar *charset;
	const gchar *uri;

	uri = lapiz_document_loader_get_uri (loader);

	charset = lapiz_metadata_manager_get (uri, "encoding");

	if (charset == NULL)
		return NULL;

	enc = lapiz_encoding_get_from_charset (charset);

	g_free (charset);
#else
	GFileInfo *info;

	info = lapiz_document_loader_get_info (loader);

	/* check if the encoding was set in the metadata */
	if (g_file_info_has_attribute (info, LAPIZ_METADATA_ATTRIBUTE_ENCODING))
	{
		const gchar *charset;

		charset = g_file_info_get_attribute_string (info,
							    LAPIZ_METADATA_ATTRIBUTE_ENCODING);

		if (charset == NULL)
			return NULL;

		enc = lapiz_encoding_get_from_charset (charset);
	}
#endif

	return enc;
}

static void
remote_load_completed_or_failed (LapizGioDocumentLoader *loader, AsyncData *async)
{
	lapiz_document_loader_loading (LAPIZ_DOCUMENT_LOADER (loader),
				       TRUE,
				       loader->priv->error);

	if (async)
		async_data_free (async);
}

static void
async_failed (AsyncData *async, GError *error)
{
	g_propagate_error (&async->loader->priv->error, error);
	remote_load_completed_or_failed (async->loader, async);
}

static void
close_input_stream_ready_cb (GInputStream *stream,
			     GAsyncResult  *res,
			     AsyncData     *async)
{
	GError *error = NULL;

	lapiz_debug (DEBUG_LOADER);

	/* check cancelled state manually */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	lapiz_debug_message (DEBUG_SAVER, "Finished closing input stream");

	if (!g_input_stream_close_finish (stream, res, &error))
	{
		lapiz_debug_message (DEBUG_SAVER, "Closing input stream error: %s", error->message);

		async_failed (async, error);
		return;
	}

	lapiz_debug_message (DEBUG_SAVER, "Close output stream");
	if (!g_output_stream_close (async->loader->priv->output,
				    async->cancellable, &error))
	{
		async_failed (async, error);
		return;
	}

	remote_load_completed_or_failed (async->loader, async);
}

static void
write_complete (AsyncData *async)
{
	if (async->loader->priv->stream)
		g_input_stream_close_async (G_INPUT_STREAM (async->loader->priv->stream),
					    G_PRIORITY_HIGH,
					    async->cancellable,
					    (GAsyncReadyCallback)close_input_stream_ready_cb,
					    async);
}

/* prototype, because they call each other... isn't C lovely */
static void	read_file_chunk		(AsyncData *async);

static void
write_file_chunk (AsyncData *async)
{
	LapizGioDocumentLoader *gvloader;
	gssize bytes_written;
	GError *error = NULL;

	gvloader = async->loader;

	/* we use sync methods on doc stream since it is in memory. Using async
	   would be racy and we can endup with invalidated iters */
	bytes_written = g_output_stream_write (G_OUTPUT_STREAM (gvloader->priv->output),
					       gvloader->priv->buffer,
					       async->read,
					       async->cancellable,
					       &error);

	lapiz_debug_message (DEBUG_SAVER, "Written: %" G_GSSIZE_FORMAT, bytes_written);
	if (bytes_written == -1)
	{
		lapiz_debug_message (DEBUG_SAVER, "Write error: %s", error->message);
		async_failed (async, error);
		return;
	}

	/* note that this signal blocks the read... check if it isn't
	 * a performance problem
	 */
	lapiz_document_loader_loading (LAPIZ_DOCUMENT_LOADER (gvloader),
				       FALSE,
				       NULL);

	read_file_chunk (async);
}

static void
async_read_cb (GInputStream *stream,
	       GAsyncResult *res,
	       AsyncData    *async)
{
	lapiz_debug (DEBUG_LOADER);
	LapizGioDocumentLoader *gvloader;
	GError *error = NULL;

	lapiz_debug (DEBUG_LOADER);

	/* manually check cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	gvloader = async->loader;

	async->read = g_input_stream_read_finish (stream, res, &error);

	/* error occurred */
	if (async->read == -1)
	{
		async_failed (async, error);
		return;
	}

	/* Check for the extremely unlikely case where the file size overflows. */
	if (gvloader->priv->bytes_read + async->read < gvloader->priv->bytes_read)
	{
		g_set_error (&gvloader->priv->error,
			     LAPIZ_DOCUMENT_ERROR,
			     LAPIZ_DOCUMENT_ERROR_TOO_BIG,
			     "File too big");

		async_failed (async, gvloader->priv->error);
		return;
	}

	/* Bump the size. */
	gvloader->priv->bytes_read += async->read;

	/* end of the file, we are done! */
	if (async->read == 0)
	{
		LapizDocumentLoader *loader;

		loader = LAPIZ_DOCUMENT_LOADER (gvloader);

		g_output_stream_flush (gvloader->priv->output,
				       NULL,
				       &gvloader->priv->error);

		loader->auto_detected_encoding =
			lapiz_smart_charset_converter_get_guessed (gvloader->priv->converter);

		loader->auto_detected_newline_type =
			lapiz_document_output_stream_detect_newline_type (LAPIZ_DOCUMENT_OUTPUT_STREAM (gvloader->priv->output));

		/* Check if we needed some fallback char, if so, check if there was
		   a previous error and if not set a fallback used error */
		/* FIXME Uncomment this when we want to manage conversion fallback */
		/*if ((lapiz_smart_charset_converter_get_num_fallbacks (gvloader->priv->converter) != 0) &&
		    gvloader->priv->error == NULL)
		{
			g_set_error_literal (&gvloader->priv->error,
					     LAPIZ_DOCUMENT_ERROR,
					     LAPIZ_DOCUMENT_ERROR_CONVERSION_FALLBACK,
					     "There was a conversion error and it was "
					     "needed to use a fallback char");
		}*/

		write_complete (async);

		return;
	}

	write_file_chunk (async);
}

static void
read_file_chunk (AsyncData *async)
{
	LapizGioDocumentLoader *gvloader;

	gvloader = async->loader;

	g_input_stream_read_async (G_INPUT_STREAM (gvloader->priv->stream),
				   gvloader->priv->buffer,
				   READ_CHUNK_SIZE,
				   G_PRIORITY_HIGH,
				   async->cancellable,
				   (GAsyncReadyCallback) async_read_cb,
				   async);
}

static GSList *
get_candidate_encodings (LapizGioDocumentLoader *gvloader)
{
	const LapizEncoding *metadata;
	GSList *encodings = NULL;

	encodings = lapiz_prefs_manager_get_auto_detected_encodings ();

	metadata = get_metadata_encoding (LAPIZ_DOCUMENT_LOADER (gvloader));
	if (metadata != NULL)
	{
		encodings = g_slist_prepend (encodings, (gpointer)metadata);
	}

	return encodings;
}

static void
finish_query_info (AsyncData *async)
{
	LapizGioDocumentLoader *gvloader;
	LapizDocumentLoader *loader;
	GInputStream *conv_stream;
	GFileInfo *info;
	GSList *candidate_encodings;

	gvloader = async->loader;
	loader = LAPIZ_DOCUMENT_LOADER (gvloader);
	info = loader->info;

	/* if it's not a regular file, error out... */
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) &&
	    g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR)
	{
		g_set_error (&gvloader->priv->error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_REGULAR_FILE,
			     "Not a regular file");

		remote_load_completed_or_failed (gvloader, async);

		return;
	}

	/* Get the candidate encodings */
	if (loader->encoding == NULL)
	{
		candidate_encodings = get_candidate_encodings (gvloader);
	}
	else
	{
		candidate_encodings = g_slist_prepend (NULL, (gpointer) loader->encoding);
	}

	gvloader->priv->converter = lapiz_smart_charset_converter_new (candidate_encodings);
	g_slist_free (candidate_encodings);

	conv_stream = g_converter_input_stream_new (gvloader->priv->stream,
						    G_CONVERTER (gvloader->priv->converter));
	g_object_unref (gvloader->priv->stream);

	gvloader->priv->stream = conv_stream;

	/* Output stream */
	gvloader->priv->output = lapiz_document_output_stream_new (loader->document);

	/* start reading */
	read_file_chunk (async);
}

static void
query_info_cb (GFile        *source G_GNUC_UNUSED,
	       GAsyncResult *res,
	       AsyncData    *async)
{
	LapizGioDocumentLoader *gvloader;
	GFileInfo *info;
	GError *error = NULL;

	lapiz_debug (DEBUG_LOADER);

	/* manually check the cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	gvloader = async->loader;

	/* finish the info query */
	info = g_file_query_info_finish (gvloader->priv->gfile,
	                                 res,
	                                 &error);

	if (info == NULL)
	{
		/* propagate the error and clean up */
		async_failed (async, error);
		return;
	}

	LAPIZ_DOCUMENT_LOADER (gvloader)->info = info;

	finish_query_info (async);
}

static void
mount_ready_callback (GFile        *file,
		      GAsyncResult *res,
		      AsyncData    *async)
{
	GError *error = NULL;
	gboolean mounted;

	lapiz_debug (DEBUG_LOADER);

	/* manual check for cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	mounted = g_file_mount_enclosing_volume_finish (file, res, &error);

	if (!mounted)
	{
		async_failed (async, error);
	}
	else
	{
		/* try again to open the file for reading */
		open_async_read (async);
	}
}

static void
recover_not_mounted (AsyncData *async)
{
	LapizDocument *doc;
	GMountOperation *mount_operation;

	lapiz_debug (DEBUG_LOADER);

	doc = lapiz_document_loader_get_document (LAPIZ_DOCUMENT_LOADER (async->loader));
	mount_operation = _lapiz_document_create_mount_operation (doc);

	async->tried_mount = TRUE;
	g_file_mount_enclosing_volume (async->loader->priv->gfile,
				       G_MOUNT_MOUNT_NONE,
				       mount_operation,
				       async->cancellable,
				       (GAsyncReadyCallback) mount_ready_callback,
				       async);

	g_object_unref (mount_operation);
}

static void
async_read_ready_callback (GObject      *source G_GNUC_UNUSED,
			   GAsyncResult *res,
		           AsyncData    *async)
{
	GError *error = NULL;
	LapizGioDocumentLoader *gvloader;

	lapiz_debug (DEBUG_LOADER);

	/* manual check for cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	gvloader = async->loader;

	gvloader->priv->stream = G_INPUT_STREAM (g_file_read_finish (gvloader->priv->gfile,
								     res, &error));

	if (!gvloader->priv->stream)
	{
		if (error->code == G_IO_ERROR_NOT_MOUNTED && !async->tried_mount)
		{
			recover_not_mounted (async);
			g_error_free (error);
			return;
		}

		/* Propagate error */
		g_propagate_error (&gvloader->priv->error, error);
		lapiz_document_loader_loading (LAPIZ_DOCUMENT_LOADER (gvloader),
					       TRUE,
					       gvloader->priv->error);

		async_data_free (async);
		return;
	}

	/* get the file info: note we cannot use
	 * g_file_input_stream_query_info_async since it is not able to get the
	 * content type etc, beside it is not supported by gvfs.
	 * Using the file instead of the stream is slightly racy, but for
	 * loading this is not too bad...
	 */
	g_file_query_info_async (gvloader->priv->gfile,
				 REMOTE_QUERY_ATTRIBUTES,
                                 G_FILE_QUERY_INFO_NONE,
				 G_PRIORITY_HIGH,
				 async->cancellable,
				 (GAsyncReadyCallback) query_info_cb,
				 async);
}

static void
open_async_read (AsyncData *async)
{
	g_file_read_async (async->loader->priv->gfile,
	                   G_PRIORITY_HIGH,
	                   async->cancellable,
	                   (GAsyncReadyCallback) async_read_ready_callback,
	                   async);
}

static void
lapiz_gio_document_loader_load (LapizDocumentLoader *loader)
{
	LapizGioDocumentLoader *gvloader = LAPIZ_GIO_DOCUMENT_LOADER (loader);
	AsyncData *async;

	lapiz_debug (DEBUG_LOADER);

	/* make sure no load operation is currently running */
	g_return_if_fail (gvloader->priv->cancellable == NULL);

	gvloader->priv->gfile = g_file_new_for_uri (loader->uri);

	/* loading start */
	lapiz_document_loader_loading (LAPIZ_DOCUMENT_LOADER (gvloader),
				       FALSE,
				       NULL);

	gvloader->priv->cancellable = g_cancellable_new ();
	async = async_data_new (gvloader);

	open_async_read (async);
}

static goffset
lapiz_gio_document_loader_get_bytes_read (LapizDocumentLoader *loader)
{
	return LAPIZ_GIO_DOCUMENT_LOADER (loader)->priv->bytes_read;
}

static gboolean
lapiz_gio_document_loader_cancel (LapizDocumentLoader *loader)
{
	LapizGioDocumentLoader *gvloader = LAPIZ_GIO_DOCUMENT_LOADER (loader);

	if (gvloader->priv->cancellable == NULL)
		return FALSE;

	g_cancellable_cancel (gvloader->priv->cancellable);

	g_set_error (&gvloader->priv->error,
		     G_IO_ERROR,
		     G_IO_ERROR_CANCELLED,
		     "Operation cancelled");

	remote_load_completed_or_failed (gvloader, NULL);

	return TRUE;
}
