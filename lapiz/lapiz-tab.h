/*
 * lapiz-tab.h
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

#ifndef __LAPIZ_TAB_H__
#define __LAPIZ_TAB_H__

#include <ctk/ctk.h>

#include <lapiz/lapiz-view.h>
#include <lapiz/lapiz-document.h>

G_BEGIN_DECLS

typedef enum
{
	LAPIZ_TAB_STATE_NORMAL = 0,
	LAPIZ_TAB_STATE_LOADING,
	LAPIZ_TAB_STATE_REVERTING,
	LAPIZ_TAB_STATE_SAVING,
	LAPIZ_TAB_STATE_PRINTING,
	LAPIZ_TAB_STATE_PRINT_PREVIEWING,
	LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW,
	LAPIZ_TAB_STATE_GENERIC_NOT_EDITABLE,
	LAPIZ_TAB_STATE_LOADING_ERROR,
	LAPIZ_TAB_STATE_REVERTING_ERROR,
	LAPIZ_TAB_STATE_SAVING_ERROR,
	LAPIZ_TAB_STATE_GENERIC_ERROR,
	LAPIZ_TAB_STATE_CLOSING,
	LAPIZ_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION,
	LAPIZ_TAB_NUM_OF_STATES /* This is not a valid state */
} LapizTabState;

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_TAB              (lapiz_tab_get_type())
#define LAPIZ_TAB(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_TAB, LapizTab))
#define LAPIZ_TAB_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_TAB, LapizTabClass))
#define LAPIZ_IS_TAB(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_TAB))
#define LAPIZ_IS_TAB_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_TAB))
#define LAPIZ_TAB_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_TAB, LapizTabClass))

/* Private structure type */
typedef struct _LapizTabPrivate LapizTabPrivate;

/*
 * Main object structure
 */
typedef struct _LapizTab LapizTab;

struct _LapizTab
{
	GtkBox vbox;

	/*< private > */
	LapizTabPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizTabClass LapizTabClass;

struct _LapizTabClass
{
	GtkBoxClass parent_class;
};

/*
 * Public methods
 */
GType 		 lapiz_tab_get_type 		(void) G_GNUC_CONST;

LapizView	*lapiz_tab_get_view		(LapizTab            *tab);

/* This is only an helper function */
LapizDocument	*lapiz_tab_get_document		(LapizTab            *tab);

LapizTab	*lapiz_tab_get_from_document	(LapizDocument       *doc);

LapizTabState	 lapiz_tab_get_state		(LapizTab	     *tab);

gboolean	 lapiz_tab_get_auto_save_enabled
						(LapizTab            *tab);

void		 lapiz_tab_set_auto_save_enabled
						(LapizTab            *tab,
						 gboolean            enable);

gint		 lapiz_tab_get_auto_save_interval
						(LapizTab            *tab);

void		 lapiz_tab_set_auto_save_interval
						(LapizTab            *tab,
						 gint                interval);

void		 lapiz_tab_set_info_bar		(LapizTab            *tab,
						 GtkWidget           *info_bar);
/*
 * Non exported methods
 */
GtkWidget 	*_lapiz_tab_new 		(void);

/* Whether create is TRUE, creates a new empty document if location does
   not refer to an existing file */
GtkWidget	*_lapiz_tab_new_from_uri	(const gchar         *uri,
						 const LapizEncoding *encoding,
						 gint                 line_pos,
						 gboolean             create);
gchar 		*_lapiz_tab_get_name		(LapizTab            *tab);
gchar 		*_lapiz_tab_get_tooltips	(LapizTab            *tab);
GdkPixbuf 	*_lapiz_tab_get_icon		(LapizTab            *tab);
void		 _lapiz_tab_load		(LapizTab            *tab,
						 const gchar         *uri,
						 const LapizEncoding *encoding,
						 gint                 line_pos,
						 gboolean             create);
void		 _lapiz_tab_revert		(LapizTab            *tab);
void		 _lapiz_tab_save		(LapizTab            *tab);
void		 _lapiz_tab_save_as		(LapizTab            *tab,
						 const gchar         *uri,
						 const LapizEncoding *encoding,
						 LapizDocumentNewlineType newline_type);

void		 _lapiz_tab_print		(LapizTab            *tab);
void		 _lapiz_tab_print_preview	(LapizTab            *tab);

void		 _lapiz_tab_mark_for_closing	(LapizTab	     *tab);

gboolean	 _lapiz_tab_can_close		(LapizTab	     *tab);

G_END_DECLS

#endif  /* __LAPIZ_TAB_H__  */
