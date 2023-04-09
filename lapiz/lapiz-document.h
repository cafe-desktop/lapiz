/*
 * lapiz-document.h
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

#ifndef __LAPIZ_DOCUMENT_H__
#define __LAPIZ_DOCUMENT_H__

#include <gio/gio.h>
#include <ctk/ctk.h>
#include <ctksourceview/ctksource.h>

#include <lapiz/lapiz-encodings.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_DOCUMENT              (lapiz_document_get_type())
#define LAPIZ_DOCUMENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_DOCUMENT, LapizDocument))
#define LAPIZ_DOCUMENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_DOCUMENT, LapizDocumentClass))
#define LAPIZ_IS_DOCUMENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_DOCUMENT))
#define LAPIZ_IS_DOCUMENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_DOCUMENT))
#define LAPIZ_DOCUMENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_DOCUMENT, LapizDocumentClass))

#define LAPIZ_METADATA_ATTRIBUTE_POSITION "metadata::lapiz-position"
#define LAPIZ_METADATA_ATTRIBUTE_ENCODING "metadata::lapiz-encoding"
#define LAPIZ_METADATA_ATTRIBUTE_LANGUAGE "metadata::lapiz-language"

typedef enum
{
	LAPIZ_DOCUMENT_NEWLINE_TYPE_LF,
	LAPIZ_DOCUMENT_NEWLINE_TYPE_CR,
	LAPIZ_DOCUMENT_NEWLINE_TYPE_CR_LF
} LapizDocumentNewlineType;

#define LAPIZ_DOCUMENT_NEWLINE_TYPE_DEFAULT LAPIZ_DOCUMENT_NEWLINE_TYPE_LF

typedef enum
{
	LAPIZ_SEARCH_DONT_SET_FLAGS	= 1 << 0,
	LAPIZ_SEARCH_ENTIRE_WORD	= 1 << 1,
	LAPIZ_SEARCH_CASE_SENSITIVE	= 1 << 2,
	LAPIZ_SEARCH_PARSE_ESCAPES	= 1 << 3,
	LAPIZ_SEARCH_MATCH_REGEX	= 1 << 4,

} LapizSearchFlags;

/**
 * LapizDocumentSaveFlags:
 * @LAPIZ_DOCUMENT_SAVE_IGNORE_MTIME: save file despite external modifications.
 * @LAPIZ_DOCUMENT_SAVE_IGNORE_BACKUP: write the file directly without attempting to backup.
 * @LAPIZ_DOCUMENT_SAVE_PRESERVE_BACKUP: preserve previous backup file, needed to support autosaving.
 */
typedef enum
{
	LAPIZ_DOCUMENT_SAVE_IGNORE_MTIME 	= 1 << 0,
	LAPIZ_DOCUMENT_SAVE_IGNORE_BACKUP	= 1 << 1,
	LAPIZ_DOCUMENT_SAVE_PRESERVE_BACKUP	= 1 << 2
} LapizDocumentSaveFlags;

/* Private structure type */
typedef struct _LapizDocumentPrivate    LapizDocumentPrivate;

/*
 * Main object structure
 */
typedef struct _LapizDocument           LapizDocument;

struct _LapizDocument
{
	GtkSourceBuffer buffer;

	/*< private > */
	LapizDocumentPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizDocumentClass 	LapizDocumentClass;

struct _LapizDocumentClass
{
	GtkSourceBufferClass parent_class;

	/* Signals */ // CHECK: ancora da rivedere

	void (* cursor_moved)		(LapizDocument    *document);

	/* Document load */
	void (* load)			(LapizDocument       *document,
					 const gchar         *uri,
					 const LapizEncoding *encoding,
					 gint                 line_pos,
					 gboolean             create);

	void (* loading)		(LapizDocument    *document,
					 goffset	   size,
					 goffset	   total_size);

	void (* loaded)			(LapizDocument    *document,
					 const GError     *error);

	/* Document save */
	void (* save)			(LapizDocument          *document,
					 const gchar            *uri,
					 const LapizEncoding    *encoding,
					 LapizDocumentSaveFlags  flags);

	void (* saving)			(LapizDocument    *document,
					 goffset	   size,
					 goffset	   total_size);

	void (* saved)  		(LapizDocument    *document,
					 const GError     *error);

	void (* search_highlight_updated)
					(LapizDocument    *document,
					 GtkTextIter      *start,
					 GtkTextIter      *end);
};


#define LAPIZ_DOCUMENT_ERROR lapiz_document_error_quark ()

enum
{
	LAPIZ_DOCUMENT_ERROR_EXTERNALLY_MODIFIED,
	LAPIZ_DOCUMENT_ERROR_CANT_CREATE_BACKUP,
	LAPIZ_DOCUMENT_ERROR_TOO_BIG,
	LAPIZ_DOCUMENT_ERROR_ENCODING_AUTO_DETECTION_FAILED,
	LAPIZ_DOCUMENT_ERROR_CONVERSION_FALLBACK,
	LAPIZ_DOCUMENT_NUM_ERRORS
};

GQuark		 lapiz_document_error_quark	(void);

GType		 lapiz_document_get_type      	(void) G_GNUC_CONST;

LapizDocument   *lapiz_document_new 		(void);

GFile		*lapiz_document_get_location	(LapizDocument       *doc);

gchar		*lapiz_document_get_uri 	(LapizDocument       *doc);
void		 lapiz_document_set_uri		(LapizDocument       *doc,
						 const gchar 	     *uri);

gchar		*lapiz_document_get_uri_for_display
						(LapizDocument       *doc);
gchar		*lapiz_document_get_short_name_for_display
					 	(LapizDocument       *doc);

void		 lapiz_document_set_short_name_for_display
						(LapizDocument       *doc,
						 const gchar         *name);

gchar		*lapiz_document_get_content_type
					 	(LapizDocument       *doc);

void		 lapiz_document_set_content_type
					 	(LapizDocument       *doc,
					 	 const gchar         *content_type);

gchar		*lapiz_document_get_mime_type 	(LapizDocument       *doc);

gboolean	 lapiz_document_get_readonly 	(LapizDocument       *doc);

void		 lapiz_document_load 		(LapizDocument       *doc,
						 const gchar         *uri,
						 const LapizEncoding *encoding,
						 gint                 line_pos,
						 gboolean             create);

gboolean	 lapiz_document_insert_file	(LapizDocument       *doc,
						 GtkTextIter         *iter,
						 const gchar         *uri,
						 const LapizEncoding *encoding);

gboolean	 lapiz_document_load_cancel	(LapizDocument       *doc);

void		 lapiz_document_save 		(LapizDocument       *doc,
						 LapizDocumentSaveFlags flags);

void		 lapiz_document_save_as 	(LapizDocument       *doc,
						 const gchar         *uri,
						 const LapizEncoding *encoding,
						 LapizDocumentSaveFlags flags);

gboolean	 lapiz_document_is_untouched 	(LapizDocument       *doc);
gboolean	 lapiz_document_is_untitled 	(LapizDocument       *doc);

gboolean	 lapiz_document_is_local	(LapizDocument       *doc);

gboolean	 lapiz_document_get_deleted	(LapizDocument       *doc);

gboolean	 lapiz_document_goto_line 	(LapizDocument       *doc,
						 gint                 line);

gboolean	 lapiz_document_goto_line_offset(LapizDocument *doc,
						 gint           line,
						 gint           line_offset);

void		 lapiz_document_set_search_text	(LapizDocument       *doc,
						 const gchar         *text,
						 guint                flags);

gchar		*lapiz_document_get_search_text	(LapizDocument       *doc,
						 guint               *flags);

gchar		*lapiz_document_get_last_replace_text
						(LapizDocument       *doc);

void		 lapiz_document_set_last_replace_text
						(LapizDocument       *doc,
						 const gchar         *text);

gboolean	 lapiz_document_get_can_search_again
						(LapizDocument       *doc);

gboolean	 lapiz_document_search_forward	(LapizDocument       *doc,
						 const GtkTextIter   *start,
						 const GtkTextIter   *end,
						 GtkTextIter         *match_start,
						 GtkTextIter         *match_end);

gboolean	 lapiz_document_search_backward	(LapizDocument       *doc,
						 const GtkTextIter   *start,
						 const GtkTextIter   *end,
						 GtkTextIter         *match_start,
						 GtkTextIter         *match_end);

gint		 lapiz_document_replace_all 	(LapizDocument       *doc,
				            	 const gchar         *find,
						 const gchar         *replace,
					    	 guint                flags);

void 		 lapiz_document_set_language 	(LapizDocument       *doc,
						 GtkSourceLanguage   *lang);
GtkSourceLanguage
		*lapiz_document_get_language 	(LapizDocument       *doc);

const LapizEncoding
		*lapiz_document_get_encoding	(LapizDocument       *doc);

void		 lapiz_document_set_enable_search_highlighting
						(LapizDocument       *doc,
						 gboolean             enable);

gboolean	 lapiz_document_get_enable_search_highlighting
						(LapizDocument       *doc);

void		 lapiz_document_set_newline_type (LapizDocument           *doc,
						  LapizDocumentNewlineType newline_type);

LapizDocumentNewlineType
		 lapiz_document_get_newline_type (LapizDocument *doc);

gchar		*lapiz_document_get_metadata	(LapizDocument *doc,
						 const gchar   *key);

void		 lapiz_document_set_metadata	(LapizDocument *doc,
						 const gchar   *first_key,
						 ...);

/*
 * Non exported functions
 */
void		 _lapiz_document_set_readonly 	(LapizDocument       *doc,
						 gboolean             readonly);

glong		 _lapiz_document_get_seconds_since_last_save_or_load
						(LapizDocument       *doc);

/* Note: this is a sync stat: use only on local files */
gboolean	_lapiz_document_check_externally_modified
						(LapizDocument       *doc);

void		_lapiz_document_search_region   (LapizDocument       *doc,
						 const GtkTextIter   *start,
						 const GtkTextIter   *end);

/* Search macros */
#define LAPIZ_SEARCH_IS_DONT_SET_FLAGS(sflags) ((sflags & LAPIZ_SEARCH_DONT_SET_FLAGS) != 0)
#define LAPIZ_SEARCH_SET_DONT_SET_FLAGS(sflags,state) ((state == TRUE) ? \
(sflags |= LAPIZ_SEARCH_DONT_SET_FLAGS) : (sflags &= ~LAPIZ_SEARCH_DONT_SET_FLAGS))

#define LAPIZ_SEARCH_IS_ENTIRE_WORD(sflags) ((sflags & LAPIZ_SEARCH_ENTIRE_WORD) != 0)
#define LAPIZ_SEARCH_SET_ENTIRE_WORD(sflags,state) ((state == TRUE) ? \
(sflags |= LAPIZ_SEARCH_ENTIRE_WORD) : (sflags &= ~LAPIZ_SEARCH_ENTIRE_WORD))

#define LAPIZ_SEARCH_IS_CASE_SENSITIVE(sflags) ((sflags &  LAPIZ_SEARCH_CASE_SENSITIVE) != 0)
#define LAPIZ_SEARCH_SET_CASE_SENSITIVE(sflags,state) ((state == TRUE) ? \
(sflags |= LAPIZ_SEARCH_CASE_SENSITIVE) : (sflags &= ~LAPIZ_SEARCH_CASE_SENSITIVE))

#define LAPIZ_SEARCH_IS_PARSE_ESCAPES(sflags) ((sflags &  LAPIZ_SEARCH_PARSE_ESCAPES) != 0)
#define LAPIZ_SEARCH_SET_PARSE_ESCAPES(sflags,state) ((state == TRUE) ? \
(sflags |= LAPIZ_SEARCH_PARSE_ESCAPES) : (sflags &= ~LAPIZ_SEARCH_PARSE_ESCAPES))

#define LAPIZ_SEARCH_IS_MATCH_REGEX(sflags) ((sflags &  LAPIZ_SEARCH_MATCH_REGEX) != 0)
#define LAPIZ_SEARCH_SET_MATCH_REGEX(sflags,state) ((state == TRUE) ? \
(sflags |= LAPIZ_SEARCH_MATCH_REGEX) : (sflags &= ~LAPIZ_SEARCH_MATCH_REGEX))


typedef GMountOperation *(*LapizMountOperationFactory)(LapizDocument *doc,
						       gpointer       userdata);

void		 _lapiz_document_set_mount_operation_factory
						(LapizDocument	            *doc,
						 LapizMountOperationFactory  callback,
						 gpointer	             userdata);
GMountOperation
		*_lapiz_document_create_mount_operation
						(LapizDocument	     *doc);

G_END_DECLS

#endif /* __LAPIZ_DOCUMENT_H__ */
