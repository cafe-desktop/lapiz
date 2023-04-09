/*
 * lapiz-print-job.h
 * This file is part of lapiz
 *
 * Copyright (C) 2000-2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2008 Paolo Maggi
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
 * Modified by the lapiz Team, 1998-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __LAPIZ_PRINT_JOB_H__
#define __LAPIZ_PRINT_JOB_H__

#include <gtk/gtk.h>
#include <lapiz/lapiz-view.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_PRINT_JOB              (lapiz_print_job_get_type())
#define LAPIZ_PRINT_JOB(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_PRINT_JOB, LapizPrintJob))
#define LAPIZ_PRINT_JOB_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_PRINT_JOB, LapizPrintJobClass))
#define LAPIZ_IS_PRINT_JOB(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_PRINT_JOB))
#define LAPIZ_IS_PRINT_JOB_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_PRINT_JOB))
#define LAPIZ_PRINT_JOB_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_PRINT_JOB, LapizPrintJobClass))


typedef enum
{
	LAPIZ_PRINT_JOB_STATUS_INIT,
	LAPIZ_PRINT_JOB_STATUS_PAGINATING,
	LAPIZ_PRINT_JOB_STATUS_DRAWING,
	LAPIZ_PRINT_JOB_STATUS_DONE
} LapizPrintJobStatus;

typedef enum
{
	LAPIZ_PRINT_JOB_RESULT_OK,
	LAPIZ_PRINT_JOB_RESULT_CANCEL,
	LAPIZ_PRINT_JOB_RESULT_ERROR
} LapizPrintJobResult;

/* Private structure type */
typedef struct _LapizPrintJobPrivate LapizPrintJobPrivate;

/*
 * Main object structure
 */
typedef struct _LapizPrintJob LapizPrintJob;


struct _LapizPrintJob
{
	GObject parent;

	/* <private> */
	LapizPrintJobPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizPrintJobClass LapizPrintJobClass;

struct _LapizPrintJobClass
{
	GObjectClass parent_class;

        /* Signals */
	void (* printing) (LapizPrintJob       *job,
	                   LapizPrintJobStatus  status);

	void (* show_preview) (LapizPrintJob   *job,
	                       GtkWidget       *preview);

        void (*done)      (LapizPrintJob       *job,
		           LapizPrintJobResult  result,
                           const GError        *error);
};

/*
 * Public methods
 */
GType			 lapiz_print_job_get_type		(void) G_GNUC_CONST;

LapizPrintJob		*lapiz_print_job_new			(LapizView                *view);

void			 lapiz_print_job_set_export_filename	(LapizPrintJob            *job,
								 const gchar              *filename);

GtkPrintOperationResult	 lapiz_print_job_print			(LapizPrintJob            *job,
								 GtkPrintOperationAction   action,
								 GtkPageSetup             *page_setup,
								 GtkPrintSettings         *settings,
								 GtkWindow                *parent,
								 GError                  **error);

void			 lapiz_print_job_cancel			(LapizPrintJob            *job);

const gchar		*lapiz_print_job_get_status_string	(LapizPrintJob            *job);

gdouble			 lapiz_print_job_get_progress		(LapizPrintJob            *job);

GtkPrintSettings	*lapiz_print_job_get_print_settings	(LapizPrintJob            *job);

GtkPageSetup		*lapiz_print_job_get_page_setup		(LapizPrintJob            *job);

G_END_DECLS

#endif /* __LAPIZ_PRINT_JOB_H__ */
