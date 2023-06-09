/*
 * document-loader.c
 * This file is part of lapiz
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
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

#include "lapiz-gio-document-loader.h"
#include "lapiz-prefs-manager-app.h"
#include <gio/gio.h>
#include <ctk/ctk.h>
#include <glib.h>
#include <string.h>

static gboolean test_completed;

typedef struct
{
	const gchar   *in_buffer;
	gint           newline_type;
	GFile         *file;
} LoaderTestData;

static GFile *
create_document (const gchar *filename,
                 const gchar *contents)
{
	GError *error = NULL;

	if (!g_file_set_contents (filename, contents, -1, &error))
	{
		g_assert_no_error (error);
	}

	return g_file_new_for_path (filename);
}

static void
delete_document (GFile *location)
{
	if (g_file_query_exists (location, NULL))
	{
		GError *err = NULL;

		g_file_delete (location, NULL, &err);
		g_assert_no_error (err);
	}

	test_completed = TRUE;
}

static void
on_document_loaded (LapizDocument  *document,
                    GError         *error,
                    LoaderTestData *data)
{
	CtkTextIter start, end;

	g_assert_no_error (error);

	if (data->in_buffer != NULL)
	{
		gchar *text;

		ctk_text_buffer_get_bounds (CTK_TEXT_BUFFER (document), &start, &end);
		text = ctk_text_iter_get_slice (&start, &end);

		g_assert_cmpstr (text, ==, data->in_buffer);

		g_free (text);
	}

	if (data->newline_type != -1)
	{
		g_assert_cmpint (lapiz_document_get_newline_type (document),
		                 ==,
		                 data->newline_type);
	}

	delete_document (data->file);
}

static void
test_loader (const gchar *filename,
             const gchar *contents,
             const gchar *in_buffer,
             gint         newline_type)
{
	GFile *file;
	gchar *uri;
	LapizDocument *document;

	file = create_document (filename, contents);

	document = lapiz_document_new ();

	LoaderTestData *data = g_slice_new (LoaderTestData);
	data->in_buffer = in_buffer;
	data->newline_type = newline_type;
	data->file = file;

	test_completed = FALSE;

	g_signal_connect (document,
	                  "loaded",
	                  G_CALLBACK (on_document_loaded),
	                  data);

	uri = g_file_get_uri (file);

	lapiz_document_load (document, uri, lapiz_encoding_get_utf8 (), 0, FALSE);

	g_free (uri);

	while (!test_completed)
	{
		g_main_context_iteration (NULL, TRUE);
	}

	g_slice_free (LoaderTestData, data);
	g_object_unref (file);
	g_object_unref (document);
}

static void
test_end_line_stripping ()
{
	test_loader ("document-loader.txt",
	             "hello world\n",
	             "hello world",
	             -1);

	test_loader ("document-loader.txt",
	             "hello world",
	             "hello world",
	             -1);

	test_loader ("document-loader.txt",
	             "\nhello world",
	             "\nhello world",
	             -1);

	test_loader ("document-loader.txt",
	             "\nhello world\n",
	             "\nhello world",
	             -1);

	test_loader ("document-loader.txt",
	             "hello world\n\n",
	             "hello world\n",
	             -1);

	test_loader ("document-loader.txt",
	             "hello world\r\n",
	             "hello world",
	             -1);

	test_loader ("document-loader.txt",
	             "hello world\r\n\r\n",
	             "hello world\r\n",
	             -1);

	test_loader ("document-loader.txt",
	             "\n",
	             "",
	             -1);

	test_loader ("document-loader.txt",
	             "\r\n",
	             "",
	             -1);

	test_loader ("document-loader.txt",
	             "\n\n",
	             "\n",
	             -1);

	test_loader ("document-loader.txt",
	             "\r\n\r\n",
	             "\r\n",
	             -1);
}

static void
test_end_new_line_detection ()
{
	test_loader ("document-loader.txt",
	             "hello world\n",
	             NULL,
	             LAPIZ_DOCUMENT_NEWLINE_TYPE_LF);

	test_loader ("document-loader.txt",
	             "hello world\r\n",
	             NULL,
	             LAPIZ_DOCUMENT_NEWLINE_TYPE_CR_LF);

	test_loader ("document-loader.txt",
	             "hello world\r",
	             NULL,
	             LAPIZ_DOCUMENT_NEWLINE_TYPE_CR);
}

static void
test_begin_new_line_detection ()
{
	test_loader ("document-loader.txt",
	             "\nhello world",
	             NULL,
	             LAPIZ_DOCUMENT_NEWLINE_TYPE_LF);

	test_loader ("document-loader.txt",
	             "\r\nhello world",
	             NULL,
	             LAPIZ_DOCUMENT_NEWLINE_TYPE_CR_LF);

	test_loader ("document-loader.txt",
	             "\rhello world",
	             NULL,
	             LAPIZ_DOCUMENT_NEWLINE_TYPE_CR);
}

int main (int   argc,
          char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	lapiz_prefs_manager_app_init ();

	g_test_add_func ("/document-loader/end-line-stripping", test_end_line_stripping);
	g_test_add_func ("/document-loader/end-new-line-detection", test_end_new_line_detection);
	g_test_add_func ("/document-loader/begin-new-line-detection", test_begin_new_line_detection);

	return g_test_run ();
}
