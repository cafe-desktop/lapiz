/*
 * lapiz-file-browser-widget.h - Lapiz plugin providing easy file access
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

#ifndef __LAPIZ_FILE_BROWSER_WIDGET_H__
#define __LAPIZ_FILE_BROWSER_WIDGET_H__

#include <ctk/ctk.h>
#include "lapiz-file-browser-store.h"
#include "lapiz-file-bookmarks-store.h"
#include "lapiz-file-browser-view.h"

G_BEGIN_DECLS
#define LAPIZ_TYPE_FILE_BROWSER_WIDGET			(lapiz_file_browser_widget_get_type ())
#define LAPIZ_FILE_BROWSER_WIDGET(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BROWSER_WIDGET, LapizFileBrowserWidget))
#define LAPIZ_FILE_BROWSER_WIDGET_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BROWSER_WIDGET, LapizFileBrowserWidget const))
#define LAPIZ_FILE_BROWSER_WIDGET_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_FILE_BROWSER_WIDGET, LapizFileBrowserWidgetClass))
#define LAPIZ_IS_FILE_BROWSER_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_FILE_BROWSER_WIDGET))
#define LAPIZ_IS_FILE_BROWSER_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_FILE_BROWSER_WIDGET))
#define LAPIZ_FILE_BROWSER_WIDGET_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_FILE_BROWSER_WIDGET, LapizFileBrowserWidgetClass))

typedef struct _LapizFileBrowserWidget        LapizFileBrowserWidget;
typedef struct _LapizFileBrowserWidgetClass   LapizFileBrowserWidgetClass;
typedef struct _LapizFileBrowserWidgetPrivate LapizFileBrowserWidgetPrivate;

typedef
gboolean (*LapizFileBrowserWidgetFilterFunc) (LapizFileBrowserWidget * obj,
					      LapizFileBrowserStore *
					      model, GtkTreeIter * iter,
					      gpointer user_data);

struct _LapizFileBrowserWidget
{
	GtkBox parent;

	LapizFileBrowserWidgetPrivate *priv;
};

struct _LapizFileBrowserWidgetClass
{
	GtkBoxClass parent_class;

	/* Signals */
	void (*uri_activated)        (LapizFileBrowserWidget * widget,
			              gchar const *uri);
	void (*error)                (LapizFileBrowserWidget * widget,
	                              guint code,
		                      gchar const *message);
	gboolean (*confirm_delete)   (LapizFileBrowserWidget * widget,
	                              LapizFileBrowserStore * model,
	                              GList *list);
	gboolean (*confirm_no_trash) (LapizFileBrowserWidget * widget,
	                              GList *list);
};

GType lapiz_file_browser_widget_get_type            (void) G_GNUC_CONST;
void _lapiz_file_browser_widget_register_type       (GTypeModule * module);

GtkWidget *lapiz_file_browser_widget_new            (const gchar *data_dir);

void lapiz_file_browser_widget_show_bookmarks       (LapizFileBrowserWidget * obj);
void lapiz_file_browser_widget_show_files           (LapizFileBrowserWidget * obj);

void lapiz_file_browser_widget_set_root             (LapizFileBrowserWidget * obj,
                                                     gchar const *root,
                                                     gboolean virtual_root);
void
lapiz_file_browser_widget_set_root_and_virtual_root (LapizFileBrowserWidget * obj,
						     gchar const *root,
						     gchar const *virtual_root);

gboolean
lapiz_file_browser_widget_get_selected_directory    (LapizFileBrowserWidget * obj,
                                                     GtkTreeIter * iter);

LapizFileBrowserStore *
lapiz_file_browser_widget_get_browser_store         (LapizFileBrowserWidget * obj);
LapizFileBookmarksStore *
lapiz_file_browser_widget_get_bookmarks_store       (LapizFileBrowserWidget * obj);
LapizFileBrowserView *
lapiz_file_browser_widget_get_browser_view          (LapizFileBrowserWidget * obj);
GtkWidget *
lapiz_file_browser_widget_get_filter_entry          (LapizFileBrowserWidget * obj);

GtkUIManager *
lapiz_file_browser_widget_get_ui_manager            (LapizFileBrowserWidget * obj);

gulong lapiz_file_browser_widget_add_filter         (LapizFileBrowserWidget * obj,
                                                     LapizFileBrowserWidgetFilterFunc func,
                                                     gpointer user_data,
                                                     GDestroyNotify notify);
void lapiz_file_browser_widget_remove_filter        (LapizFileBrowserWidget * obj,
                                                     gulong id);
void lapiz_file_browser_widget_set_filter_pattern   (LapizFileBrowserWidget * obj,
                                                     gchar const *pattern);

void lapiz_file_browser_widget_refresh		    (LapizFileBrowserWidget * obj);
void lapiz_file_browser_widget_history_back	    (LapizFileBrowserWidget * obj);
void lapiz_file_browser_widget_history_forward	    (LapizFileBrowserWidget * obj);

G_END_DECLS
#endif /* __LAPIZ_FILE_BROWSER_WIDGET_H__ */

// ex:ts=8:noet:
