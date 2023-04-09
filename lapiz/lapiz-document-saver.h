/*
 * lapiz-document-saver.h
 * This file is part of lapiz
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyrhing (C) 2007 - Paolo Maggi, Steve Frécinaux
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
 * Modified by the lapiz Team, 2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __LAPIZ_DOCUMENT_SAVER_H__
#define __LAPIZ_DOCUMENT_SAVER_H__

#include <lapiz/lapiz-document.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_DOCUMENT_SAVER              (lapiz_document_saver_get_type())
#define LAPIZ_DOCUMENT_SAVER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_DOCUMENT_SAVER, PlumaDocumentSaver))
#define LAPIZ_DOCUMENT_SAVER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_DOCUMENT_SAVER, PlumaDocumentSaverClass))
#define LAPIZ_IS_DOCUMENT_SAVER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_DOCUMENT_SAVER))
#define LAPIZ_IS_DOCUMENT_SAVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_DOCUMENT_SAVER))
#define LAPIZ_DOCUMENT_SAVER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_DOCUMENT_SAVER, PlumaDocumentSaverClass))

/*
 * Main object structure
 */
typedef struct _PlumaDocumentSaver PlumaDocumentSaver;

struct _PlumaDocumentSaver
{
	GObject object;

	/*< private >*/
	GFileInfo		 *info;
	PlumaDocument		 *document;
	gboolean		  used;

	gchar			 *uri;
	const PlumaEncoding      *encoding;
	PlumaDocumentNewlineType  newline_type;

	PlumaDocumentSaveFlags    flags;

	gboolean		  keep_backup;
};

/*
 * Class definition
 */
typedef struct _PlumaDocumentSaverClass PlumaDocumentSaverClass;

struct _PlumaDocumentSaverClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* saving) (PlumaDocumentSaver *saver,
			 gboolean             completed,
			 const GError        *error);

	/* VTable */
	void			(* save)		(PlumaDocumentSaver *saver,
							 GTimeVal           *old_mtime);
	goffset			(* get_file_size)	(PlumaDocumentSaver *saver);
	goffset			(* get_bytes_written)	(PlumaDocumentSaver *saver);
};

/*
 * Public methods
 */
GType 		 	 lapiz_document_saver_get_type		(void) G_GNUC_CONST;

/* If enconding == NULL, the encoding will be autodetected */
PlumaDocumentSaver 	*lapiz_document_saver_new 		(PlumaDocument           *doc,
								 const gchar             *uri,
								 const PlumaEncoding     *encoding,
								 PlumaDocumentNewlineType newline_type,
								 PlumaDocumentSaveFlags   flags);

void			 lapiz_document_saver_saving		(PlumaDocumentSaver *saver,
								 gboolean            completed,
								 GError             *error);
void			 lapiz_document_saver_save		(PlumaDocumentSaver  *saver,
								 GTimeVal            *old_mtime);

#if 0
void			 lapiz_document_saver_cancel		(PlumaDocumentSaver  *saver);
#endif

PlumaDocument		*lapiz_document_saver_get_document	(PlumaDocumentSaver  *saver);

const gchar		*lapiz_document_saver_get_uri		(PlumaDocumentSaver  *saver);

/* If backup_uri is NULL no backup will be made */
const gchar		*lapiz_document_saver_get_backup_uri	(PlumaDocumentSaver  *saver);
void			*lapiz_document_saver_set_backup_uri	(PlumaDocumentSaver  *saver,
							 	 const gchar         *backup_uri);

/* Returns 0 if file size is unknown */
goffset			 lapiz_document_saver_get_file_size	(PlumaDocumentSaver  *saver);

goffset			 lapiz_document_saver_get_bytes_written	(PlumaDocumentSaver  *saver);

GFileInfo		*lapiz_document_saver_get_info		(PlumaDocumentSaver  *saver);

G_END_DECLS

#endif  /* __LAPIZ_DOCUMENT_SAVER_H__  */
