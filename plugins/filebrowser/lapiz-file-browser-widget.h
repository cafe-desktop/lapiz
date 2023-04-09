/*
 * lapiz-file-browser-widget.h - Pluma plugin providing easy file access
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

#include <gtk/gtk.h>
#include "lapiz-file-browser-store.h"
#include "lapiz-file-bookmarks-store.h"
#include "lapiz-file-browser-view.h"

G_BEGIN_DECLS
#define LAPIZ_TYPE_FILE_BROWSER_WIDGET			(lapiz_file_browser_widget_get_type ())
#define LAPIZ_FILE_BROWSER_WIDGET(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BROWSER_WIDGET, PlumaFileBrowserWidget))
#define LAPIZ_FILE_BROWSER_WIDGET_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_FILE_BROWSER_WIDGET, PlumaFileBrowserWidget const))
#define LAPIZ_FILE_BROWSER_WIDGET_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_FILE_BROWSER_WIDGET, PlumaFileBrowserWidgetClass))
#define LAPIZ_IS_FILE_BROWSER_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_FILE_BROWSER_WIDGET))
#define LAPIZ_IS_FILE_BROWSER_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_FILE_BROWSER_WIDGET))
#define LAPIZ_FILE_BROWSER_WIDGET_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_FILE_BROWSER_WIDGET, PlumaFileBrowserWidgetClass))

typedef struct _PlumaFileBrowserWidget        PlumaFileBrowserWidget;
typedef struct _PlumaFileBrowserWidgetClass   PlumaFileBrowserWidgetClass;
typedef struct _PlumaFileBrowserWidgetPrivate PlumaFileBrowserWidgetPrivate;

typedef
gboolean (*PlumaFileBrowserWidgetFilterFunc) (PlumaFileBrowserWidget * obj,
					      PlumaFileBrowserStore *
					      model, GtkTreeIter * iter,
					      gpointer user_data);

struct _PlumaFileBrowserWidget
{
	GtkBox parent;

	PlumaFileBrowserWidgetPrivate *priv;
};

struct _PlumaFileBrowserWidgetClass
{
	GtkBoxClass parent_class;

	/* Signals */
	void (*uri_activated)        (PlumaFileBrowserWidget * widget,
			              gchar const *uri);
	void (*error)                (PlumaFileBrowserWidget * widget,
	                              guint code,
		                      gchar const *message);
	gboolean (*confirm_delete)   (PlumaFileBrowserWidget * widget,
	                              PlumaFileBrowserStore * model,
	                              GList *list);
	gboolean (*confirm_no_trash) (PlumaFileBrowserWidget * widget,
	                              GList *list);
};

GType lapiz_file_browser_widget_get_type            (void) G_GNUC_CONST;
void _lapiz_file_browser_widget_register_type       (GTypeModule * module);

GtkWidget *lapiz_file_browser_widget_new            (const gchar *data_dir);

void lapiz_file_browser_widget_show_bookmarks       (PlumaFileBrowserWidget * obj);
void lapiz_file_browser_widget_show_files           (PlumaFileBrowserWidget * obj);

void lapiz_file_browser_widget_set_root             (PlumaFileBrowserWidget * obj,
                                                     gchar const *root,
                                                     gboolean virtual_root);
void
lapiz_file_browser_widget_set_root_and_virtual_root (PlumaFileBrowserWidget * obj,
						     gchar const *root,
						     gchar const *virtual_root);

gboolean
lapiz_file_browser_widget_get_selected_directory    (PlumaFileBrowserWidget * obj,
                                                     GtkTreeIter * iter);

PlumaFileBrowserStore *
lapiz_file_browser_widget_get_browser_store         (PlumaFileBrowserWidget * obj);
PlumaFileBookmarksStore *
lapiz_file_browser_widget_get_bookmarks_store       (PlumaFileBrowserWidget * obj);
PlumaFileBrowserView *
lapiz_file_browser_widget_get_browser_view          (PlumaFileBrowserWidget * obj);
GtkWidget *
lapiz_file_browser_widget_get_filter_entry          (PlumaFileBrowserWidget * obj);

GtkUIManager *
lapiz_file_browser_widget_get_ui_manager            (PlumaFileBrowserWidget * obj);

gulong lapiz_file_browser_widget_add_filter         (PlumaFileBrowserWidget * obj,
                                                     PlumaFileBrowserWidgetFilterFunc func,
                                                     gpointer user_data,
                                                     GDestroyNotify notify);
void lapiz_file_browser_widget_remove_filter        (PlumaFileBrowserWidget * obj,
                                                     gulong id);
void lapiz_file_browser_widget_set_filter_pattern   (PlumaFileBrowserWidget * obj,
                                                     gchar const *pattern);

void lapiz_file_browser_widget_refresh		    (PlumaFileBrowserWidget * obj);
void lapiz_file_browser_widget_history_back	    (PlumaFileBrowserWidget * obj);
void lapiz_file_browser_widget_history_forward	    (PlumaFileBrowserWidget * obj);

G_END_DECLS
#endif /* __LAPIZ_FILE_BROWSER_WIDGET_H__ */

// ex:ts=8:noet:
