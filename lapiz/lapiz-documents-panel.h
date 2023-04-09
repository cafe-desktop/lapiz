/*
 * lapiz-documents-panel.h
 * This file is part of lapiz
 *
 * Copyright (C) 2005 - Paolo Maggi
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

#ifndef __LAPIZ_DOCUMENTS_PANEL_H__
#define __LAPIZ_DOCUMENTS_PANEL_H__

#include <gtk/gtk.h>

#include <lapiz/lapiz-window.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_DOCUMENTS_PANEL              (lapiz_documents_panel_get_type())
#define LAPIZ_DOCUMENTS_PANEL(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_DOCUMENTS_PANEL, LapizDocumentsPanel))
#define LAPIZ_DOCUMENTS_PANEL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_DOCUMENTS_PANEL, LapizDocumentsPanelClass))
#define LAPIZ_IS_DOCUMENTS_PANEL(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_DOCUMENTS_PANEL))
#define LAPIZ_IS_DOCUMENTS_PANEL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_DOCUMENTS_PANEL))
#define LAPIZ_DOCUMENTS_PANEL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_DOCUMENTS_PANEL, LapizDocumentsPanelClass))

/* Private structure type */
typedef struct _LapizDocumentsPanelPrivate LapizDocumentsPanelPrivate;

/*
 * Main object structure
 */
typedef struct _LapizDocumentsPanel LapizDocumentsPanel;

struct _LapizDocumentsPanel
{
	GtkBox vbox;

	/*< private > */
	LapizDocumentsPanelPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizDocumentsPanelClass LapizDocumentsPanelClass;

struct _LapizDocumentsPanelClass
{
	GtkBoxClass parent_class;
};

/*
 * Public methods
 */
GType 		 lapiz_documents_panel_get_type	(void) G_GNUC_CONST;

GtkWidget	*lapiz_documents_panel_new 	(LapizWindow *window);

G_END_DECLS

#endif  /* __LAPIZ_DOCUMENTS_PANEL_H__  */
