/*
 * lapiz-file-browser-view.h - Lapiz plugin providing easy file access
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

#ifndef __LAPIZ_FILE_BROWSER_VIEW_H__
#define __LAPIZ_FILE_BROWSER_VIEW_H__

#include <ctk/ctk.h>

G_BEGIN_DECLS
#define LAPIZ_TYPE_FILE_BROWSER_VIEW			(lapiz_file_browser_view_get_type ())
#define LAPIZ_FILE_BROWSER_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BROWSER_VIEW, LapizFileBrowserView))
#define LAPIZ_FILE_BROWSER_VIEW_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BROWSER_VIEW, LapizFileBrowserView const))
#define LAPIZ_FILE_BROWSER_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_FILE_BROWSER_VIEW, LapizFileBrowserViewClass))
#define LAPIZ_IS_FILE_BROWSER_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_FILE_BROWSER_VIEW))
#define LAPIZ_IS_FILE_BROWSER_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_FILE_BROWSER_VIEW))
#define LAPIZ_FILE_BROWSER_VIEW_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_FILE_BROWSER_VIEW, LapizFileBrowserViewClass))

typedef struct _LapizFileBrowserView        LapizFileBrowserView;
typedef struct _LapizFileBrowserViewClass   LapizFileBrowserViewClass;
typedef struct _LapizFileBrowserViewPrivate LapizFileBrowserViewPrivate;

typedef enum {
	LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_DOUBLE,
	LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE
} LapizFileBrowserViewClickPolicy;

struct _LapizFileBrowserView
{
	GtkTreeView parent;

	LapizFileBrowserViewPrivate *priv;
};

struct _LapizFileBrowserViewClass
{
	GtkTreeViewClass parent_class;

	/* Signals */
	void (*error) (LapizFileBrowserView * filetree,
	               guint code,
		       gchar const *message);
	void (*file_activated) (LapizFileBrowserView * filetree,
				    GtkTreeIter *iter);
	void (*directory_activated) (LapizFileBrowserView * filetree,
				    GtkTreeIter *iter);
	void (*bookmark_activated) (LapizFileBrowserView * filetree,
				    GtkTreeIter *iter);
};

GType lapiz_file_browser_view_get_type			(void) G_GNUC_CONST;
void _lapiz_file_browser_view_register_type		(GTypeModule 			* module);

GtkWidget *lapiz_file_browser_view_new			(void);
void lapiz_file_browser_view_set_model			(LapizFileBrowserView 		* tree_view,
							 GtkTreeModel 			* model);
void lapiz_file_browser_view_start_rename		(LapizFileBrowserView 		* tree_view,
							 GtkTreeIter 			* iter);
void lapiz_file_browser_view_set_click_policy		(LapizFileBrowserView 		* tree_view,
							 LapizFileBrowserViewClickPolicy  policy);
void lapiz_file_browser_view_set_restore_expand_state	(LapizFileBrowserView 		* tree_view,
							 gboolean 			  restore_expand_state);

G_END_DECLS
#endif				/* __LAPIZ_FILE_BROWSER_VIEW_H__ */

// ex:ts=8:noet:
