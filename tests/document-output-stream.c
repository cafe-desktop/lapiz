/*
 * document-output-stream.c
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


#include "lapiz-document-output-stream.h"
#include "lapiz-prefs-manager-app.h"
#include <gio/gio.h>
#include <ctk/ctk.h>
#include <glib.h>
#include <string.h>

static void
test_consecutive_write (const gchar *inbuf,
			const gchar *outbuf,
			gsize write_chunk_len,
			LapizDocumentNewlineType newline_type)
{
	LapizDocument *doc;
	GOutputStream *out;
	gsize len;
	gssize n, w;
	GError *err = NULL;
	gchar *b;
	LapizDocumentNewlineType type;

	doc = lapiz_document_new ();
	out = lapiz_document_output_stream_new (doc);

	n = 0;

	do
	{
		len = MIN (write_chunk_len, strlen (inbuf + n));
		w = g_output_stream_write (out, inbuf + n, len, NULL, &err);
		g_assert_cmpint (w, >=, 0);
		g_assert_no_error (err);

		n += w;
	} while (w != 0);

	g_assert(g_output_stream_flush (out, NULL, &err) == TRUE);
	g_assert_no_error (err);

	g_object_get (G_OBJECT (doc), "text", &b, NULL);

	g_assert_cmpstr (inbuf, ==, b);
	g_free (b);

	type = lapiz_document_output_stream_detect_newline_type (LAPIZ_DOCUMENT_OUTPUT_STREAM (out));
	g_assert (type == newline_type);

	g_output_stream_close (out, NULL, &err);
	g_assert_no_error (err);

	g_object_get (G_OBJECT (doc), "text", &b, NULL);

	g_assert_cmpstr (outbuf, ==, b);
	g_free (b);

	g_assert (ctk_text_buffer_get_modified (CTK_TEXT_BUFFER (doc)) == FALSE);

	g_object_unref (doc);
	g_object_unref (out);
}

static void
test_empty ()
{
	test_consecutive_write ("", "", 10, LAPIZ_DOCUMENT_NEWLINE_TYPE_DEFAULT);
	test_consecutive_write ("\r\n", "", 10, LAPIZ_DOCUMENT_NEWLINE_TYPE_CR_LF);
	test_consecutive_write ("\r", "", 10, LAPIZ_DOCUMENT_NEWLINE_TYPE_CR);
	test_consecutive_write ("\n", "", 10, LAPIZ_DOCUMENT_NEWLINE_TYPE_LF);
}

static void
test_consecutive ()
{
	test_consecutive_write ("hello\nhow\nare\nyou", "hello\nhow\nare\nyou", 3,
				LAPIZ_DOCUMENT_NEWLINE_TYPE_LF);
	test_consecutive_write ("hello\rhow\rare\ryou", "hello\rhow\rare\ryou", 3,
				LAPIZ_DOCUMENT_NEWLINE_TYPE_CR);
	test_consecutive_write ("hello\r\nhow\r\nare\r\nyou", "hello\r\nhow\r\nare\r\nyou", 3,
				LAPIZ_DOCUMENT_NEWLINE_TYPE_CR_LF);
}

static void
test_consecutive_tnewline ()
{
	test_consecutive_write ("hello\nhow\nare\nyou\n", "hello\nhow\nare\nyou", 3,
				LAPIZ_DOCUMENT_NEWLINE_TYPE_LF);
	test_consecutive_write ("hello\rhow\rare\ryou\r", "hello\rhow\rare\ryou", 3,
				LAPIZ_DOCUMENT_NEWLINE_TYPE_CR);
	test_consecutive_write ("hello\r\nhow\r\nare\r\nyou\r\n", "hello\r\nhow\r\nare\r\nyou", 3,
				LAPIZ_DOCUMENT_NEWLINE_TYPE_CR_LF);
}

static void
test_big_char ()
{
	test_consecutive_write ("\343\203\200\343\203\200", "\343\203\200\343\203\200", 2,
				LAPIZ_DOCUMENT_NEWLINE_TYPE_LF);
}

int main (int   argc,
          char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	lapiz_prefs_manager_app_init ();

	g_test_add_func ("/document-output-stream/empty", test_empty);

	g_test_add_func ("/document-output-stream/consecutive", test_consecutive);
	g_test_add_func ("/document-output-stream/consecutive_tnewline", test_consecutive_tnewline);
	g_test_add_func ("/document-output-stream/big-char", test_big_char);

	return g_test_run ();
}
