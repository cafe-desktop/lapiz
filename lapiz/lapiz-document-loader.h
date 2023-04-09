/*
 * lapiz-document-loader.h
 * This file is part of lapiz
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2007 - Paolo Maggi, Steve Frécinaux
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
 * Modified by the lapiz Team, 2005-2007. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __LAPIZ_DOCUMENT_LOADER_H__
#define __LAPIZ_DOCUMENT_LOADER_H__

#include <lapiz/lapiz-document.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_DOCUMENT_LOADER              (lapiz_document_loader_get_type())
#define LAPIZ_DOCUMENT_LOADER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_DOCUMENT_LOADER, LapizDocumentLoader))
#define LAPIZ_DOCUMENT_LOADER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_DOCUMENT_LOADER, LapizDocumentLoaderClass))
#define LAPIZ_IS_DOCUMENT_LOADER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_DOCUMENT_LOADER))
#define LAPIZ_IS_DOCUMENT_LOADER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_DOCUMENT_LOADER))
#define LAPIZ_DOCUMENT_LOADER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_DOCUMENT_LOADER, LapizDocumentLoaderClass))

/* Private structure type */
typedef struct _LapizDocumentLoaderPrivate LapizDocumentLoaderPrivate;

/*
 * Main object structure
 */
typedef struct _LapizDocumentLoader LapizDocumentLoader;

struct _LapizDocumentLoader
{
	GObject object;

	LapizDocument		 *document;
	gboolean		  used;

	/* Info on the current file */
	GFileInfo		 *info;
	gchar			 *uri;
	const LapizEncoding	 *encoding;
	const LapizEncoding	 *auto_detected_encoding;
	LapizDocumentNewlineType  auto_detected_newline_type;
};

/*
 * Class definition
 */
typedef struct _LapizDocumentLoaderClass LapizDocumentLoaderClass;

struct _LapizDocumentLoaderClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* loading) (LapizDocumentLoader *loader,
			  gboolean             completed,
			  const GError        *error);

	/* VTable */
	void			(* load)		(LapizDocumentLoader *loader);
	gboolean		(* cancel)		(LapizDocumentLoader *loader);
	goffset			(* get_bytes_read)	(LapizDocumentLoader *loader);
};

/*
 * Public methods
 */
GType 		 	 lapiz_document_loader_get_type		(void) G_GNUC_CONST;

/* If enconding == NULL, the encoding will be autodetected */
LapizDocumentLoader 	*lapiz_document_loader_new 		(LapizDocument       *doc,
								 const gchar         *uri,
								 const LapizEncoding *encoding);

void			 lapiz_document_loader_loading		(LapizDocumentLoader *loader,
								 gboolean             completed,
								 GError              *error);

void			 lapiz_document_loader_load		(LapizDocumentLoader *loader);
#if 0
gboolean		 lapiz_document_loader_load_from_stdin	(LapizDocumentLoader *loader);
#endif
gboolean		 lapiz_document_loader_cancel		(LapizDocumentLoader *loader);

LapizDocument		*lapiz_document_loader_get_document	(LapizDocumentLoader *loader);

/* Returns STDIN_URI if loading from stdin */
#define STDIN_URI "stdin:"
const gchar		*lapiz_document_loader_get_uri		(LapizDocumentLoader *loader);

const LapizEncoding	*lapiz_document_loader_get_encoding	(LapizDocumentLoader *loader);

LapizDocumentNewlineType lapiz_document_loader_get_newline_type (LapizDocumentLoader *loader);

goffset			 lapiz_document_loader_get_bytes_read	(LapizDocumentLoader *loader);

/* You can get from the info: content_type, time_modified, standard_size, access_can_write
   and also the metadata*/
GFileInfo		*lapiz_document_loader_get_info		(LapizDocumentLoader *loader);

G_END_DECLS

#endif  /* __LAPIZ_DOCUMENT_LOADER_H__  */
