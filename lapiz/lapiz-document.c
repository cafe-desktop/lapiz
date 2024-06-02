/*
 * lapiz-document.c
 * This file is part of lapiz
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
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
 * Modified by the lapiz Team, 1998-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <ctk/ctk.h>

#include "lapiz-prefs-manager-app.h"
#include "lapiz-document.h"
#include "lapiz-debug.h"
#include "lapiz-utils.h"
#include "lapiz-language-manager.h"
#include "lapiz-style-scheme-manager.h"
#include "lapiz-document-loader.h"
#include "lapiz-document-saver.h"
#include "lapiz-marshal.h"
#include "lapiz-enum-types.h"
#include "lapiztextregion.h"

#ifndef ENABLE_GVFS_METADATA
#include "lapiz-metadata-manager.h"
#else
#define METADATA_QUERY "metadata::*"
#endif

#undef ENABLE_PROFILE

#ifdef ENABLE_PROFILE
#define PROFILE(x) x
#else
#define PROFILE(x)
#endif

PROFILE (static GTimer *timer = NULL)

#ifdef MAXPATHLEN
#define LAPIZ_MAX_PATH_LEN  MAXPATHLEN
#elif defined (PATH_MAX)
#define LAPIZ_MAX_PATH_LEN  PATH_MAX
#else
#define LAPIZ_MAX_PATH_LEN  2048
#endif

static void	lapiz_document_load_real	(LapizDocument          *doc,
						 const gchar            *uri,
						 const LapizEncoding    *encoding,
						 gint                    line_pos,
						 gboolean                create);
static void	lapiz_document_save_real	(LapizDocument          *doc,
						 const gchar            *uri,
						 const LapizEncoding    *encoding,
						 LapizDocumentSaveFlags  flags);
static void	to_search_region_range 		(LapizDocument *doc,
						 CtkTextIter   *start,
						 CtkTextIter   *end);
static void 	insert_text_cb		 	(LapizDocument *doc,
						 CtkTextIter   *pos,
						 const gchar   *text,
						 gint           length);

static void	delete_range_cb 		(LapizDocument *doc,
						 CtkTextIter   *start,
						 CtkTextIter   *end);

struct _LapizDocumentPrivate
{
	gchar	    *uri;
	gint 	     untitled_number;
	gchar       *short_name;

	GFileInfo   *metadata_info;

	const LapizEncoding *encoding;

	gchar	    *content_type;

	GTimeVal     mtime;
	GTimeVal     time_of_last_save_or_load;

	guint        search_flags;
	gchar       *search_text;
	gchar       *last_replace_text;
	gint	     num_of_lines_search_text;

	LapizDocumentNewlineType newline_type;

	/* Temp data while loading */
	LapizDocumentLoader *loader;
	gboolean             create; /* Create file if uri points
	                              * to a non existing file */
	const LapizEncoding *requested_encoding;
	gint                 requested_line_pos;

	/* Saving stuff */
	LapizDocumentSaver *saver;

	/* Search highlighting support variables */
	LapizTextRegion *to_search_region;
	CtkTextTag      *found_tag;

	/* Mount operation factory */
	LapizMountOperationFactory  mount_operation_factory;
	gpointer		    mount_operation_userdata;

	gint readonly : 1;
	gint last_save_was_manually : 1;
	gint language_set_by_user : 1;
	gint stop_cursor_moved_emission : 1;
	gint dispose_has_run : 1;
};

enum {
	PROP_0,

	PROP_URI,
	PROP_SHORTNAME,
	PROP_CONTENT_TYPE,
	PROP_MIME_TYPE,
	PROP_READ_ONLY,
	PROP_ENCODING,
	PROP_CAN_SEARCH_AGAIN,
	PROP_ENABLE_SEARCH_HIGHLIGHTING,
	PROP_NEWLINE_TYPE
};

enum {
	CURSOR_MOVED,
	LOAD,
	LOADING,
	LOADED,
	SAVE,
	SAVING,
	SAVED,
	SEARCH_HIGHLIGHT_UPDATED,
	LAST_SIGNAL
};

static guint document_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (LapizDocument, lapiz_document, CTK_SOURCE_TYPE_BUFFER)

GQuark
lapiz_document_error_quark (void)
{
	static GQuark quark = 0;

	if (G_UNLIKELY (quark == 0))
		quark = g_quark_from_static_string ("lapiz_io_load_error");

	return quark;
}

static GHashTable *allocated_untitled_numbers = NULL;

static gint
get_untitled_number (void)
{
	gint i = 1;

	if (allocated_untitled_numbers == NULL)
		allocated_untitled_numbers = g_hash_table_new (NULL, NULL);

	g_return_val_if_fail (allocated_untitled_numbers != NULL, -1);

	while (TRUE)
	{
		if (g_hash_table_lookup (allocated_untitled_numbers, GINT_TO_POINTER (i)) == NULL)
		{
			g_hash_table_insert (allocated_untitled_numbers,
					     GINT_TO_POINTER (i),
					     GINT_TO_POINTER (i));

			return i;
		}

		++i;
	}
}

static void
release_untitled_number (gint n)
{
	g_return_if_fail (allocated_untitled_numbers != NULL);

	g_hash_table_remove (allocated_untitled_numbers, GINT_TO_POINTER (n));
}

static void
lapiz_document_dispose (GObject *object)
{
	LapizDocument *doc = LAPIZ_DOCUMENT (object);

	lapiz_debug (DEBUG_DOCUMENT);

	/* Metadata must be saved here and not in finalize
	 * because the language is gone by the time finalize runs.
	 * beside if some plugin prevents proper finalization by
	 * holding a ref to the doc, we still save the metadata */
	if ((!doc->priv->dispose_has_run) && (doc->priv->uri != NULL))
	{
		CtkTextIter iter;
		gchar *position;
		const gchar *language = NULL;

		if (doc->priv->language_set_by_user)
		{
			CtkSourceLanguage *lang;

			lang = lapiz_document_get_language (doc);

			if (lang == NULL)
				language = "_NORMAL_";
			else
				language = ctk_source_language_get_id (lang);
		}

		ctk_text_buffer_get_iter_at_mark (
				CTK_TEXT_BUFFER (doc),
				&iter,
				ctk_text_buffer_get_insert (CTK_TEXT_BUFFER (doc)));

		position = g_strdup_printf ("%d",
					    ctk_text_iter_get_offset (&iter));

		if (language == NULL)
			lapiz_document_set_metadata (doc, LAPIZ_METADATA_ATTRIBUTE_POSITION,
						     position, NULL);
		else
			lapiz_document_set_metadata (doc, LAPIZ_METADATA_ATTRIBUTE_POSITION,
						     position, LAPIZ_METADATA_ATTRIBUTE_LANGUAGE,
						     language, NULL);
		g_free (position);
	}

	if (doc->priv->loader)
	{
		g_object_unref (doc->priv->loader);
		doc->priv->loader = NULL;
	}

	if (doc->priv->metadata_info != NULL)
	{
		g_object_unref (doc->priv->metadata_info);
		doc->priv->metadata_info = NULL;
	}

	doc->priv->dispose_has_run = TRUE;

	G_OBJECT_CLASS (lapiz_document_parent_class)->dispose (object);
}

static void
lapiz_document_finalize (GObject *object)
{
	LapizDocument *doc = LAPIZ_DOCUMENT (object);

	lapiz_debug (DEBUG_DOCUMENT);

	if (doc->priv->untitled_number > 0)
	{
		g_return_if_fail (doc->priv->uri == NULL);
		release_untitled_number (doc->priv->untitled_number);
	}

	g_free (doc->priv->uri);
	g_free (doc->priv->content_type);
	g_free (doc->priv->search_text);
	g_free (doc->priv->last_replace_text);

	if (doc->priv->to_search_region != NULL)
	{
		/* we can't delete marks if we're finalizing the buffer */
		lapiz_text_region_destroy (doc->priv->to_search_region, FALSE);
	}

	G_OBJECT_CLASS (lapiz_document_parent_class)->finalize (object);
}

static void
lapiz_document_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	LapizDocument *doc = LAPIZ_DOCUMENT (object);

	switch (prop_id)
	{
		case PROP_URI:
			g_value_set_string (value, doc->priv->uri);
			break;
		case PROP_SHORTNAME:
			g_value_take_string (value, lapiz_document_get_short_name_for_display (doc));
			break;
		case PROP_CONTENT_TYPE:
			g_value_take_string (value, lapiz_document_get_content_type (doc));
			break;
		case PROP_MIME_TYPE:
			g_value_take_string (value, lapiz_document_get_mime_type (doc));
			break;
		case PROP_READ_ONLY:
			g_value_set_boolean (value, doc->priv->readonly);
			break;
		case PROP_ENCODING:
			g_value_set_boxed (value, doc->priv->encoding);
			break;
		case PROP_CAN_SEARCH_AGAIN:
			g_value_set_boolean (value, lapiz_document_get_can_search_again (doc));
			break;
		case PROP_ENABLE_SEARCH_HIGHLIGHTING:
			g_value_set_boolean (value, lapiz_document_get_enable_search_highlighting (doc));
			break;
		case PROP_NEWLINE_TYPE:
			g_value_set_enum (value, doc->priv->newline_type);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_document_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	LapizDocument *doc = LAPIZ_DOCUMENT (object);

	switch (prop_id)
	{
		case PROP_ENABLE_SEARCH_HIGHLIGHTING:
			lapiz_document_set_enable_search_highlighting (doc,
								       g_value_get_boolean (value));
			break;
		case PROP_NEWLINE_TYPE:
			lapiz_document_set_newline_type (doc,
							 g_value_get_enum (value));
			break;
		case PROP_SHORTNAME:
			lapiz_document_set_short_name_for_display (doc,
			                                           g_value_get_string (value));
			break;
		case PROP_CONTENT_TYPE:
			lapiz_document_set_content_type (doc,
			                                 g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
emit_cursor_moved (LapizDocument *doc)
{
	if (!doc->priv->stop_cursor_moved_emission)
	{
		g_signal_emit (doc,
			       document_signals[CURSOR_MOVED],
			       0);
	}
}

static void
lapiz_document_mark_set (CtkTextBuffer     *buffer,
                         const CtkTextIter *iter,
                         CtkTextMark       *mark)
{
	LapizDocument *doc = LAPIZ_DOCUMENT (buffer);

	if (CTK_TEXT_BUFFER_CLASS (lapiz_document_parent_class)->mark_set)
		CTK_TEXT_BUFFER_CLASS (lapiz_document_parent_class)->mark_set (buffer,
									       iter,
									       mark);

	if (mark == ctk_text_buffer_get_insert (buffer))
	{
		emit_cursor_moved (doc);
	}
}

static void
lapiz_document_changed (CtkTextBuffer *buffer)
{
	emit_cursor_moved (LAPIZ_DOCUMENT (buffer));

	CTK_TEXT_BUFFER_CLASS (lapiz_document_parent_class)->changed (buffer);
}

static void
lapiz_document_class_init (LapizDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CtkTextBufferClass *buf_class = CTK_TEXT_BUFFER_CLASS (klass);

	object_class->dispose = lapiz_document_dispose;
	object_class->finalize = lapiz_document_finalize;
	object_class->get_property = lapiz_document_get_property;
	object_class->set_property = lapiz_document_set_property;

	buf_class->mark_set = lapiz_document_mark_set;
	buf_class->changed = lapiz_document_changed;

	klass->load = lapiz_document_load_real;
	klass->save = lapiz_document_save_real;

	g_object_class_install_property (object_class, PROP_URI,
					 g_param_spec_string ("uri",
							      "URI",
							      "The document's URI",
							      NULL,
							      G_PARAM_READABLE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_SHORTNAME,
					 g_param_spec_string ("shortname",
							      "Short Name",
							      "The document's short name",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_CONTENT_TYPE,
					 g_param_spec_string ("content-type",
							      "Content Type",
							      "The document's Content Type",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_MIME_TYPE,
					 g_param_spec_string ("mime-type",
							      "MIME Type",
							      "The document's MIME Type",
							      "text/plain",
							      G_PARAM_READABLE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_READ_ONLY,
					 g_param_spec_boolean ("read-only",
							       "Read Only",
							       "Whether the document is read only or not",
							       FALSE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_ENCODING,
					 g_param_spec_boxed ("encoding",
							     "Encoding",
							     "The LapizEncoding used for the document",
							     LAPIZ_TYPE_ENCODING,
							     G_PARAM_READABLE |
							     G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_CAN_SEARCH_AGAIN,
					 g_param_spec_boolean ("can-search-again",
							       "Can search again",
							       "Whether it's possible to search again in the document",
							       FALSE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_ENABLE_SEARCH_HIGHLIGHTING,
					 g_param_spec_boolean ("enable-search-highlighting",
							       "Enable Search Highlighting",
							       "Whether all the occurrences of the searched string must be highlighted",
							       FALSE,
							       G_PARAM_READWRITE |
							       G_PARAM_STATIC_STRINGS));

	/**
	 * LapizDocument:newline-type:
	 *
	 * The :newline-type property determines what is considered
	 * as a line ending when saving the document
	 */
	g_object_class_install_property (object_class, PROP_NEWLINE_TYPE,
	                                 g_param_spec_enum ("newline-type",
	                                                    "Newline type",
	                                                    "The accepted types of line ending",
	                                                    LAPIZ_TYPE_DOCUMENT_NEWLINE_TYPE,
	                                                    LAPIZ_DOCUMENT_NEWLINE_TYPE_LF,
	                                                    G_PARAM_READWRITE |
	                                                    G_PARAM_STATIC_NAME |
	                                                    G_PARAM_STATIC_BLURB));

	/* This signal is used to update the cursor position is the statusbar,
	 * it's emitted either when the insert mark is moved explicitely or
	 * when the buffer changes (insert/delete).
	 * We prevent the emission of the signal during replace_all to
	 * improve performance.
	 */
	document_signals[CURSOR_MOVED] =
   		g_signal_new ("cursor-moved",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizDocumentClass, cursor_moved),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	/**
	 * LapizDocument::load:
	 * @document: the #LapizDocument.
	 * @uri: the uri where to load the document from.
	 * @encoding: the #LapizEncoding to encode the document.
	 * @line_pos: the line to show.
	 * @create: whether the document should be created if it doesn't exist.
	 *
	 * The "load" signal is emitted when a document is loaded.
	 */
	document_signals[LOAD] =
		g_signal_new ("load",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizDocumentClass, load),
			      NULL, NULL,
			      lapiz_marshal_VOID__STRING_BOXED_INT_BOOLEAN,
			      G_TYPE_NONE,
			      4,
			      G_TYPE_STRING,
			      /* we rely on the fact that the LapizEncoding pointer stays
			       * the same forever */
			      LAPIZ_TYPE_ENCODING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_INT,
			      G_TYPE_BOOLEAN);


	document_signals[LOADING] =
		g_signal_new ("loading",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizDocumentClass, loading),
			      NULL, NULL,
			      lapiz_marshal_VOID__UINT64_UINT64,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_UINT64,
			      G_TYPE_UINT64);

	document_signals[LOADED] =
   		g_signal_new ("loaded",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizDocumentClass, loaded),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	/**
	 * LapizDocument::save:
	 * @document: the #LapizDocument.
	 * @uri: the uri where the document is about to be saved.
	 * @encoding: the #LapizEncoding used to save the document.
	 * @flags: the #LapizDocumentSaveFlags for the save operation.
	 *
	 * The "save" signal is emitted when the document is saved.
	 */
	document_signals[SAVE] =
		g_signal_new ("save",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizDocumentClass, save),
			      NULL, NULL,
			      lapiz_marshal_VOID__STRING_BOXED_FLAGS,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING,
			      /* we rely on the fact that the LapizEncoding pointer stays
			       * the same forever */
			      LAPIZ_TYPE_ENCODING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      LAPIZ_TYPE_DOCUMENT_SAVE_FLAGS);

	document_signals[SAVING] =
   		g_signal_new ("saving",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizDocumentClass, saving),
			      NULL, NULL,
			      lapiz_marshal_VOID__UINT64_UINT64,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_UINT64,
			      G_TYPE_UINT64);

	document_signals[SAVED] =
   		g_signal_new ("saved",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizDocumentClass, saved),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	document_signals[SEARCH_HIGHLIGHT_UPDATED] =
	    	g_signal_new ("search_highlight_updated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizDocumentClass, search_highlight_updated),
			      NULL, NULL,
			      lapiz_marshal_VOID__BOXED_BOXED,
			      G_TYPE_NONE,
			      2,
			      CTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
			      CTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static gboolean
file_with_bom (GFile *file)
{
	FILE    *testfile;
	gchar    c;
	int      i;
	gchar    bom[3];
	gchar   *file_path;

	bom[0] = bom[1] = bom[2] = 0;

	file_path = g_file_get_path (file);

	testfile = fopen (file_path, "r");

	g_free (file_path);

	if (testfile == NULL)
	{
		perror ("fopen");
		return FALSE;
	}

	for (i = 0; i < 3; i++)
	{
		c = fgetc (testfile);

		if (c == EOF)
			break;
		else
			bom[i] = c;
	}

	fclose (testfile);

	if ((bom[0] == '\357') &&
	    (bom[1] == '\273') &&
	    (bom[2] == '\277'))
		return TRUE;
	else
		return FALSE;
}

static void
set_language (LapizDocument     *doc,
              CtkSourceLanguage *lang,
              gboolean           set_by_user)
{
	CtkSourceLanguage *old_lang;
	const gchar       *bom_langs;

	lapiz_debug (DEBUG_DOCUMENT);

	old_lang = ctk_source_buffer_get_language (CTK_SOURCE_BUFFER (doc));

	if (old_lang == lang)
		return;

	bom_langs = "asp,dtl,docbook,html,mxml,mallard,markdown,mediawiki,php,tera,xml,xslt";

	if (g_strrstr (bom_langs, ctk_source_language_get_id (lang)))
	{
		GFile *file;
		file = lapiz_document_get_location (doc);

		if (!file_with_bom (file))
			ctk_source_buffer_set_language (CTK_SOURCE_BUFFER (doc), lang);

		g_object_unref (file);
	}
	else
		ctk_source_buffer_set_language (CTK_SOURCE_BUFFER (doc), lang);

	if (lang != NULL)
		ctk_source_buffer_set_highlight_syntax (CTK_SOURCE_BUFFER (doc),
				 lapiz_prefs_manager_get_enable_syntax_highlighting ());
	else
		ctk_source_buffer_set_highlight_syntax (CTK_SOURCE_BUFFER (doc),
				 FALSE);

	if (set_by_user && (doc->priv->uri != NULL))
	{
		lapiz_document_set_metadata (doc, LAPIZ_METADATA_ATTRIBUTE_LANGUAGE,
			(lang == NULL) ? "_NORMAL_" : ctk_source_language_get_id (lang),
			NULL);
	}

	doc->priv->language_set_by_user = set_by_user;
}

static void
set_encoding (LapizDocument       *doc,
	      const LapizEncoding *encoding,
	      gboolean             set_by_user)
{
	g_return_if_fail (encoding != NULL);

	lapiz_debug (DEBUG_DOCUMENT);

	if (doc->priv->encoding == encoding)
		return;

	doc->priv->encoding = encoding;

	if (set_by_user)
	{
		const gchar *charset;

		charset = lapiz_encoding_get_charset (encoding);

		lapiz_document_set_metadata (doc, LAPIZ_METADATA_ATTRIBUTE_ENCODING,
					     charset, NULL);
	}

	g_object_notify (G_OBJECT (doc), "encoding");
}

static CtkSourceStyleScheme *
get_default_style_scheme (void)
{
	gchar *scheme_id;
	CtkSourceStyleScheme *def_style;
	CtkSourceStyleSchemeManager *manager;

	manager = lapiz_get_style_scheme_manager ();
	scheme_id = lapiz_prefs_manager_get_source_style_scheme ();
	def_style = ctk_source_style_scheme_manager_get_scheme (manager,
								scheme_id);

	if (def_style == NULL)
	{
		g_warning ("Default style scheme '%s' cannot be found, falling back to 'classic' style scheme ", scheme_id);

		def_style = ctk_source_style_scheme_manager_get_scheme (manager, "classic");
		if (def_style == NULL)
		{
			g_warning ("Style scheme 'classic' cannot be found, check your CtkSourceView installation.");
		}
	}

	g_free (scheme_id);

	return def_style;
}

static void
on_uri_changed (LapizDocument *doc,
		GParamSpec    *pspec G_GNUC_UNUSED,
		gpointer       useless G_GNUC_UNUSED)
{
#ifdef ENABLE_GVFS_METADATA
	GFile *location;

	location = lapiz_document_get_location (doc);

	/* load metadata for this uri: we load sync since metadata is
	 * always local so it should be fast and we need the information
	 * right after the uri was set.
	 */
	if (location != NULL)
	{
		GError *error = NULL;

		if (doc->priv->metadata_info != NULL)
			g_object_unref (doc->priv->metadata_info);

		doc->priv->metadata_info = g_file_query_info (location,
							      METADATA_QUERY,
							      G_FILE_QUERY_INFO_NONE,
							      NULL,
							      &error);

		if (error != NULL)
		{
			if (error->code != G_FILE_ERROR_ISDIR &&
			    error->code != G_FILE_ERROR_NOTDIR &&
			    error->code != G_FILE_ERROR_NOENT)
			{
				g_warning ("%s", error->message);
			}

			g_error_free (error);
		}

		g_object_unref (location);
	}
#endif
}

static CtkSourceLanguage *
guess_language (LapizDocument *doc,
		const gchar   *content_type)
{
	gchar *data;
	CtkSourceLanguage *language = NULL;

	data = lapiz_document_get_metadata (doc, LAPIZ_METADATA_ATTRIBUTE_LANGUAGE);

	if (data != NULL)
	{
		lapiz_debug_message (DEBUG_DOCUMENT, "Language from metadata: %s", data);

		if (strcmp (data, "_NORMAL_") != 0)
		{
			language = ctk_source_language_manager_get_language (
						lapiz_get_language_manager (),
						data);
		}

		g_free (data);
	}
	else
	{
		GFile *file;
		gchar *basename = NULL;

		file = lapiz_document_get_location (doc);
		lapiz_debug_message (DEBUG_DOCUMENT, "Sniffing Language");

		if (file)
		{
			basename = g_file_get_basename (file);
		}
		else if (doc->priv->short_name != NULL)
		{
			basename = g_strdup (doc->priv->short_name);
		}

		language = ctk_source_language_manager_guess_language (
					lapiz_get_language_manager (),
					basename,
					content_type);

		g_free (basename);

		if (file != NULL)
		{
			g_object_unref (file);
		}
	}

	return language;
}

static void
on_content_type_changed (LapizDocument *doc,
			 GParamSpec    *pspec G_GNUC_UNUSED,
			 gpointer       useless G_GNUC_UNUSED)
{
	if (!doc->priv->language_set_by_user)
	{
		CtkSourceLanguage *language;

		language = guess_language (doc, doc->priv->content_type);

		lapiz_debug_message (DEBUG_DOCUMENT, "Language: %s",
				     language != NULL ? ctk_source_language_get_name (language) : "None");

		set_language (doc, language, FALSE);
	}
}

static gchar *
get_default_content_type (void)
{
	return g_content_type_from_mime_type ("text/plain");
}

static void
lapiz_document_init (LapizDocument *doc)
{
	CtkSourceStyleScheme *style_scheme;

	lapiz_debug (DEBUG_DOCUMENT);

	doc->priv = lapiz_document_get_instance_private (doc);

	doc->priv->uri = NULL;
	doc->priv->untitled_number = get_untitled_number ();

	doc->priv->metadata_info = NULL;

	doc->priv->content_type = get_default_content_type ();

	doc->priv->readonly = FALSE;

	doc->priv->stop_cursor_moved_emission = FALSE;

	doc->priv->last_save_was_manually = TRUE;
	doc->priv->language_set_by_user = FALSE;

	doc->priv->dispose_has_run = FALSE;

	doc->priv->mtime.tv_sec = 0;
	doc->priv->mtime.tv_usec = 0;

	g_get_current_time (&doc->priv->time_of_last_save_or_load);

	doc->priv->encoding = lapiz_encoding_get_utf8 ();

	doc->priv->newline_type = LAPIZ_DOCUMENT_NEWLINE_TYPE_DEFAULT;

	ctk_source_buffer_set_max_undo_levels (CTK_SOURCE_BUFFER (doc),
					       lapiz_prefs_manager_get_undo_actions_limit ());

	ctk_source_buffer_set_highlight_matching_brackets (CTK_SOURCE_BUFFER (doc),
							   lapiz_prefs_manager_get_bracket_matching ());

	lapiz_document_set_enable_search_highlighting (doc,
						       lapiz_prefs_manager_get_enable_search_highlighting ());

	style_scheme = get_default_style_scheme ();
	if (style_scheme != NULL)
		ctk_source_buffer_set_style_scheme (CTK_SOURCE_BUFFER (doc),
						    style_scheme);

	g_signal_connect_after (doc,
			  	"insert-text",
			  	G_CALLBACK (insert_text_cb),
			  	NULL);

	g_signal_connect_after (doc,
			  	"delete-range",
			  	G_CALLBACK (delete_range_cb),
			  	NULL);

	g_signal_connect (doc,
			  "notify::content-type",
			  G_CALLBACK (on_content_type_changed),
			  NULL);

	g_signal_connect (doc,
			  "notify::uri",
			  G_CALLBACK (on_uri_changed),
			  NULL);
}

LapizDocument *
lapiz_document_new (void)
{
	lapiz_debug (DEBUG_DOCUMENT);

	return LAPIZ_DOCUMENT (g_object_new (LAPIZ_TYPE_DOCUMENT, NULL));
}

static void
set_content_type_no_guess (LapizDocument *doc,
			   const gchar   *content_type)
{
	lapiz_debug (DEBUG_DOCUMENT);

	if (doc->priv->content_type != NULL && content_type != NULL &&
	    (0 == strcmp (doc->priv->content_type, content_type)))
		return;

	g_free (doc->priv->content_type);

	if (content_type == NULL || g_content_type_is_unknown (content_type))
		doc->priv->content_type = get_default_content_type ();
	else
		doc->priv->content_type = g_strdup (content_type);

	g_object_notify (G_OBJECT (doc), "content-type");
}

static void
set_content_type (LapizDocument *doc,
		  const gchar   *content_type)
{
	lapiz_debug (DEBUG_DOCUMENT);

	if (content_type == NULL)
	{
		GFile *file;
		gchar *guessed_type = NULL;

		/* If content type is null, we guess from the filename */
		file = lapiz_document_get_location (doc);
		if (file != NULL)
		{
			gchar *basename;

			basename = g_file_get_basename (file);
			guessed_type = g_content_type_guess (basename, NULL, 0, NULL);

			g_free (basename);
			g_object_unref (file);
		}

		set_content_type_no_guess (doc, guessed_type);

		g_free (guessed_type);
	}
	else
	{
		set_content_type_no_guess (doc, content_type);
	}
}

/**
 * lapiz_document_set_content_type:
 * @doc:
 * @content_type: (allow-none):
 */
void
lapiz_document_set_content_type (LapizDocument *doc,
                                 const gchar   *content_type)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

	set_content_type (doc, content_type);
}

static void
set_uri (LapizDocument *doc,
	 const gchar   *uri)
{
	lapiz_debug (DEBUG_DOCUMENT);

	g_return_if_fail ((uri == NULL) || lapiz_utils_is_valid_uri (uri));

	if (uri != NULL)
	{
		if (doc->priv->uri == uri)
			return;

		g_free (doc->priv->uri);
		doc->priv->uri = g_strdup (uri);

		if (doc->priv->untitled_number > 0)
		{
			release_untitled_number (doc->priv->untitled_number);
			doc->priv->untitled_number = 0;
		}
	}

	g_object_notify (G_OBJECT (doc), "uri");

	if (doc->priv->short_name == NULL)
	{
		g_object_notify (G_OBJECT (doc), "shortname");
	}
}


/**
 * lapiz_document_get_location:
 * @doc: a #LapizDocument
 *
 * Returns: (allow-none) (transfer full): a new #GFile
 */
GFile *
lapiz_document_get_location (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);

	return doc->priv->uri == NULL ? NULL : g_file_new_for_uri (doc->priv->uri);
}

gchar *
lapiz_document_get_uri (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);

	return g_strdup (doc->priv->uri);
}

void
lapiz_document_set_uri (LapizDocument *doc,
			const gchar   *uri)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));
	g_return_if_fail (uri != NULL);

	set_uri (doc, uri);
	set_content_type (doc, NULL);
}

/**
 * lapiz_document_get_uri_for_display:
 * @doc:
 *
 * Note: this never returns %NULL.
 **/
gchar *
lapiz_document_get_uri_for_display (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), g_strdup (""));

	if (doc->priv->uri == NULL)
		return g_strdup_printf (_("Unsaved Document %d"),
					doc->priv->untitled_number);
	else
		return lapiz_utils_uri_for_display (doc->priv->uri);
}

/**
 * lapiz_document_get_short_name_for_display:
 * @doc:
 *
 * Note: this never returns %NULL.
 **/
gchar *
lapiz_document_get_short_name_for_display (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), g_strdup (""));

	if (doc->priv->short_name != NULL)
		return g_strdup (doc->priv->short_name);
	else if (doc->priv->uri == NULL)
		return g_strdup_printf (_("Unsaved Document %d"),
					doc->priv->untitled_number);
	else
		return lapiz_utils_basename_for_display (doc->priv->uri);
}

/**
 * lapiz_document_set_short_name_for_display:
 * @doc:
 * @name: (allow-none):
 */
void
lapiz_document_set_short_name_for_display (LapizDocument *doc,
                                           const gchar   *short_name)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

	g_free (doc->priv->short_name);
	doc->priv->short_name = g_strdup (short_name);

	g_object_notify (G_OBJECT (doc), "shortname");
}

gchar *
lapiz_document_get_content_type (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);

 	return g_strdup (doc->priv->content_type);
}

/**
 * lapiz_document_get_mime_type:
 * @doc:
 *
 * Note: this never returns %NULL.
 **/
gchar *
lapiz_document_get_mime_type (LapizDocument *doc)
{
	gchar *mime_type = NULL;

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), g_strdup ("text/plain"));

	if ((doc->priv->content_type != NULL) &&
	    (!g_content_type_is_unknown (doc->priv->content_type)))
	{
		mime_type = g_content_type_get_mime_type (doc->priv->content_type);
	}

 	return mime_type != NULL ? mime_type : g_strdup ("text/plain");
}

/* Note: do not emit the notify::read-only signal */
static gboolean
set_readonly (LapizDocument *doc,
	      gboolean       readonly)
{
	lapiz_debug (DEBUG_DOCUMENT);

	readonly = (readonly != FALSE);

	if (doc->priv->readonly == readonly)
		return FALSE;

	doc->priv->readonly = readonly;

	return TRUE;
}

/**
 * lapiz_document_set_readonly:
 * @doc: a #LapizDocument
 * @readonly: %TRUE to se the document as read-only
 *
 * If @readonly is %TRUE sets @doc as read-only.
 */
void
_lapiz_document_set_readonly (LapizDocument *doc,
			      gboolean       readonly)
{
	lapiz_debug (DEBUG_DOCUMENT);

	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

	if (set_readonly (doc, readonly))
	{
		g_object_notify (G_OBJECT (doc), "read-only");
	}
}

gboolean
lapiz_document_get_readonly (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), TRUE);

	return doc->priv->readonly;
}

gboolean
_lapiz_document_check_externally_modified (LapizDocument *doc)
{
	GFile *gfile;
	GFileInfo *info;

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);

	if (doc->priv->uri == NULL)
	{
		return FALSE;
	}

	gfile = g_file_new_for_uri (doc->priv->uri);
	info = g_file_query_info (gfile,
				  G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
				  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL, NULL);
	g_object_unref (gfile);

	if (info != NULL)
	{
		/* While at it also check if permissions changed */
		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
		{
			gboolean read_only;

			read_only = !g_file_info_get_attribute_boolean (info,
									G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

			_lapiz_document_set_readonly (doc, read_only);
		}

		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
		{
			GTimeVal timeval;

			g_file_info_get_modification_time (info, &timeval);
			g_object_unref (info);

			return (timeval.tv_sec > doc->priv->mtime.tv_sec) ||
			       (timeval.tv_sec == doc->priv->mtime.tv_sec &&
			       timeval.tv_usec > doc->priv->mtime.tv_usec);
		}
	}

	return FALSE;
}

static void
reset_temp_loading_data (LapizDocument       *doc)
{
	/* the loader has been used, throw it away */
	g_object_unref (doc->priv->loader);
	doc->priv->loader = NULL;

	doc->priv->requested_encoding = NULL;
	doc->priv->requested_line_pos = 0;
}

static void
document_loader_loaded (LapizDocumentLoader *loader,
			const GError        *error,
			LapizDocument       *doc)
{
	/* load was successful */
	if (error == NULL ||
	    (error->domain == LAPIZ_DOCUMENT_ERROR &&
	     error->code == LAPIZ_DOCUMENT_ERROR_CONVERSION_FALLBACK))
	{
		CtkTextIter iter;
		GFileInfo *info;
		const gchar *content_type = NULL;
		gboolean read_only = FALSE;
		GTimeVal mtime = {0, 0};

		info = lapiz_document_loader_get_info (loader);

		if (info)
		{
			if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
				content_type = g_file_info_get_attribute_string (info,
										 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

			if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
				g_file_info_get_modification_time (info, &mtime);

			if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
				read_only = !g_file_info_get_attribute_boolean (info,
										G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
		}

		doc->priv->mtime = mtime;

		set_readonly (doc, read_only);

		g_get_current_time (&doc->priv->time_of_last_save_or_load);

		set_encoding (doc,
			      lapiz_document_loader_get_encoding (loader),
			      (doc->priv->requested_encoding != NULL));

		set_content_type (doc, content_type);

		lapiz_document_set_newline_type (doc,
		                                 lapiz_document_loader_get_newline_type (loader));

		/* move the cursor at the requested line if any */
		if (doc->priv->requested_line_pos > 0)
		{
			/* line_pos - 1 because get_iter_at_line counts from 0 */
			ctk_text_buffer_get_iter_at_line (CTK_TEXT_BUFFER (doc),
							  &iter,
							  doc->priv->requested_line_pos - 1);
		}
		/* else, if enabled, to the position stored in the metadata */
		else if (lapiz_prefs_manager_get_restore_cursor_position ())
		{
			gchar *pos;
			gint offset;

			pos = lapiz_document_get_metadata (doc, LAPIZ_METADATA_ATTRIBUTE_POSITION);

			offset = pos ? atoi (pos) : 0;
			g_free (pos);

			ctk_text_buffer_get_iter_at_offset (CTK_TEXT_BUFFER (doc),
							    &iter,
							    MAX (offset, 0));

			/* make sure it's a valid position, if the file
			 * changed we may have ended up in the middle of
			 * a utf8 character cluster */
			if (!ctk_text_iter_is_cursor_position (&iter))
			{
				ctk_text_buffer_get_start_iter (CTK_TEXT_BUFFER (doc),
								&iter);
			}
		}
		/* otherwise to the top */
		else
		{
			ctk_text_buffer_get_start_iter (CTK_TEXT_BUFFER (doc),
							&iter);
		}

		ctk_text_buffer_place_cursor (CTK_TEXT_BUFFER (doc), &iter);
	}

	/* special case creating a named new doc */
	else if (doc->priv->create &&
	         (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_FOUND) &&
	         (lapiz_utils_uri_has_file_scheme (doc->priv->uri)))
	{
		reset_temp_loading_data (doc);

		g_signal_emit (doc,
			       document_signals[LOADED],
			       0,
			       NULL);

		return;
	}

	g_signal_emit (doc,
		       document_signals[LOADED],
		       0,
		       error);

	reset_temp_loading_data (doc);
}

static void
document_loader_loading (LapizDocumentLoader *loader,
			 gboolean             completed,
			 const GError        *error,
			 LapizDocument       *doc)
{
	if (completed)
	{
		document_loader_loaded (loader, error, doc);
	}
	else
	{
		goffset size = 0;
		goffset read;
		GFileInfo *info;

		info = lapiz_document_loader_get_info (loader);

		if (info && g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
			size = g_file_info_get_attribute_uint64 (info,
								 G_FILE_ATTRIBUTE_STANDARD_SIZE);

		read = lapiz_document_loader_get_bytes_read (loader);

		g_signal_emit (doc,
			       document_signals[LOADING],
			       0,
			       read,
			       size);
	}
}

static void
lapiz_document_load_real (LapizDocument       *doc,
			  const gchar         *uri,
			  const LapizEncoding *encoding,
			  gint                 line_pos,
			  gboolean             create)
{
	g_return_if_fail (doc->priv->loader == NULL);

	lapiz_debug_message (DEBUG_DOCUMENT, "load_real: uri = %s", uri);

	/* create a loader. It will be destroyed when loading is completed */
	doc->priv->loader = lapiz_document_loader_new (doc, uri, encoding);

	g_signal_connect (doc->priv->loader,
			  "loading",
			  G_CALLBACK (document_loader_loading),
			  doc);

	doc->priv->create = create;
	doc->priv->requested_encoding = encoding;
	doc->priv->requested_line_pos = line_pos;

	set_uri (doc, uri);
	set_content_type (doc, NULL);

	lapiz_document_loader_load (doc->priv->loader);
}

/**
 * lapiz_document_load:
 * @doc: the #LapizDocument.
 * @uri: the uri where to load the document from.
 * @encoding: the #LapizEncoding to encode the document.
 * @line_pos: the line to show.
 * @create: whether the document should be created if it doesn't exist.
 *
 * Load a document. This results in the "load" signal to be emitted.
 */
void
lapiz_document_load (LapizDocument       *doc,
		     const gchar         *uri,
		     const LapizEncoding *encoding,
		     gint                 line_pos,
		     gboolean             create)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (lapiz_utils_is_valid_uri (uri));

	g_signal_emit (doc, document_signals[LOAD], 0, uri, encoding, line_pos, create);
}

/**
 * lapiz_document_load_cancel:
 * @doc: the #LapizDocument.
 *
 * Cancel load of a document.
 */
gboolean
lapiz_document_load_cancel (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);

	if (doc->priv->loader == NULL)
		return FALSE;

	return lapiz_document_loader_cancel (doc->priv->loader);
}

static void
document_saver_saving (LapizDocumentSaver *saver,
		       gboolean            completed,
		       const GError       *error,
		       LapizDocument      *doc)
{
	lapiz_debug (DEBUG_DOCUMENT);

	if (completed)
	{
		/* save was successful */
		if (error == NULL)
		{
			const gchar *uri;
			const gchar *content_type = NULL;
			GTimeVal mtime = {0, 0};
			GFileInfo *info;

			uri = lapiz_document_saver_get_uri (saver);
			set_uri (doc, uri);

			info = lapiz_document_saver_get_info (saver);

			if (info != NULL)
			{
				if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
					content_type = g_file_info_get_attribute_string (info,
											 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

				if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
					g_file_info_get_modification_time (info, &mtime);
			}

			set_content_type (doc, content_type);
			doc->priv->mtime = mtime;

			g_get_current_time (&doc->priv->time_of_last_save_or_load);

			_lapiz_document_set_readonly (doc, FALSE);

			ctk_text_buffer_set_modified (CTK_TEXT_BUFFER (doc),
						      FALSE);

			set_encoding (doc,
				      doc->priv->requested_encoding,
				      TRUE);
		}

		g_signal_emit (doc,
			       document_signals[SAVED],
			       0,
			       error);

		/* the saver has been used, throw it away */
		g_object_unref (doc->priv->saver);
		doc->priv->saver = NULL;
	}
	else
	{
		goffset size = 0;
		goffset written = 0;

		size = lapiz_document_saver_get_file_size (saver);
		written = lapiz_document_saver_get_bytes_written (saver);

		lapiz_debug_message (DEBUG_DOCUMENT, "save progress: %" G_GINT64_FORMAT " of %" G_GINT64_FORMAT, written, size);

		g_signal_emit (doc,
			       document_signals[SAVING],
			       0,
			       written,
			       size);
	}
}

static void
lapiz_document_save_real (LapizDocument          *doc,
			  const gchar            *uri,
			  const LapizEncoding    *encoding,
			  LapizDocumentSaveFlags  flags)
{
	g_return_if_fail (doc->priv->saver == NULL);

	/* create a saver, it will be destroyed once saving is complete */
	doc->priv->saver = lapiz_document_saver_new (doc, uri, encoding,
						     doc->priv->newline_type,
						     flags);

	g_signal_connect (doc->priv->saver,
			  "saving",
			  G_CALLBACK (document_saver_saving),
			  doc);

	doc->priv->requested_encoding = encoding;

	lapiz_document_saver_save (doc->priv->saver,
				   &doc->priv->mtime);
}

/**
 * lapiz_document_save:
 * @doc: the #LapizDocument.
 * @flags: optionnal #LapizDocumentSaveFlags.
 *
 * Save the document to its previous location. This results in the "save"
 * signal to be emitted.
 */
void
lapiz_document_save (LapizDocument          *doc,
		     LapizDocumentSaveFlags  flags)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));
	g_return_if_fail (doc->priv->uri != NULL);

	g_signal_emit (doc,
		       document_signals[SAVE],
		       0,
		       doc->priv->uri,
		       doc->priv->encoding,
		       flags);
}

/**
 * lapiz_document_save_as:
 * @doc: the #LapizDocument.
 * @uri: the uri where to save the document.
 * @encoding: the #LapizEncoding to encode the document.
 * @flags: optionnal #LapizDocumentSaveFlags.
 *
 * Save the document to a new location. This results in the "save" signal
 * to be emitted.
 */
void
lapiz_document_save_as (LapizDocument          *doc,
			const gchar            *uri,
			const LapizEncoding    *encoding,
			LapizDocumentSaveFlags  flags)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (encoding != NULL);

	/* priv->mtime refers to the the old uri (if any). Thus, it should be
	 * ignored when saving as. */
	g_signal_emit (doc,
		       document_signals[SAVE],
		       0,
		       uri,
		       encoding,
		       flags | LAPIZ_DOCUMENT_SAVE_IGNORE_MTIME);
}

gboolean
lapiz_document_insert_file (LapizDocument       *doc,
			    CtkTextIter         *iter,
			    const gchar         *uri G_GNUC_UNUSED,
			    const LapizEncoding *encoding G_GNUC_UNUSED)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (ctk_text_iter_get_buffer (iter) ==
				CTK_TEXT_BUFFER (doc), FALSE);

	/* TODO */

	return FALSE;
}

gboolean
lapiz_document_is_untouched (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), TRUE);

	return (doc->priv->uri == NULL) &&
		(!ctk_text_buffer_get_modified (CTK_TEXT_BUFFER (doc)));
}

gboolean
lapiz_document_is_untitled (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), TRUE);

	return (doc->priv->uri == NULL);
}

gboolean
lapiz_document_is_local (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);

	if (doc->priv->uri == NULL)
	{
		return FALSE;
	}

	return lapiz_utils_uri_has_file_scheme (doc->priv->uri);
}

gboolean
lapiz_document_get_deleted (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);

	return doc->priv->uri && !lapiz_utils_uri_exists (doc->priv->uri);
}

/*
 * If @line is bigger than the lines of the document, the cursor is moved
 * to the last line and FALSE is returned.
 */
gboolean
lapiz_document_goto_line (LapizDocument *doc,
			  gint           line)
{
	gboolean ret = TRUE;
	guint line_count;
	CtkTextIter iter;

	lapiz_debug (DEBUG_DOCUMENT);

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (line >= -1, FALSE);

	line_count = ctk_text_buffer_get_line_count (CTK_TEXT_BUFFER (doc));

	if (line >= line_count)
	{
		ret = FALSE;
		ctk_text_buffer_get_end_iter (CTK_TEXT_BUFFER (doc),
					      &iter);
	}
	else
	{
		ctk_text_buffer_get_iter_at_line (CTK_TEXT_BUFFER (doc),
						  &iter,
						  line);
	}

	ctk_text_buffer_place_cursor (CTK_TEXT_BUFFER (doc), &iter);

	return ret;
}

gboolean
lapiz_document_goto_line_offset (LapizDocument *doc,
				 gint           line,
				 gint           line_offset)
{
	gboolean ret = TRUE;
	guint offset_count;
	CtkTextIter iter;

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (line >= -1, FALSE);
	g_return_val_if_fail (line_offset >= -1, FALSE);

	ctk_text_buffer_get_iter_at_line (CTK_TEXT_BUFFER (doc),
					  &iter,
					  line);

	offset_count = ctk_text_iter_get_chars_in_line (&iter);
	if (line_offset > offset_count)
	{
		ret = FALSE;
	}
	else
	{
		ctk_text_iter_set_line_offset (&iter, line_offset);
	}

	ctk_text_buffer_place_cursor (CTK_TEXT_BUFFER (doc), &iter);

	return ret;
}

static gint
compute_num_of_lines (const gchar *text)
{
	const gchar *p;
	gint len;
	gint n = 1;

	g_return_val_if_fail (text != NULL, 0);

	len = strlen (text);
	p = text;

	while (len > 0)
	{
		gint del, par;

		pango_find_paragraph_boundary (p, len, &del, &par);

		if (del == par) /* not found */
			break;

		p += par;
		len -= par;
		++n;
	}

	return n;
}

/**
 * lapiz_document_set_search_text:
 * @doc:
 * @text: (allow-none):
 * @flags:
 **/
void
lapiz_document_set_search_text (LapizDocument *doc,
				const gchar   *text,
				guint          flags)
{
	gchar *converted_text;
	gboolean notify = FALSE;
	gboolean update_to_search_region = FALSE;

	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));
	g_return_if_fail ((text == NULL) || (doc->priv->search_text != text));
	g_return_if_fail ((text == NULL) || g_utf8_validate (text, -1, NULL));

	lapiz_debug_message (DEBUG_DOCUMENT, "text = %s", text);

	if (text != NULL)
	{
		if (*text != '\0')
		{
			converted_text = lapiz_utils_unescape_search_text (text);
			notify = !lapiz_document_get_can_search_again (doc);
		}
		else
		{
			converted_text = g_strdup("");
			notify = lapiz_document_get_can_search_again (doc);
		}

		g_free (doc->priv->search_text);

		doc->priv->search_text = converted_text;
		doc->priv->num_of_lines_search_text = compute_num_of_lines (doc->priv->search_text);
		update_to_search_region = TRUE;
	}

	if (!LAPIZ_SEARCH_IS_DONT_SET_FLAGS (flags))
	{
		if (doc->priv->search_flags != flags)
			update_to_search_region = TRUE;

		doc->priv->search_flags = flags;

	}

	if (update_to_search_region)
	{
		CtkTextIter begin;
		CtkTextIter end;

		ctk_text_buffer_get_bounds (CTK_TEXT_BUFFER (doc),
					    &begin,
					    &end);

		to_search_region_range (doc,
					&begin,
					&end);
	}

	if (notify)
		g_object_notify (G_OBJECT (doc), "can-search-again");
}

/**
 * lapiz_document_get_search_text:
 * @doc:
 * @flags: (allow-none):
 */
gchar *
lapiz_document_get_search_text (LapizDocument *doc,
				guint         *flags)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);

	if (flags != NULL)
		*flags = doc->priv->search_flags;

	return lapiz_utils_escape_search_text (doc->priv->search_text);
}

/**
 * lapiz_document_set_last_replace_text:
 * @doc:
 * @text: (allow-none):
 **/
void
lapiz_document_set_last_replace_text (LapizDocument *doc,
				      const gchar   *text)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

	g_free(doc->priv->last_replace_text);

	lapiz_debug_message (DEBUG_SEARCH, "last_replace_text = %s", text == NULL ? "NULL" : text);
	doc->priv->last_replace_text = g_strdup(text);
}

/**
 * lapiz_document_get_last_replace_text:
 * @doc:
 */
gchar *
lapiz_document_get_last_replace_text (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);

	return doc->priv->last_replace_text;
}

gboolean
lapiz_document_get_can_search_again (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);

	return ((doc->priv->search_text != NULL) &&
	        (*doc->priv->search_text != '\0'));
}

/**
 * lapiz_document_search_forward:
 * @doc:
 * @start: (allow-none):
 * @end: (allow-none):
 * @match_start: (allow-none):
 * @match_end: (allow-none):
 **/
gboolean
lapiz_document_search_forward (LapizDocument     *doc,
			       const CtkTextIter *start,
			       const CtkTextIter *end,
			       CtkTextIter       *match_start,
			       CtkTextIter       *match_end)
{
	CtkTextIter iter;
	CtkTextSearchFlags search_flags;
	gboolean found = FALSE;
	CtkTextIter m_start;
	CtkTextIter m_end;

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail ((start == NULL) ||
			      (ctk_text_iter_get_buffer (start) ==  CTK_TEXT_BUFFER (doc)), FALSE);
	g_return_val_if_fail ((end == NULL) ||
			      (ctk_text_iter_get_buffer (end) ==  CTK_TEXT_BUFFER (doc)), FALSE);

	if (doc->priv->search_text == NULL)
	{
		lapiz_debug_message (DEBUG_DOCUMENT, "doc->priv->search_text == NULL\n");
		return FALSE;
	}
	else
		lapiz_debug_message (DEBUG_DOCUMENT, "doc->priv->search_text == \"%s\"\n", doc->priv->search_text);

	if (start == NULL)
		ctk_text_buffer_get_start_iter (CTK_TEXT_BUFFER (doc), &iter);
	else
		iter = *start;

	search_flags = CTK_TEXT_SEARCH_VISIBLE_ONLY | CTK_TEXT_SEARCH_TEXT_ONLY;

	if (!LAPIZ_SEARCH_IS_CASE_SENSITIVE (doc->priv->search_flags))
	{
		search_flags = search_flags | CTK_TEXT_SEARCH_CASE_INSENSITIVE;
	}

	while (!found)
	{
		if(!LAPIZ_SEARCH_IS_MATCH_REGEX(doc->priv->search_flags))
		{
			found = ctk_text_iter_forward_search (&iter,
							      doc->priv->search_text,
							      search_flags,
							      &m_start,
							      &m_end,
							      end);
		} else {
			found = lapiz_ctk_text_iter_regex_search (&iter,
								  doc->priv->search_text,
								  search_flags,
								  &m_start,
								  &m_end,
								  end,
								  TRUE,
								  &doc->priv->last_replace_text);
		}

		if (found && LAPIZ_SEARCH_IS_ENTIRE_WORD (doc->priv->search_flags))
		{
			found = ctk_text_iter_starts_word (&m_start) &&
					ctk_text_iter_ends_word (&m_end);

			if (!found)
				iter = m_end;
		}
		else
			break;
	}

	if (found && (match_start != NULL))
		*match_start = m_start;

	if (found && (match_end != NULL))
		*match_end = m_end;

	return found;
}

/**
 * lapiz_document_search_backward:
 * @doc:
 * @start: (allow-none):
 * @end: (allow-none):
 * @match_start: (allow-none):
 * @match_end: (allow-none):
 **/
gboolean
lapiz_document_search_backward (LapizDocument     *doc,
		const CtkTextIter *start,
		const CtkTextIter *end,
		CtkTextIter       *match_start,
		CtkTextIter       *match_end)
{
	CtkTextIter iter;
	CtkTextSearchFlags search_flags;
	gboolean found = FALSE;
	CtkTextIter m_start;
	CtkTextIter m_end;

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail ((start == NULL) ||
(			      ctk_text_iter_get_buffer (start) ==  CTK_TEXT_BUFFER (doc)), FALSE);
	g_return_val_if_fail ((end == NULL) ||
			      (ctk_text_iter_get_buffer (end) ==  CTK_TEXT_BUFFER (doc)), FALSE);

	if (doc->priv->search_text == NULL)
	{
		lapiz_debug_message (DEBUG_DOCUMENT, "doc->priv->search_text == NULL\n");
		return FALSE;
	}
	else
		lapiz_debug_message (DEBUG_DOCUMENT, "doc->priv->search_text == \"%s\"\n", doc->priv->search_text);

	if (end == NULL)
		ctk_text_buffer_get_end_iter (CTK_TEXT_BUFFER (doc), &iter);
	else
		iter = *end;

	search_flags = CTK_TEXT_SEARCH_VISIBLE_ONLY | CTK_TEXT_SEARCH_TEXT_ONLY;

	if (!LAPIZ_SEARCH_IS_CASE_SENSITIVE (doc->priv->search_flags))
	{
		search_flags = search_flags | CTK_TEXT_SEARCH_CASE_INSENSITIVE;
	}

	while (!found)
	{
		if(!LAPIZ_SEARCH_IS_MATCH_REGEX(doc->priv->search_flags))
		{
			found = ctk_text_iter_backward_search (&iter,
							       doc->priv->search_text,
							       search_flags,
							       &m_start,
							       &m_end,
							       start);
		}
		else
		{
			found = lapiz_ctk_text_iter_regex_search (&iter,
								  doc->priv->search_text,
								  search_flags,
								  &m_start,
								  &m_end,
								  start,
								  FALSE,
								  &doc->priv->last_replace_text);
		}

		if (found && LAPIZ_SEARCH_IS_ENTIRE_WORD (doc->priv->search_flags))
		{
			found = ctk_text_iter_starts_word (&m_start) &&
			ctk_text_iter_ends_word (&m_end);

			if (!found)
				iter = m_start;
		}
		else
			break;
	}

	if (found && (match_start != NULL))
		*match_start = m_start;

	if (found && (match_end != NULL))
		*match_end = m_end;

	return found;
}

/* FIXME this is an issue for introspection regardning @find */
gint
lapiz_document_replace_all (LapizDocument       *doc,
			    const gchar         *find,
			    const gchar         *replace,
			    guint                flags)
{
	CtkTextIter iter;
	CtkTextIter m_start;
	CtkTextIter m_end;
	CtkTextSearchFlags search_flags = 0;
	gboolean found = TRUE;
	gint cont = 0;
	gchar *search_text;
	gchar *replace_text = NULL;
	gint replace_text_len = 0;
	CtkTextBuffer *buffer;
	gboolean brackets_highlighting;
	gboolean search_highliting;

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), 0);
	g_return_val_if_fail (replace != NULL, 0);
	g_return_val_if_fail ((find != NULL) || (doc->priv->search_text != NULL), 0);

	buffer = CTK_TEXT_BUFFER (doc);

	if (find == NULL)
		search_text = g_strdup (doc->priv->search_text);
	else
		search_text = lapiz_utils_unescape_search_text (find);

	if(!LAPIZ_SEARCH_IS_MATCH_REGEX(flags))
	{
		replace_text = lapiz_utils_unescape_search_text (replace);
		replace_text_len = strlen (replace_text);
	}

	ctk_text_buffer_get_start_iter (buffer, &iter);

	search_flags = CTK_TEXT_SEARCH_VISIBLE_ONLY | CTK_TEXT_SEARCH_TEXT_ONLY;

	if (!LAPIZ_SEARCH_IS_CASE_SENSITIVE (flags))
	{
		search_flags = search_flags | CTK_TEXT_SEARCH_CASE_INSENSITIVE;
	}


	/* disable cursor_moved emission until the end of the
	 * replace_all so that we don't spend all the time
	 * updating the position in the statusbar
	 */
	doc->priv->stop_cursor_moved_emission = TRUE;

	/* also avoid spending time matching brackets */
	brackets_highlighting = ctk_source_buffer_get_highlight_matching_brackets (CTK_SOURCE_BUFFER (buffer));
	ctk_source_buffer_set_highlight_matching_brackets (CTK_SOURCE_BUFFER (buffer), FALSE);

	/* and do search highliting later */
	search_highliting = lapiz_document_get_enable_search_highlighting (doc);
	lapiz_document_set_enable_search_highlighting (doc, FALSE);

	ctk_text_buffer_begin_user_action (buffer);

	do
	{
		if(!LAPIZ_SEARCH_IS_MATCH_REGEX(flags))
		{
			found = ctk_text_iter_forward_search (&iter,
							      search_text,
							      search_flags,
							      &m_start,
							      &m_end,
							      NULL);
		} else {
			if(replace_text != NULL)
				g_free (replace_text);
			replace_text = g_strdup (replace);
			found = lapiz_ctk_text_iter_regex_search (&iter,
							          search_text,
							          search_flags,
							          &m_start,
							          &m_end,
							          NULL,
							          TRUE,
							          &replace_text);
			replace_text_len = strlen (replace_text);
		}

		if (found && LAPIZ_SEARCH_IS_ENTIRE_WORD (flags))
		{
			gboolean word;

			word = ctk_text_iter_starts_word (&m_start) &&
			       ctk_text_iter_ends_word (&m_end);

			if (!word)
			{
				iter = m_end;
				continue;
			}
		}

		if (found)
		{
			++cont;

			ctk_text_buffer_delete (buffer,
						&m_start,
						&m_end);
			ctk_text_buffer_insert (buffer,
						&m_start,
						replace_text,
						replace_text_len);

			iter = m_start;
        }

	} while (found);

	ctk_text_buffer_end_user_action (buffer);

	/* re-enable cursor_moved emission and notify
	 * the current position
	 */
	doc->priv->stop_cursor_moved_emission = FALSE;
	emit_cursor_moved (doc);

	ctk_source_buffer_set_highlight_matching_brackets (CTK_SOURCE_BUFFER (buffer),
							   brackets_highlighting);
	lapiz_document_set_enable_search_highlighting (doc, search_highliting);

	g_free (search_text);
	if(replace_text != NULL)
		g_free (replace_text);

	return cont;
}

/**
 * lapiz_document_set_language:
 * @doc:
 * @lang: (allow-none):
 **/
void
lapiz_document_set_language (LapizDocument     *doc,
			     CtkSourceLanguage *lang)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

	set_language (doc, lang, TRUE);
}

/**
 * lapiz_document_get_language:
 * @doc:
 *
 * Return value: (transfer none):
 */
CtkSourceLanguage *
lapiz_document_get_language (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);

	return ctk_source_buffer_get_language (CTK_SOURCE_BUFFER (doc));
}

const LapizEncoding *
lapiz_document_get_encoding (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);

	return doc->priv->encoding;
}

glong
_lapiz_document_get_seconds_since_last_save_or_load (LapizDocument *doc)
{
	GTimeVal current_time;

	lapiz_debug (DEBUG_DOCUMENT);

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), -1);

	g_get_current_time (&current_time);

	return (current_time.tv_sec - doc->priv->time_of_last_save_or_load.tv_sec);
}

static void
get_search_match_colors (LapizDocument *doc,
			 gboolean      *foreground_set,
			 CdkRGBA       *foreground,
			 gboolean      *background_set,
			 CdkRGBA       *background)
{
	CtkSourceStyleScheme *style_scheme;
	CtkSourceStyle *style;
	gchar *bg;
	gchar *fg;

	style_scheme = ctk_source_buffer_get_style_scheme (CTK_SOURCE_BUFFER (doc));
	if (style_scheme == NULL)
		goto fallback;

	style = ctk_source_style_scheme_get_style (style_scheme,
						   "search-match");
	if (style == NULL)
		goto fallback;

	g_object_get (style,
		      "foreground-set", foreground_set,
		      "foreground", &fg,
		      "background-set", background_set,
		      "background", &bg,
		      NULL);

	if (*foreground_set)
	{
		if (fg == NULL ||
		    !cdk_rgba_parse (foreground, fg))
		{
			*foreground_set = FALSE;
		}
	}

	if (*background_set)
	{
		if (bg == NULL ||
		    !cdk_rgba_parse (background, bg))
		{
			*background_set = FALSE;
		}
	}

	g_free (fg);
	g_free (bg);

	return;

 fallback:
	lapiz_debug_message (DEBUG_DOCUMENT,
			     "Falling back to hard-coded colors "
			     "for the \"found\" text tag.");

	cdk_rgba_parse (background, "#FFFF78");
	*background_set = TRUE;
	*foreground_set = FALSE;

	return;
}

static void
sync_found_tag (LapizDocument *doc,
		GParamSpec    *pspec G_GNUC_UNUSED,
		gpointer       data G_GNUC_UNUSED)
{
	CdkRGBA fg;
	CdkRGBA bg;
	gboolean fg_set;
	gboolean bg_set;

	lapiz_debug (DEBUG_DOCUMENT);

	g_return_if_fail (CTK_TEXT_TAG (doc->priv->found_tag));

	get_search_match_colors (doc,
				 &fg_set, &fg,
				 &bg_set, &bg);

	g_object_set (doc->priv->found_tag,
		      "foreground-rgba", fg_set ? &fg : NULL,
		      NULL);
	g_object_set (doc->priv->found_tag,
		      "background-rgba", bg_set ? &bg : NULL,
		      NULL);
}

static void
text_tag_set_highest_priority (CtkTextTag    *tag,
			       CtkTextBuffer *buffer)
{
	CtkTextTagTable *table;
	gint n;

	table = ctk_text_buffer_get_tag_table (buffer);
	n = ctk_text_tag_table_get_size (table);
	ctk_text_tag_set_priority (tag, n - 1);
}

static void
search_region (LapizDocument *doc,
	       CtkTextIter   *start,
	       CtkTextIter   *end)
{
	CtkTextIter iter;
	CtkTextIter m_start;
	CtkTextIter m_end;
	CtkTextSearchFlags search_flags = 0;
	gboolean found = TRUE;

	CtkTextBuffer *buffer;

	lapiz_debug (DEBUG_DOCUMENT);

	buffer = CTK_TEXT_BUFFER (doc);

	if (doc->priv->found_tag == NULL)
	{
		doc->priv->found_tag = ctk_text_buffer_create_tag (CTK_TEXT_BUFFER (doc),
								   "found",
								   NULL);

		sync_found_tag (doc, NULL, NULL);

		g_signal_connect (doc,
				  "notify::style-scheme",
				  G_CALLBACK (sync_found_tag),
				  NULL);
	}

	/* make sure the 'found' tag has the priority over
	 * syntax highlighting tags */
	text_tag_set_highest_priority (doc->priv->found_tag,
				       CTK_TEXT_BUFFER (doc));


	if (doc->priv->search_text == NULL)
		return;

	g_return_if_fail (doc->priv->num_of_lines_search_text > 0);

	ctk_text_iter_backward_lines (start, doc->priv->num_of_lines_search_text);
	ctk_text_iter_forward_lines (end, doc->priv->num_of_lines_search_text);

	if (ctk_text_iter_has_tag (start, doc->priv->found_tag) &&
	    !ctk_text_iter_starts_tag (start, doc->priv->found_tag))
		ctk_text_iter_backward_to_tag_toggle (start, doc->priv->found_tag);

	if (ctk_text_iter_has_tag (end, doc->priv->found_tag) &&
	    !ctk_text_iter_ends_tag (end, doc->priv->found_tag))
		ctk_text_iter_forward_to_tag_toggle (end, doc->priv->found_tag);

	/*
	g_print ("[%u (%u), %u (%u)]\n", ctk_text_iter_get_line (start), ctk_text_iter_get_offset (start),
					   ctk_text_iter_get_line (end), ctk_text_iter_get_offset (end));
	*/

	ctk_text_buffer_remove_tag (buffer,
				    doc->priv->found_tag,
				    start,
				    end);

	if (*doc->priv->search_text == '\0')
		return;

	iter = *start;

	search_flags = CTK_TEXT_SEARCH_VISIBLE_ONLY | CTK_TEXT_SEARCH_TEXT_ONLY;

	if (!LAPIZ_SEARCH_IS_CASE_SENSITIVE (doc->priv->search_flags))
	{
		search_flags = search_flags | CTK_TEXT_SEARCH_CASE_INSENSITIVE;
	}

	do
	{
		if ((end != NULL) && ctk_text_iter_is_end (end))
			end = NULL;

		found = ctk_text_iter_forward_search (&iter,
							doc->priv->search_text,
							search_flags,
                        	                	&m_start,
                        	                	&m_end,
                                	               	end);

		iter = m_end;

		if (found && LAPIZ_SEARCH_IS_ENTIRE_WORD (doc->priv->search_flags))
		{
			gboolean word;

			word = ctk_text_iter_starts_word (&m_start) &&
			       ctk_text_iter_ends_word (&m_end);

			if (!word)
				continue;
		}

		if (found)
		{
			ctk_text_buffer_apply_tag (buffer,
						   doc->priv->found_tag,
						   &m_start,
						   &m_end);
		}

	} while (found);
}

static void
to_search_region_range (LapizDocument *doc,
			CtkTextIter   *start,
			CtkTextIter   *end)
{
	lapiz_debug (DEBUG_DOCUMENT);

	if (doc->priv->to_search_region == NULL)
		return;

	ctk_text_iter_set_line_offset (start, 0);
	ctk_text_iter_forward_to_line_end (end);

	/*
	g_print ("+ [%u (%u), %u (%u)]\n", ctk_text_iter_get_line (start), ctk_text_iter_get_offset (start),
					   ctk_text_iter_get_line (end), ctk_text_iter_get_offset (end));
	*/

	/* Add the region to the refresh region */
	lapiz_text_region_add (doc->priv->to_search_region, start, end);

	/* Notify views of the updated highlight region */
	ctk_text_iter_backward_lines (start, doc->priv->num_of_lines_search_text);
	ctk_text_iter_forward_lines (end, doc->priv->num_of_lines_search_text);

	g_signal_emit (doc, document_signals [SEARCH_HIGHLIGHT_UPDATED], 0, start, end);
}

void
_lapiz_document_search_region (LapizDocument     *doc,
			       const CtkTextIter *start,
			       const CtkTextIter *end)
{
	LapizTextRegion *region;

	lapiz_debug (DEBUG_DOCUMENT);

	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);

	if (doc->priv->to_search_region == NULL)
		return;

	/*
	g_print ("U [%u (%u), %u (%u)]\n", ctk_text_iter_get_line (start), ctk_text_iter_get_offset (start),
					   ctk_text_iter_get_line (end), ctk_text_iter_get_offset (end));
	*/

	/* get the subregions not yet highlighted */
	region = lapiz_text_region_intersect (doc->priv->to_search_region,
					      start,
					      end);
	if (region)
	{
		gint i;
		CtkTextIter start_search;
		CtkTextIter end_search;

		i = lapiz_text_region_subregions (region);
		lapiz_text_region_nth_subregion (region,
						 0,
						 &start_search,
						 NULL);

		lapiz_text_region_nth_subregion (region,
						 i - 1,
						 NULL,
						 &end_search);

		lapiz_text_region_destroy (region, TRUE);

		ctk_text_iter_order (&start_search, &end_search);

		search_region (doc, &start_search, &end_search);

		/* remove the just highlighted region */
		lapiz_text_region_subtract (doc->priv->to_search_region,
					    start,
					    end);
	}
}

static void
insert_text_cb (LapizDocument *doc,
		CtkTextIter   *pos,
		const gchar   *text,
		gint           length)
{
	CtkTextIter start;
	CtkTextIter end;

	lapiz_debug (DEBUG_DOCUMENT);

	start = end = *pos;

	/*
	 * pos is invalidated when
	 * insertion occurs (because the buffer contents change), but the
	 * default signal handler revalidates it to point to the end of the
	 * inserted text
	 */
	ctk_text_iter_backward_chars (&start,
				      g_utf8_strlen (text, length));

	to_search_region_range (doc, &start, &end);
}

static void
delete_range_cb (LapizDocument *doc,
		 CtkTextIter   *start,
		 CtkTextIter   *end)
{
	CtkTextIter d_start;
	CtkTextIter d_end;

	lapiz_debug (DEBUG_DOCUMENT);

	d_start = *start;
	d_end = *end;

	to_search_region_range (doc, &d_start, &d_end);
}

void
lapiz_document_set_enable_search_highlighting (LapizDocument *doc,
					       gboolean       enable)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

	enable = enable != FALSE;

	if ((doc->priv->to_search_region != NULL) == enable)
		return;

	if (doc->priv->to_search_region != NULL)
	{
		/* Disable search highlighting */
		if (doc->priv->found_tag != NULL)
		{
			/* If needed remove the found_tag */
			CtkTextIter begin;
			CtkTextIter end;

			ctk_text_buffer_get_bounds (CTK_TEXT_BUFFER (doc),
						    &begin,
						    &end);

			ctk_text_buffer_remove_tag (CTK_TEXT_BUFFER (doc),
				    		    doc->priv->found_tag,
				    		    &begin,
				    		    &end);
		}

		lapiz_text_region_destroy (doc->priv->to_search_region,
					   TRUE);
		doc->priv->to_search_region = NULL;
	}
	else
	{
		doc->priv->to_search_region = lapiz_text_region_new (CTK_TEXT_BUFFER (doc));
		if (lapiz_document_get_can_search_again (doc))
		{
			/* If search_text is not empty, highligth all its occurrences */
			CtkTextIter begin;
			CtkTextIter end;

			ctk_text_buffer_get_bounds (CTK_TEXT_BUFFER (doc),
						    &begin,
						    &end);

			to_search_region_range (doc,
						&begin,
						&end);
		}
	}
}

gboolean
lapiz_document_get_enable_search_highlighting (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), FALSE);

	return (doc->priv->to_search_region != NULL);
}

void
lapiz_document_set_newline_type (LapizDocument           *doc,
				 LapizDocumentNewlineType newline_type)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

	if (doc->priv->newline_type != newline_type)
	{
		doc->priv->newline_type = newline_type;

		g_object_notify (G_OBJECT (doc), "newline-type");
	}
}

LapizDocumentNewlineType
lapiz_document_get_newline_type (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), 0);

	return doc->priv->newline_type;
}

void
_lapiz_document_set_mount_operation_factory (LapizDocument 	       *doc,
					    LapizMountOperationFactory	callback,
					    gpointer	                userdata)
{
	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));

	doc->priv->mount_operation_factory = callback;
	doc->priv->mount_operation_userdata = userdata;
}

GMountOperation *
_lapiz_document_create_mount_operation (LapizDocument *doc)
{
	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);

	if (doc->priv->mount_operation_factory == NULL)
		return g_mount_operation_new ();
	else
		return doc->priv->mount_operation_factory (doc,
						           doc->priv->mount_operation_userdata);
}

#ifndef ENABLE_GVFS_METADATA
gchar *
lapiz_document_get_metadata (LapizDocument *doc,
			     const gchar   *key)
{
	gchar *value = NULL;

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	if (!lapiz_document_is_untitled (doc))
	{
		value = lapiz_metadata_manager_get (doc->priv->uri, key);
	}

	return value;
}

void
lapiz_document_set_metadata (LapizDocument *doc,
			     const gchar   *first_key,
			     ...)
{
	const gchar *key;
	const gchar *value;
	va_list var_args;

	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));
	g_return_if_fail (first_key != NULL);

	if (lapiz_document_is_untitled (doc))
	{
		/* Can't set metadata for untitled documents */
		return;
	}

	va_start (var_args, first_key);

	for (key = first_key; key; key = va_arg (var_args, const gchar *))
	{
		value = va_arg (var_args, const gchar *);

		lapiz_metadata_manager_set (doc->priv->uri,
					    key,
					    value);
	}

	va_end (var_args);
}

#else

/**
 * lapiz_document_get_metadata:
 * @doc: a #LapizDocument
 * @key: name of the key
 *
 * Gets the metadata assigned to @key.
 *
 * Returns: the value assigned to @key.
 */
gchar *
lapiz_document_get_metadata (LapizDocument *doc,
			     const gchar   *key)
{
	gchar *value = NULL;

	g_return_val_if_fail (LAPIZ_IS_DOCUMENT (doc), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	if (doc->priv->metadata_info && g_file_info_has_attribute (doc->priv->metadata_info,
								   key))
	{
		value = g_strdup (g_file_info_get_attribute_string (doc->priv->metadata_info,
								    key));
	}

	return value;
}

static void
set_attributes_cb (GObject      *source,
		   GAsyncResult *res,
		   gpointer      useless G_GNUC_UNUSED)
{
	g_file_set_attributes_finish (G_FILE (source),
				      res,
				      NULL,
				      NULL);
}

/**
 * lapiz_document_set_metadata:
 * @doc: a #LapizDocument
 * @first_key: name of the first key to set
 * @...: value for the first key, followed optionally by more key/value pairs,
 * followed by %NULL.
 *
 * Sets metadata on a document.
 */
void
lapiz_document_set_metadata (LapizDocument *doc,
			     const gchar   *first_key,
			     ...)
{
	const gchar *key;
	const gchar *value;
	va_list var_args;
	GFileInfo *info;
	GFile *location;

	g_return_if_fail (LAPIZ_IS_DOCUMENT (doc));
	g_return_if_fail (first_key != NULL);

	info = g_file_info_new ();

	va_start (var_args, first_key);

	for (key = first_key; key; key = va_arg (var_args, const gchar *))
	{
		value = va_arg (var_args, const gchar *);

		if (value != NULL)
		{
			g_file_info_set_attribute_string (info,
							  key, value);
		}
		else
		{
			/* Unset the key */
			g_file_info_set_attribute (info, key,
						   G_FILE_ATTRIBUTE_TYPE_INVALID,
						   NULL);
		}
	}

	va_end (var_args);

	if (doc->priv->metadata_info != NULL)
		g_file_info_copy_into (info, doc->priv->metadata_info);

	location = lapiz_document_get_location (doc);

	if (location != NULL)
	{
		g_file_set_attributes_async (location,
					     info,
					     G_FILE_QUERY_INFO_NONE,
					     G_PRIORITY_DEFAULT,
					     NULL,
					     set_attributes_cb,
					     NULL);

		g_object_unref (location);
	}

	g_object_unref (info);
}
#endif
