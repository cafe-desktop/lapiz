/*
 * lapiz-file-browser-store.h - Lapiz plugin providing easy file access
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

#ifndef __LAPIZ_FILE_BROWSER_STORE_H__
#define __LAPIZ_FILE_BROWSER_STORE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define LAPIZ_TYPE_FILE_BROWSER_STORE			(lapiz_file_browser_store_get_type ())
#define LAPIZ_FILE_BROWSER_STORE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BROWSER_STORE, LapizFileBrowserStore))
#define LAPIZ_FILE_BROWSER_STORE_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BROWSER_STORE, LapizFileBrowserStore const))
#define LAPIZ_FILE_BROWSER_STORE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_FILE_BROWSER_STORE, LapizFileBrowserStoreClass))
#define LAPIZ_IS_FILE_BROWSER_STORE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_FILE_BROWSER_STORE))
#define LAPIZ_IS_FILE_BROWSER_STORE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_FILE_BROWSER_STORE))
#define LAPIZ_FILE_BROWSER_STORE_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_FILE_BROWSER_STORE, LapizFileBrowserStoreClass))

typedef enum
{
	LAPIZ_FILE_BROWSER_STORE_COLUMN_ICON = 0,
	LAPIZ_FILE_BROWSER_STORE_COLUMN_NAME,
	LAPIZ_FILE_BROWSER_STORE_COLUMN_URI,
	LAPIZ_FILE_BROWSER_STORE_COLUMN_FLAGS,
	LAPIZ_FILE_BROWSER_STORE_COLUMN_EMBLEM,
	LAPIZ_FILE_BROWSER_STORE_COLUMN_NUM
} LapizFileBrowserStoreColumn;

typedef enum
{
	LAPIZ_FILE_BROWSER_STORE_FLAG_IS_DIRECTORY = 1 << 0,
	LAPIZ_FILE_BROWSER_STORE_FLAG_IS_HIDDEN    = 1 << 1,
	LAPIZ_FILE_BROWSER_STORE_FLAG_IS_TEXT      = 1 << 2,
	LAPIZ_FILE_BROWSER_STORE_FLAG_LOADED       = 1 << 3,
	LAPIZ_FILE_BROWSER_STORE_FLAG_IS_FILTERED  = 1 << 4,
	LAPIZ_FILE_BROWSER_STORE_FLAG_IS_DUMMY     = 1 << 5
} LapizFileBrowserStoreFlag;

typedef enum
{
	LAPIZ_FILE_BROWSER_STORE_RESULT_OK,
	LAPIZ_FILE_BROWSER_STORE_RESULT_NO_CHANGE,
	LAPIZ_FILE_BROWSER_STORE_RESULT_ERROR,
	LAPIZ_FILE_BROWSER_STORE_RESULT_NO_TRASH,
	LAPIZ_FILE_BROWSER_STORE_RESULT_MOUNTING,
	LAPIZ_FILE_BROWSER_STORE_RESULT_NUM
} LapizFileBrowserStoreResult;

typedef enum
{
	LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_NONE        = 0,
	LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN = 1 << 0,
	LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY = 1 << 1
} LapizFileBrowserStoreFilterMode;

#define FILE_IS_DIR(flags)	(flags & LAPIZ_FILE_BROWSER_STORE_FLAG_IS_DIRECTORY)
#define FILE_IS_HIDDEN(flags)	(flags & LAPIZ_FILE_BROWSER_STORE_FLAG_IS_HIDDEN)
#define FILE_IS_TEXT(flags)	(flags & LAPIZ_FILE_BROWSER_STORE_FLAG_IS_TEXT)
#define FILE_LOADED(flags)	(flags & LAPIZ_FILE_BROWSER_STORE_FLAG_LOADED)
#define FILE_IS_FILTERED(flags)	(flags & LAPIZ_FILE_BROWSER_STORE_FLAG_IS_FILTERED)
#define FILE_IS_DUMMY(flags)	(flags & LAPIZ_FILE_BROWSER_STORE_FLAG_IS_DUMMY)

typedef struct _LapizFileBrowserStore        LapizFileBrowserStore;
typedef struct _LapizFileBrowserStoreClass   LapizFileBrowserStoreClass;
typedef struct _LapizFileBrowserStorePrivate LapizFileBrowserStorePrivate;

typedef gboolean (*LapizFileBrowserStoreFilterFunc) (LapizFileBrowserStore
						     * model,
						     GtkTreeIter * iter,
						     gpointer user_data);

struct _LapizFileBrowserStore
{
	GObject parent;

	LapizFileBrowserStorePrivate *priv;
};

struct _LapizFileBrowserStoreClass {
	GObjectClass parent_class;

	/* Signals */
	void (*begin_loading)        (LapizFileBrowserStore * model,
			              GtkTreeIter * iter);
	void (*end_loading)          (LapizFileBrowserStore * model,
			              GtkTreeIter * iter);
	void (*error)                (LapizFileBrowserStore * model,
	                              guint code,
		                      gchar * message);
	gboolean (*no_trash)	     (LapizFileBrowserStore * model,
				      GList * files);
	void (*rename)		     (LapizFileBrowserStore * model,
				      const gchar * olduri,
				      const gchar * newuri);
	void (*begin_refresh)	     (LapizFileBrowserStore * model);
	void (*end_refresh)	     (LapizFileBrowserStore * model);
	void (*unload)		     (LapizFileBrowserStore * model,
				      const gchar * uri);
};

GType lapiz_file_browser_store_get_type               (void) G_GNUC_CONST;
void _lapiz_file_browser_store_register_type          (GTypeModule * module);

LapizFileBrowserStore *lapiz_file_browser_store_new   (gchar const *root);

LapizFileBrowserStoreResult
lapiz_file_browser_store_set_root_and_virtual_root    (LapizFileBrowserStore * model,
						       gchar const *root,
			  			       gchar const *virtual_root);
LapizFileBrowserStoreResult
lapiz_file_browser_store_set_root                     (LapizFileBrowserStore * model,
				                       gchar const *root);
LapizFileBrowserStoreResult
lapiz_file_browser_store_set_virtual_root             (LapizFileBrowserStore * model,
					               GtkTreeIter * iter);
LapizFileBrowserStoreResult
lapiz_file_browser_store_set_virtual_root_from_string (LapizFileBrowserStore * model,
                                                       gchar const *root);
LapizFileBrowserStoreResult
lapiz_file_browser_store_set_virtual_root_up          (LapizFileBrowserStore * model);
LapizFileBrowserStoreResult
lapiz_file_browser_store_set_virtual_root_top         (LapizFileBrowserStore * model);

gboolean
lapiz_file_browser_store_get_iter_virtual_root        (LapizFileBrowserStore * model,
                                                       GtkTreeIter * iter);
gboolean lapiz_file_browser_store_get_iter_root       (LapizFileBrowserStore * model,
						       GtkTreeIter * iter);
gchar * lapiz_file_browser_store_get_root             (LapizFileBrowserStore * model);
gchar * lapiz_file_browser_store_get_virtual_root     (LapizFileBrowserStore * model);

gboolean lapiz_file_browser_store_iter_equal          (LapizFileBrowserStore * model,
                                                       GtkTreeIter * iter1,
					               GtkTreeIter * iter2);

void lapiz_file_browser_store_set_value               (LapizFileBrowserStore * tree_model,
                                                       GtkTreeIter * iter,
                                                       gint column,
                                                       GValue * value);

void _lapiz_file_browser_store_iter_expanded          (LapizFileBrowserStore * model,
                                                       GtkTreeIter * iter);
void _lapiz_file_browser_store_iter_collapsed         (LapizFileBrowserStore * model,
                                                       GtkTreeIter * iter);

LapizFileBrowserStoreFilterMode
lapiz_file_browser_store_get_filter_mode              (LapizFileBrowserStore * model);
void lapiz_file_browser_store_set_filter_mode         (LapizFileBrowserStore * model,
                                                       LapizFileBrowserStoreFilterMode mode);
void lapiz_file_browser_store_set_filter_func         (LapizFileBrowserStore * model,
                                                       LapizFileBrowserStoreFilterFunc func,
                                                       gpointer user_data);
void lapiz_file_browser_store_refilter                (LapizFileBrowserStore * model);
LapizFileBrowserStoreFilterMode
lapiz_file_browser_store_filter_mode_get_default      (void);

void lapiz_file_browser_store_refresh                 (LapizFileBrowserStore * model);
gboolean lapiz_file_browser_store_rename              (LapizFileBrowserStore * model,
                                                       GtkTreeIter * iter,
                                                       gchar const *new_name,
                                                       GError ** error);
LapizFileBrowserStoreResult
lapiz_file_browser_store_delete                       (LapizFileBrowserStore * model,
                                                       GtkTreeIter * iter,
                                                       gboolean trash);
LapizFileBrowserStoreResult
lapiz_file_browser_store_delete_all                   (LapizFileBrowserStore * model,
                                                       GList *rows,
                                                       gboolean trash);

gboolean lapiz_file_browser_store_new_file            (LapizFileBrowserStore * model,
                                                       GtkTreeIter * parent,
                                                       GtkTreeIter * iter);
gboolean lapiz_file_browser_store_new_directory       (LapizFileBrowserStore * model,
                                                       GtkTreeIter * parent,
                                                       GtkTreeIter * iter);

void lapiz_file_browser_store_cancel_mount_operation  (LapizFileBrowserStore *store);

G_END_DECLS
#endif				/* __LAPIZ_FILE_BROWSER_STORE_H__ */

// ex:ts=8:noet:
