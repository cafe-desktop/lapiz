/*
 * lapiz-app.h
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

#ifndef __LAPIZ_APP_H__
#define __LAPIZ_APP_H__

#include <gtk/gtk.h>

#include <lapiz/lapiz-window.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_APP              (lapiz_app_get_type())
#define LAPIZ_APP(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_APP, LapizApp))
#define LAPIZ_APP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_APP, LapizAppClass))
#define LAPIZ_IS_APP(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_APP))
#define LAPIZ_IS_APP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_APP))
#define LAPIZ_APP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_APP, LapizAppClass))

/* Private structure type */
typedef struct _LapizAppPrivate LapizAppPrivate;

/*
 * Main object structure
 */
typedef struct _LapizApp LapizApp;

struct _LapizApp
{
	GObject object;

	/*< private > */
	LapizAppPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizAppClass LapizAppClass;

struct _LapizAppClass
{
	GObjectClass parent_class;
};

/*
 * Lockdown mask definition
 */
typedef enum
{
	LAPIZ_LOCKDOWN_COMMAND_LINE	= 1 << 0,
	LAPIZ_LOCKDOWN_PRINTING		= 1 << 1,
	LAPIZ_LOCKDOWN_PRINT_SETUP	= 1 << 2,
	LAPIZ_LOCKDOWN_SAVE_TO_DISK	= 1 << 3,
	LAPIZ_LOCKDOWN_ALL		= 0xF
} LapizLockdownMask;

/*
 * Public methods
 */
GType 		 lapiz_app_get_type 			(void) G_GNUC_CONST;

LapizApp 	*lapiz_app_get_default			(void);

LapizWindow	*lapiz_app_create_window		(LapizApp  *app,
							 GdkScreen *screen);

const GList	*lapiz_app_get_windows			(LapizApp *app);
LapizWindow	*lapiz_app_get_active_window		(LapizApp *app);

/* Returns a newly allocated list with all the documents */
GList		*lapiz_app_get_documents		(LapizApp *app);

/* Returns a newly allocated list with all the views */
GList		*lapiz_app_get_views			(LapizApp *app);

/* Lockdown state */
LapizLockdownMask lapiz_app_get_lockdown		(LapizApp *app);

/*
 * Non exported functions
 */
LapizWindow	*_lapiz_app_restore_window		(LapizApp    *app,
							 const gchar *role);
LapizWindow	*_lapiz_app_get_window_in_viewport	(LapizApp     *app,
							 GdkScreen    *screen,
							 gint          workspace,
							 gint          viewport_x,
							 gint          viewport_y);
void		 _lapiz_app_set_lockdown		(LapizApp          *app,
							 LapizLockdownMask  lockdown);
void		 _lapiz_app_set_lockdown_bit		(LapizApp          *app,
							 LapizLockdownMask  bit,
							 gboolean           value);
/*
 * This one is a lapiz-window function, but we declare it here to avoid
 * #include headaches since it needs the LapizLockdownMask declaration.
 */
void		 _lapiz_window_set_lockdown		(LapizWindow         *window,
							 LapizLockdownMask    lockdown);

/* global print config */
GtkPageSetup		*_lapiz_app_get_default_page_setup	(LapizApp         *app);
void			 _lapiz_app_set_default_page_setup	(LapizApp         *app,
								 GtkPageSetup     *page_setup);
GtkPrintSettings	*_lapiz_app_get_default_print_settings	(LapizApp         *app);
void			 _lapiz_app_set_default_print_settings	(LapizApp         *app,
								 GtkPrintSettings *settings);

G_END_DECLS

#endif  /* __LAPIZ_APP_H__  */
