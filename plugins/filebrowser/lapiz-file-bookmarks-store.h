/*
 * lapiz-file-bookmarks-store.h - Lapiz plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __LAPIZ_FILE_BOOKMARKS_STORE_H__
#define __LAPIZ_FILE_BOOKMARKS_STORE_H__

#include <ctk/ctk.h>

G_BEGIN_DECLS
#define LAPIZ_TYPE_FILE_BOOKMARKS_STORE			(lapiz_file_bookmarks_store_get_type ())
#define LAPIZ_FILE_BOOKMARKS_STORE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BOOKMARKS_STORE, LapizFileBookmarksStore))
#define LAPIZ_FILE_BOOKMARKS_STORE_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BOOKMARKS_STORE, LapizFileBookmarksStore const))
#define LAPIZ_FILE_BOOKMARKS_STORE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_FILE_BOOKMARKS_STORE, LapizFileBookmarksStoreClass))
#define LAPIZ_IS_FILE_BOOKMARKS_STORE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_FILE_BOOKMARKS_STORE))
#define LAPIZ_IS_FILE_BOOKMARKS_STORE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_FILE_BOOKMARKS_STORE))
#define LAPIZ_FILE_BOOKMARKS_STORE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_FILE_BOOKMARKS_STORE, LapizFileBookmarksStoreClass))

typedef struct _LapizFileBookmarksStore        LapizFileBookmarksStore;
typedef struct _LapizFileBookmarksStoreClass   LapizFileBookmarksStoreClass;
typedef struct _LapizFileBookmarksStorePrivate LapizFileBookmarksStorePrivate;

enum
{
	LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_ICON = 0,
	LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_NAME,
	LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_OBJECT,
	LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS,
	LAPIZ_FILE_BOOKMARKS_STORE_N_COLUMNS
};

enum
{
	LAPIZ_FILE_BOOKMARKS_STORE_NONE            	= 0,
	LAPIZ_FILE_BOOKMARKS_STORE_IS_SEPARATOR   	= 1 << 0,  /* Separator item */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_SPECIAL_DIR 	= 1 << 1,  /* Special user dir */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_HOME         	= 1 << 2,  /* The special Home user directory */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_DESKTOP      	= 1 << 3,  /* The special Desktop user directory */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_DOCUMENTS    	= 1 << 4,  /* The special Documents user directory */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_FS        	= 1 << 5,  /* A mount object */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_MOUNT        	= 1 << 6,  /* A mount object */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_VOLUME        	= 1 << 7,  /* A volume object */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_DRIVE        	= 1 << 8,  /* A drive object */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_ROOT         	= 1 << 9,  /* The root file system (file:///) */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_BOOKMARK     	= 1 << 10,  /* A ctk bookmark */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_REMOTE_BOOKMARK	= 1 << 11, /* A remote ctk bookmark */
	LAPIZ_FILE_BOOKMARKS_STORE_IS_LOCAL_BOOKMARK	= 1 << 12  /* A local ctk bookmark */
};

struct _LapizFileBookmarksStore
{
	GtkTreeStore parent;

	LapizFileBookmarksStorePrivate *priv;
};

struct _LapizFileBookmarksStoreClass
{
	GtkTreeStoreClass parent_class;
};

GType lapiz_file_bookmarks_store_get_type               (void) G_GNUC_CONST;
void _lapiz_file_bookmarks_store_register_type          (GTypeModule * module);

LapizFileBookmarksStore *lapiz_file_bookmarks_store_new (void);
gchar *lapiz_file_bookmarks_store_get_uri               (LapizFileBookmarksStore * model,
					                 GtkTreeIter * iter);
void lapiz_file_bookmarks_store_refresh                 (LapizFileBookmarksStore * model);

G_END_DECLS
#endif				/* __LAPIZ_FILE_BOOKMARKS_STORE_H__ */

// ex:ts=8:noet:
