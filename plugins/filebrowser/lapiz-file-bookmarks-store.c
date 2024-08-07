/*
 * lapiz-file-bookmarks-store.c - Lapiz plugin providing easy file access
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

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <lapiz/lapiz-utils.h>

#include "lapiz-file-bookmarks-store.h"
#include "lapiz-file-browser-utils.h"

struct _LapizFileBookmarksStorePrivate
{
	GVolumeMonitor * volume_monitor;
	GFileMonitor * bookmarks_monitor;
};

static void remove_node               (CtkTreeModel * model,
                                       CtkTreeIter * iter);

static void on_fs_changed             (GVolumeMonitor 		*monitor,
                                       GObject 			*object,
                                       LapizFileBookmarksStore 	*model);

static void on_bookmarks_file_changed (GFileMonitor * monitor,
				       GFile * file,
				       GFile * other_file,
				       GFileMonitorEvent event_type,
				       LapizFileBookmarksStore * model);
static gboolean find_with_flags       (CtkTreeModel * model,
                                       CtkTreeIter * iter,
                                       gpointer obj,
                                       guint flags,
                                       guint notflags);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizFileBookmarksStore,
                                lapiz_file_bookmarks_store,
                                CTK_TYPE_TREE_STORE,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizFileBookmarksStore))

static void
lapiz_file_bookmarks_store_dispose (GObject * object)
{
	LapizFileBookmarksStore *obj = LAPIZ_FILE_BOOKMARKS_STORE (object);

	if (obj->priv->volume_monitor != NULL) {
		g_signal_handlers_disconnect_by_func (obj->priv->volume_monitor,
						      on_fs_changed,
						      obj);

		g_object_unref (obj->priv->volume_monitor);
		obj->priv->volume_monitor = NULL;
	}

	if (obj->priv->bookmarks_monitor != NULL) {
		g_object_unref (obj->priv->bookmarks_monitor);
		obj->priv->bookmarks_monitor = NULL;
	}

	G_OBJECT_CLASS (lapiz_file_bookmarks_store_parent_class)->dispose (object);
}

static void
lapiz_file_bookmarks_store_finalize (GObject * object)
{
	G_OBJECT_CLASS (lapiz_file_bookmarks_store_parent_class)->finalize (object);
}

static void
lapiz_file_bookmarks_store_class_init (LapizFileBookmarksStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = lapiz_file_bookmarks_store_dispose;
	object_class->finalize = lapiz_file_bookmarks_store_finalize;
}

static void
lapiz_file_bookmarks_store_class_finalize (LapizFileBookmarksStoreClass *klass G_GNUC_UNUSED)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
lapiz_file_bookmarks_store_init (LapizFileBookmarksStore * obj)
{
	obj->priv = lapiz_file_bookmarks_store_get_instance_private (obj);
}

/* Private */
static void
add_node (LapizFileBookmarksStore *model,
	  GdkPixbuf 		  *pixbuf,
	  const gchar 		  *name,
	  GObject 		  *obj,
	  guint 		   flags,
	  CtkTreeIter 		  *iter)
{
	CtkTreeIter newiter;

	ctk_tree_store_append (CTK_TREE_STORE (model), &newiter, NULL);

	ctk_tree_store_set (CTK_TREE_STORE (model), &newiter,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_ICON, pixbuf,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_NAME, name,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_OBJECT, obj,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS, flags,
			    -1);

	if (iter != NULL)
		*iter = newiter;
}

static gboolean
add_file (LapizFileBookmarksStore *model,
	  GFile 		  *file,
	  const gchar 		  *name,
	  guint 		   flags,
	  CtkTreeIter 		  *iter)
{
	GdkPixbuf *pixbuf = NULL;
	gboolean native;
	gchar *newname;

	native = g_file_is_native (file);

	if (native && !g_file_query_exists (file, NULL)) {
		return FALSE;
	}

	if (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_HOME)
		pixbuf = lapiz_file_browser_utils_pixbuf_from_theme ("user-home", CTK_ICON_SIZE_MENU);
	else if (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_DESKTOP)
		pixbuf = lapiz_file_browser_utils_pixbuf_from_theme ("user-desktop", CTK_ICON_SIZE_MENU);
	else if (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_ROOT)
		pixbuf = lapiz_file_browser_utils_pixbuf_from_theme ("drive-harddisk", CTK_ICON_SIZE_MENU);

	if (pixbuf == NULL) {
		/* getting the icon is a sync get_info call, so we just do it for local files */
		if (native) {
			pixbuf = lapiz_file_browser_utils_pixbuf_from_file (file, CTK_ICON_SIZE_MENU);
		} else {
			pixbuf = lapiz_file_browser_utils_pixbuf_from_theme ("folder", CTK_ICON_SIZE_MENU);
		}
	}

	if (name == NULL) {
		newname = lapiz_file_browser_utils_file_basename (file);
	} else {
		newname = g_strdup (name);
	}

	add_node (model, pixbuf, newname, G_OBJECT (file), flags, iter);

	if (pixbuf)
		g_object_unref (pixbuf);

	g_free (newname);

	return TRUE;
}

static void
check_mount_separator (LapizFileBookmarksStore * model, guint flags,
			gboolean added)
{
	CtkTreeIter iter;
	gboolean found;

	found =
	    find_with_flags (CTK_TREE_MODEL (model), &iter, NULL,
			     flags |
			     LAPIZ_FILE_BOOKMARKS_STORE_IS_SEPARATOR, 0);

	if (added && !found) {
		/* Add the separator */
		add_node (model, NULL, NULL, NULL,
			  flags | LAPIZ_FILE_BOOKMARKS_STORE_IS_SEPARATOR,
			  NULL);
	} else if (!added && found) {
		remove_node (CTK_TREE_MODEL (model), &iter);
	}
}

static void
init_special_directories (LapizFileBookmarksStore * model)
{
	gchar const *path;
	GFile * file;

	path = g_get_home_dir ();
	if (path != NULL)
	{
		file = g_file_new_for_path (path);
		add_file (model, file, NULL, LAPIZ_FILE_BOOKMARKS_STORE_IS_HOME |
			 LAPIZ_FILE_BOOKMARKS_STORE_IS_SPECIAL_DIR, NULL);
		g_object_unref (file);
	}

	path = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
	if (path != NULL)
	{
		file = g_file_new_for_path (path);
		add_file (model, file, NULL, LAPIZ_FILE_BOOKMARKS_STORE_IS_DESKTOP |
			  LAPIZ_FILE_BOOKMARKS_STORE_IS_SPECIAL_DIR, NULL);
		g_object_unref (file);
	}

	path = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
	if (path != NULL)
	{
		file = g_file_new_for_path (path);
		add_file (model, file, NULL, LAPIZ_FILE_BOOKMARKS_STORE_IS_DOCUMENTS |
			 LAPIZ_FILE_BOOKMARKS_STORE_IS_SPECIAL_DIR, NULL);
		g_object_unref (file);
	}

	file = g_file_new_for_uri ("file:///");
	add_file (model, file, _("File System"), LAPIZ_FILE_BOOKMARKS_STORE_IS_ROOT, NULL);
	g_object_unref (file);

	check_mount_separator (model, LAPIZ_FILE_BOOKMARKS_STORE_IS_ROOT, TRUE);
}

static void
get_fs_properties (gpointer    fs,
		   gchar     **name,
		   GdkPixbuf **pixbuf,
		   guint      *flags)
{
	GIcon *icon = NULL;

	*flags = LAPIZ_FILE_BOOKMARKS_STORE_IS_FS;
	*name = NULL;
	*pixbuf = NULL;

	if (G_IS_DRIVE (fs))
	{
		icon = g_drive_get_icon (G_DRIVE (fs));
		*name = g_drive_get_name (G_DRIVE (fs));

		*flags |= LAPIZ_FILE_BOOKMARKS_STORE_IS_DRIVE;
	}
	else if (G_IS_VOLUME (fs))
	{
		icon = g_volume_get_icon (G_VOLUME (fs));
		*name = g_volume_get_name (G_VOLUME (fs));

		*flags |= LAPIZ_FILE_BOOKMARKS_STORE_IS_VOLUME;
	}
	else if (G_IS_MOUNT (fs))
	{
		icon = g_mount_get_icon (G_MOUNT (fs));
		*name = g_mount_get_name (G_MOUNT (fs));

		*flags |= LAPIZ_FILE_BOOKMARKS_STORE_IS_MOUNT;
	}

	if (icon)
	{
		*pixbuf = lapiz_file_browser_utils_pixbuf_from_icon (icon, CTK_ICON_SIZE_MENU);
		g_object_unref (icon);
	}
}


static void
add_fs (LapizFileBookmarksStore *model,
	gpointer 		 fs,
	guint 			 flags,
	CtkTreeIter		*iter)
{
	gchar *name;
	GdkPixbuf *pixbuf;
	guint fsflags;

	get_fs_properties (fs, &name, &pixbuf, &fsflags);
	add_node (model, pixbuf, name, fs, flags | fsflags, iter);

	if (pixbuf)
		g_object_unref (pixbuf);

	g_free (name);
	check_mount_separator (model, LAPIZ_FILE_BOOKMARKS_STORE_IS_FS, TRUE);
}

static void
process_volume_cb (GVolume 		   *volume,
		   LapizFileBookmarksStore *model)
{
	GMount *mount;
	guint flags = LAPIZ_FILE_BOOKMARKS_STORE_NONE;
	mount = g_volume_get_mount (volume);

	/* CHECK: should we use the LOCAL/REMOTE thing still? */
	if (mount)
	{
		/* Show mounted volume */
		add_fs (model, mount, flags, NULL);
		g_object_unref (mount);
	}
	else if (g_volume_can_mount (volume))
	{
		/* We also show the unmounted volume here so users can
		   mount it if they want to access it */
		add_fs (model, volume, flags, NULL);
	}
}

static void
process_drive_novolumes (LapizFileBookmarksStore *model,
			 GDrive			 *drive)
{
	if (g_drive_is_media_removable (drive) &&
	   !g_drive_is_media_check_automatic (drive) &&
	    g_drive_can_poll_for_media (drive))
	{
		/* This can be the case for floppy drives or other
		   drives where media detection fails. We show the
		   drive and poll for media when the user activates
		   it */
		add_fs (model, drive, LAPIZ_FILE_BOOKMARKS_STORE_NONE, NULL);
	}
}

static void
process_drive_cb (GDrive   	          *drive,
	          LapizFileBookmarksStore *model)
{
	GList *volumes;

	volumes = g_drive_get_volumes (drive);

	if (volumes)
	{
		/* Add all volumes for the drive */
		g_list_foreach (volumes, (GFunc)process_volume_cb, model);
		g_list_free (volumes);
	}
	else
	{
		process_drive_novolumes (model, drive);
	}
}

static void
init_drives (LapizFileBookmarksStore *model)
{
	GList *drives;

	drives = g_volume_monitor_get_connected_drives (model->priv->volume_monitor);

	g_list_foreach (drives, (GFunc)process_drive_cb, model);
	g_list_foreach (drives, (GFunc)g_object_unref, NULL);
	g_list_free (drives);
}

static void
process_volume_nodrive_cb (GVolume 		   *volume,
			   LapizFileBookmarksStore *model)
{
	GDrive *drive;

	drive = g_volume_get_drive (volume);

	if (drive)
	{
		g_object_unref (drive);
		return;
	}

	process_volume_cb (volume, model);
}

static void
init_volumes (LapizFileBookmarksStore *model)
{
	GList *volumes;

	volumes = g_volume_monitor_get_volumes (model->priv->volume_monitor);

	g_list_foreach (volumes, (GFunc)process_volume_nodrive_cb, model);
	g_list_foreach (volumes, (GFunc)g_object_unref, NULL);
	g_list_free (volumes);
}

static void
process_mount_novolume_cb (GMount 		   *mount,
			   LapizFileBookmarksStore *model)
{
	GVolume *volume;

	volume = g_mount_get_volume (mount);

	if (volume)
	{
		g_object_unref (volume);
	}
	else if (!g_mount_is_shadowed (mount))
	{
		/* Add the mount */
		add_fs (model, mount, LAPIZ_FILE_BOOKMARKS_STORE_NONE, NULL);
	}
}

static void
init_mounts (LapizFileBookmarksStore *model)
{
	GList *mounts;

	mounts = g_volume_monitor_get_mounts (model->priv->volume_monitor);

	g_list_foreach (mounts, (GFunc)process_mount_novolume_cb, model);
	g_list_foreach (mounts, (GFunc)g_object_unref, NULL);
	g_list_free (mounts);
}

static void
init_fs (LapizFileBookmarksStore * model)
{
	if (model->priv->volume_monitor == NULL) {
		const gchar **ptr;
		const gchar *signals[] = {
			"drive-connected", "drive-changed", "drive-disconnected",
			"volume-added", "volume-removed", "volume-changed",
			"mount-added", "mount-removed", "mount-changed",
			NULL
		};

		model->priv->volume_monitor = g_volume_monitor_get ();

		/* Connect signals */
		for (ptr = signals; *ptr; ptr++)
		{
			g_signal_connect (model->priv->volume_monitor,
					  *ptr,
					  G_CALLBACK (on_fs_changed), model);
		}
	}

	/* First go through all the connected drives */
	init_drives (model);

	/* Then add all volumes, not associated with a drive */
	init_volumes (model);

	/* Then finally add all mounts that have no volume */
	init_mounts (model);
}

static gboolean
add_bookmark (LapizFileBookmarksStore * model,
	      gchar const * name,
	      gchar const * uri)
{
	GFile * file;
	gboolean ret;
	guint flags = LAPIZ_FILE_BOOKMARKS_STORE_IS_BOOKMARK;
	CtkTreeIter iter;

	file = g_file_new_for_uri (uri);

	if (g_file_is_native (file)) {
		flags |= LAPIZ_FILE_BOOKMARKS_STORE_IS_LOCAL_BOOKMARK;
	} else {
		flags |= LAPIZ_FILE_BOOKMARKS_STORE_IS_REMOTE_BOOKMARK;
	}

	ret = add_file (model, file, name, flags, &iter);

	g_object_unref (file);

	return ret;
}

static gchar *
get_bookmarks_file (void)
{
	return g_build_filename (g_get_user_config_dir (), "ctk-3.0", "bookmarks", NULL);
}

static gchar *
get_legacy_bookmarks_file (void)
{
	return g_build_filename (g_get_home_dir (), ".ctk-bookmarks", NULL);
}

static gboolean
parse_bookmarks_file (LapizFileBookmarksStore *model,
		      const gchar             *bookmarks,
		      gboolean                *added)
{
	GError *error = NULL;
	gchar *contents;
	gchar **lines;
	gchar **line;

	if (!g_file_get_contents (bookmarks, &contents, NULL, &error))
	{
		/* The bookmarks file doesn't exist (which is perfectly fine) */
		g_error_free (error);

		return FALSE;
	}

	lines = g_strsplit (contents, "\n", 0);

	for (line = lines; *line; ++line)
	{
		if (**line)
		{
			gchar *pos;
			gchar *name;

			/* CHECK: is this really utf8? */
			pos = g_utf8_strchr (*line, -1, ' ');

			if (pos != NULL)
			{
				*pos = '\0';
				name = pos + 1;
			}
			else
			{
				name = NULL;
			}

			/* the bookmarks file should contain valid
			 * URIs, but paranoia is good */
			if (lapiz_utils_is_valid_uri (*line))
			{
				*added |= add_bookmark (model, name, *line);
			}
		}
	}

	g_strfreev (lines);
	g_free (contents);

	/* Add a watch */
	if (model->priv->bookmarks_monitor == NULL)
	{
		GFile *file;

		file = g_file_new_for_path (bookmarks);
		model->priv->bookmarks_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
		g_object_unref (file);

		g_signal_connect (model->priv->bookmarks_monitor,
				  "changed",
				  G_CALLBACK (on_bookmarks_file_changed),
				  model);
	}

	return TRUE;
}

static void
init_bookmarks (LapizFileBookmarksStore *model)
{
	gchar *bookmarks;
	gboolean added = FALSE;

	bookmarks = get_bookmarks_file ();

	if (!parse_bookmarks_file (model, bookmarks, &added))
	{
		g_free (bookmarks);

		/* try the old location (ctk <= 3.4) */
		bookmarks = get_legacy_bookmarks_file ();
		parse_bookmarks_file (model, bookmarks, &added);
	}

	if (added) {
		/* Bookmarks separator */
		add_node (model, NULL, NULL, NULL,
			  LAPIZ_FILE_BOOKMARKS_STORE_IS_BOOKMARK |
			  LAPIZ_FILE_BOOKMARKS_STORE_IS_SEPARATOR, NULL);
	}

	g_free (bookmarks);
}

static gint flags_order[] = {
	LAPIZ_FILE_BOOKMARKS_STORE_IS_HOME,
	LAPIZ_FILE_BOOKMARKS_STORE_IS_DESKTOP,
	LAPIZ_FILE_BOOKMARKS_STORE_IS_SPECIAL_DIR,
	LAPIZ_FILE_BOOKMARKS_STORE_IS_ROOT,
	LAPIZ_FILE_BOOKMARKS_STORE_IS_FS,
	LAPIZ_FILE_BOOKMARKS_STORE_IS_BOOKMARK,
	-1
};

static gint
utf8_casecmp (gchar const *s1, const gchar * s2)
{
	gchar *n1;
	gchar *n2;
	gint result;

	n1 = g_utf8_casefold (s1, -1);
	n2 = g_utf8_casefold (s2, -1);

	result = g_utf8_collate (n1, n2);

	g_free (n1);
	g_free (n2);

	return result;
}

static gint
bookmarks_compare_names (CtkTreeModel * model, CtkTreeIter * a,
			 CtkTreeIter * b)
{
	gchar *n1;
	gchar *n2;
	gint result;
	guint f1;
	guint f2;

	ctk_tree_model_get (model, a,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_NAME, &n1,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS, &f1,
			    -1);
	ctk_tree_model_get (model, b,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_NAME, &n2,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS, &f2,
			    -1);

	/* do not sort actual bookmarks to keep same order as in baul */
	if ((f1 & LAPIZ_FILE_BOOKMARKS_STORE_IS_BOOKMARK) &&
	    (f2 & LAPIZ_FILE_BOOKMARKS_STORE_IS_BOOKMARK))
		result = 0;
	else if (n1 == NULL && n2 == NULL)
		result = 0;
	else if (n1 == NULL)
		result = -1;
	else if (n2 == NULL)
		result = 1;
	else
		result = utf8_casecmp (n1, n2);

	g_free (n1);
	g_free (n2);

	return result;
}

static gint
bookmarks_compare_flags (CtkTreeModel * model, CtkTreeIter * a,
			 CtkTreeIter * b)
{
	guint f1;
	guint f2;
	gint *flags;
	guint sep;

	sep = LAPIZ_FILE_BOOKMARKS_STORE_IS_SEPARATOR;

	ctk_tree_model_get (model, a,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS, &f1,
			    -1);
	ctk_tree_model_get (model, b,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS, &f2,
			    -1);

	for (flags = flags_order; *flags != -1; ++flags) {
		if ((f1 & *flags) != (f2 & *flags)) {
			if (f1 & *flags) {
				return -1;
			} else {
				return 1;
			}
		} else if ((f1 & *flags) && (f1 & sep) != (f2 & sep)) {
			if (f1 & sep)
				return -1;
			else
				return 1;
		}
	}

	return 0;
}

static gint
bookmarks_compare_func (CtkTreeModel *model,
			CtkTreeIter  *a,
			CtkTreeIter  *b,
			gpointer      user_data G_GNUC_UNUSED)
{
	gint result;

	result = bookmarks_compare_flags (model, a, b);

	if (result == 0)
		result = bookmarks_compare_names (model, a, b);

	return result;
}

static gboolean
find_with_flags (CtkTreeModel * model, CtkTreeIter * iter, gpointer obj,
		 guint flags, guint notflags)
{
	CtkTreeIter child;
	guint childflags = 0;
 	GObject * childobj;
 	gboolean fequal;

	if (!ctk_tree_model_get_iter_first (model, &child))
		return FALSE;

	do {
		ctk_tree_model_get (model, &child,
				    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_OBJECT,
				    &childobj,
				    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS,
				    &childflags, -1);

		fequal = (obj == childobj);

		if (childobj)
			g_object_unref (childobj);

		if ((obj == NULL || fequal) &&
		    (childflags & flags) == flags
		    && !(childflags & notflags)) {
			*iter = child;
			return TRUE;
		}
	} while (ctk_tree_model_iter_next (model, &child));

	return FALSE;
}

static void
remove_node (CtkTreeModel * model, CtkTreeIter * iter)
{
	guint flags;

	ctk_tree_model_get (model, iter,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS, &flags,
			    -1);

	if (!(flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_SEPARATOR)) {
		if (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_FS) {
			check_mount_separator (LAPIZ_FILE_BOOKMARKS_STORE (model),
			     		       flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_FS,
					       FALSE);
		}
	}

	ctk_tree_store_remove (CTK_TREE_STORE (model), iter);
}

static void
remove_bookmarks (LapizFileBookmarksStore * model)
{
	CtkTreeIter iter;

	while (find_with_flags (CTK_TREE_MODEL (model), &iter, NULL,
				LAPIZ_FILE_BOOKMARKS_STORE_IS_BOOKMARK,
				0)) {
		remove_node (CTK_TREE_MODEL (model), &iter);
	}
}

static void
initialize_fill (LapizFileBookmarksStore * model)
{
	init_special_directories (model);
	init_fs (model);
	init_bookmarks (model);
}

/* Public */
LapizFileBookmarksStore *
lapiz_file_bookmarks_store_new (void)
{
	LapizFileBookmarksStore *model;
	GType column_types[] = {
		GDK_TYPE_PIXBUF,
		G_TYPE_STRING,
		G_TYPE_OBJECT,
		G_TYPE_UINT
	};

	model = g_object_new (LAPIZ_TYPE_FILE_BOOKMARKS_STORE, NULL);
	ctk_tree_store_set_column_types (CTK_TREE_STORE (model),
					 LAPIZ_FILE_BOOKMARKS_STORE_N_COLUMNS,
					 column_types);

	ctk_tree_sortable_set_default_sort_func (CTK_TREE_SORTABLE (model),
						 bookmarks_compare_func,
						 NULL, NULL);
	ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (model),
					      CTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      CTK_SORT_ASCENDING);

	initialize_fill (model);

	return model;
}

gchar *
lapiz_file_bookmarks_store_get_uri (LapizFileBookmarksStore * model,
				    CtkTreeIter * iter)
{
	GObject * obj;
	guint flags;
	gchar * ret = NULL;

	g_return_val_if_fail (LAPIZ_IS_FILE_BOOKMARKS_STORE (model), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	ctk_tree_model_get (CTK_TREE_MODEL (model), iter,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS,
			    &flags,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_OBJECT,
			    &obj,
			    -1);

	if (obj != NULL)
	{
		if (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_FS)
		{
			if (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_MOUNT)
			{
				GFile * file;

				file = g_mount_get_root (G_MOUNT (obj));
				ret = g_file_get_uri (file);
				g_object_unref (file);
			}
		}
		else
		{
			ret = g_file_get_uri (G_FILE (obj));
		}

		g_object_unref (obj);
	}

	return ret;
}

void
lapiz_file_bookmarks_store_refresh (LapizFileBookmarksStore * model)
{
	ctk_tree_store_clear (CTK_TREE_STORE (model));
	initialize_fill (model);
}

static void
on_fs_changed (GVolumeMonitor          *monitor G_GNUC_UNUSED,
	       GObject                 *object G_GNUC_UNUSED,
	       LapizFileBookmarksStore *model)
{
	CtkTreeModel *tree_model = CTK_TREE_MODEL (model);
	guint flags = LAPIZ_FILE_BOOKMARKS_STORE_IS_FS;
	guint noflags = LAPIZ_FILE_BOOKMARKS_STORE_IS_SEPARATOR;
	CtkTreeIter iter;

	/* clear all fs items */
	while (find_with_flags (tree_model, &iter, NULL, flags, noflags))
		remove_node (tree_model, &iter);

	/* then reinitialize */
	init_fs (model);
}

static void
on_bookmarks_file_changed (GFileMonitor            *monitor,
			   GFile                   *file G_GNUC_UNUSED,
			   GFile                   *other_file G_GNUC_UNUSED,
			   GFileMonitorEvent        event_type,
			   LapizFileBookmarksStore *model)
{
	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGED:
	case G_FILE_MONITOR_EVENT_CREATED:
		/* Re-initialize bookmarks */
		remove_bookmarks (model);
		init_bookmarks (model);
		break;
	case G_FILE_MONITOR_EVENT_DELETED: // FIXME: shouldn't we also monitor the directory?
		/* Remove bookmarks */
		remove_bookmarks (model);
		g_object_unref (monitor);
		model->priv->bookmarks_monitor = NULL;
		break;
	default:
		break;
	}
}

void
_lapiz_file_bookmarks_store_register_type (GTypeModule *type_module)
{
	lapiz_file_bookmarks_store_register_type (type_module);
}

// ex:ts=8:noet:
