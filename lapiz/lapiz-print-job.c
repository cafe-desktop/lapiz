/*
 * lapiz-print.c
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
 * $Id: lapiz-print.c 6022 2007-12-09 14:38:57Z pborelli $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <ctksourceview/ctksource.h>

#include "lapiz-print-job.h"
#include "lapiz-debug.h"
#include "lapiz-prefs-manager.h"
#include "lapiz-print-preview.h"
#include "lapiz-marshal.h"
#include "lapiz-utils.h"
#include "lapiz-dirs.h"


struct _LapizPrintJobPrivate
{
	LapizView                *view;
	LapizDocument            *doc;

	CtkPrintOperation        *operation;
	CtkSourcePrintCompositor *compositor;

	CtkPrintSettings         *settings;

	CtkWidget                *preview;

	LapizPrintJobStatus       status;

	gchar                    *status_string;

	gdouble			  progress;

	gboolean                  is_preview;

	/* widgets part of the custom print preferences widget.
	 * These pointers are valid just when the dialog is displayed */
	CtkWidget *syntax_checkbutton;
	CtkWidget *page_header_checkbutton;
	CtkWidget *line_numbers_checkbutton;
	CtkWidget *line_numbers_hbox;
	CtkWidget *line_numbers_spinbutton;
	CtkWidget *text_wrapping_checkbutton;
	CtkWidget *do_not_split_checkbutton;
	CtkWidget *fonts_table;
	CtkWidget *body_font_label;
	CtkWidget *headers_font_label;
	CtkWidget *numbers_font_label;
	CtkWidget *body_fontbutton;
	CtkWidget *headers_fontbutton;
	CtkWidget *numbers_fontbutton;
	CtkWidget *restore_button;
};

enum
{
	PROP_0,
	PROP_VIEW
};

enum
{
	PRINTING,
	SHOW_PREVIEW,
	DONE,
	LAST_SIGNAL
};

static guint print_job_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (LapizPrintJob, lapiz_print_job, G_TYPE_OBJECT)

static void
set_view (LapizPrintJob *job, LapizView *view)
{
	job->priv->view = view;
	job->priv->doc = LAPIZ_DOCUMENT (ctk_text_view_get_buffer (CTK_TEXT_VIEW (view)));
}

static void
lapiz_print_job_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	LapizPrintJob *job = LAPIZ_PRINT_JOB (object);

	switch (prop_id)
	{
		case PROP_VIEW:
			g_value_set_object (value, job->priv->view);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_print_job_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	LapizPrintJob *job = LAPIZ_PRINT_JOB (object);

	switch (prop_id)
	{
		case PROP_VIEW:
			set_view (job, g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_print_job_finalize (GObject *object)
{
	LapizPrintJob *job = LAPIZ_PRINT_JOB (object);

	g_free (job->priv->status_string);

	if (job->priv->compositor != NULL)
		g_object_unref (job->priv->compositor);

	if (job->priv->operation != NULL)
		g_object_unref (job->priv->operation);

	G_OBJECT_CLASS (lapiz_print_job_parent_class)->finalize (object);
}

static void
lapiz_print_job_class_init (LapizPrintJobClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = lapiz_print_job_get_property;
	object_class->set_property = lapiz_print_job_set_property;
	object_class->finalize = lapiz_print_job_finalize;

	g_object_class_install_property (object_class,
					 PROP_VIEW,
					 g_param_spec_object ("view",
							      "Lapiz View",
							      "Lapiz View to print",
							      LAPIZ_TYPE_VIEW,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS |
							      G_PARAM_CONSTRUCT_ONLY));

	print_job_signals[PRINTING] =
		g_signal_new ("printing",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizPrintJobClass, printing),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	print_job_signals[SHOW_PREVIEW] =
		g_signal_new ("show-preview",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizPrintJobClass, show_preview),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      CTK_TYPE_WIDGET);

	print_job_signals[DONE] =
		g_signal_new ("done",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizPrintJobClass, done),
			      NULL, NULL,
			      lapiz_marshal_VOID__UINT_POINTER,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_UINT,
			      G_TYPE_POINTER);
}

static void
line_numbers_checkbutton_toggled (CtkToggleButton *button,
				  LapizPrintJob   *job)
{
	if (ctk_toggle_button_get_active (button))
	{
		ctk_widget_set_sensitive (job->priv->line_numbers_hbox,
					  lapiz_prefs_manager_print_line_numbers_can_set ());
	}
	else
	{
		ctk_widget_set_sensitive (job->priv->line_numbers_hbox, FALSE);
	}
}

static void
wrap_mode_checkbutton_toggled (CtkToggleButton *button G_GNUC_UNUSED,
			       LapizPrintJob   *job)
{
	if (!ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (job->priv->text_wrapping_checkbutton)))
	{
		ctk_widget_set_sensitive (job->priv->do_not_split_checkbutton,
					  FALSE);
		ctk_toggle_button_set_inconsistent (
			CTK_TOGGLE_BUTTON (job->priv->do_not_split_checkbutton),
					   TRUE);
	}
	else
	{
		ctk_widget_set_sensitive (job->priv->do_not_split_checkbutton,
					  TRUE);
		ctk_toggle_button_set_inconsistent (
			CTK_TOGGLE_BUTTON (job->priv->do_not_split_checkbutton),
					   FALSE);
	}
}

static void
restore_button_clicked (CtkButton     *button G_GNUC_UNUSED,
			LapizPrintJob *job)

{
	if (lapiz_prefs_manager_print_font_body_can_set ())
	{
		gchar *font;

		font = lapiz_prefs_manager_get_default_print_font_body ();

		ctk_font_chooser_set_font (CTK_FONT_CHOOSER (job->priv->body_fontbutton),
					   font);

		g_free (font);
	}

	if (lapiz_prefs_manager_print_font_header_can_set ())
	{
		gchar *font;

		font = lapiz_prefs_manager_get_default_print_font_header ();

		ctk_font_chooser_set_font (CTK_FONT_CHOOSER (job->priv->headers_fontbutton),
					   font);

		g_free (font);
	}

	if (lapiz_prefs_manager_print_font_numbers_can_set ())
	{
		gchar *font;

		font = lapiz_prefs_manager_get_default_print_font_numbers ();

		ctk_font_chooser_set_font (CTK_FONT_CHOOSER (job->priv->numbers_fontbutton),
					   font);

		g_free (font);
	}
}

static GObject *
create_custom_widget_cb (CtkPrintOperation *operation G_GNUC_UNUSED,
			 LapizPrintJob     *job)
{
	gboolean ret;
	CtkWidget *widget;
	CtkWidget *error_widget;
	gchar *font;
	gint line_numbers;
	gboolean can_set;
	CtkWrapMode wrap_mode;
	gchar *file;
	gchar *root_objects[] = {
		"adjustment1",
		"contents",
		NULL
	};

	file = lapiz_dirs_get_ui_file ("lapiz-print-preferences.ui");
	ret = lapiz_utils_get_ui_objects (file,
					  root_objects,
					  &error_widget,
					  "contents", &widget,
					  "syntax_checkbutton", &job->priv->syntax_checkbutton,
					  "line_numbers_checkbutton", &job->priv->line_numbers_checkbutton,
					  "line_numbers_hbox", &job->priv->line_numbers_hbox,
					  "line_numbers_spinbutton", &job->priv->line_numbers_spinbutton,
					  "page_header_checkbutton", &job->priv->page_header_checkbutton,
					  "text_wrapping_checkbutton", &job->priv->text_wrapping_checkbutton,
					  "do_not_split_checkbutton", &job->priv->do_not_split_checkbutton,
					  "fonts_table", &job->priv->fonts_table,
					  "body_font_label", &job->priv->body_font_label,
					  "body_fontbutton", &job->priv->body_fontbutton,
					  "headers_font_label", &job->priv->headers_font_label,
					  "headers_fontbutton", &job->priv->headers_fontbutton,
					  "numbers_font_label", &job->priv->numbers_font_label,
					  "numbers_fontbutton", &job->priv->numbers_fontbutton,
					  "restore_button", &job->priv->restore_button,
					  NULL);
	g_free (file);

	if (!ret)
	{
		return G_OBJECT (error_widget);
	}

	/* Print syntax */
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (job->priv->syntax_checkbutton),
				      lapiz_prefs_manager_get_print_syntax_hl ());
	ctk_widget_set_sensitive (job->priv->syntax_checkbutton,
				  lapiz_prefs_manager_print_syntax_hl_can_set ());

	/* Print page headers */
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (job->priv->page_header_checkbutton),
				      lapiz_prefs_manager_get_print_header ());
	ctk_widget_set_sensitive (job->priv->page_header_checkbutton,
				  lapiz_prefs_manager_print_header_can_set ());

	/* Line numbers */
	line_numbers =  lapiz_prefs_manager_get_print_line_numbers ();
	can_set = lapiz_prefs_manager_print_line_numbers_can_set ();

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (job->priv->line_numbers_checkbutton),
				      line_numbers > 0);
	ctk_widget_set_sensitive (job->priv->line_numbers_checkbutton, can_set);

	if (line_numbers > 0)
	{
		ctk_spin_button_set_value (CTK_SPIN_BUTTON (job->priv->line_numbers_spinbutton),
					   (guint) line_numbers);
		ctk_widget_set_sensitive (job->priv->line_numbers_hbox, can_set);
	}
	else
	{
		ctk_spin_button_set_value (CTK_SPIN_BUTTON (job->priv->line_numbers_spinbutton),
					   1);
		ctk_widget_set_sensitive (job->priv->line_numbers_hbox, FALSE);
	}

	/* Text wrapping */
	wrap_mode = lapiz_prefs_manager_get_print_wrap_mode ();

	switch (wrap_mode)
	{
		case CTK_WRAP_WORD:
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (job->priv->text_wrapping_checkbutton), TRUE);
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (job->priv->do_not_split_checkbutton), TRUE);
			break;
		case CTK_WRAP_CHAR:
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (job->priv->text_wrapping_checkbutton), TRUE);
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (job->priv->do_not_split_checkbutton), FALSE);
			break;
		default:
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (job->priv->text_wrapping_checkbutton), FALSE);
			ctk_toggle_button_set_inconsistent (
				CTK_TOGGLE_BUTTON (job->priv->do_not_split_checkbutton), TRUE);
	}

	can_set = lapiz_prefs_manager_print_wrap_mode_can_set ();

	ctk_widget_set_sensitive (job->priv->text_wrapping_checkbutton, can_set);
	ctk_widget_set_sensitive (job->priv->do_not_split_checkbutton,
				  can_set && (wrap_mode != CTK_WRAP_NONE));

	/* Set initial values */
	font = lapiz_prefs_manager_get_print_font_body ();
	ctk_font_chooser_set_font (CTK_FONT_CHOOSER (job->priv->body_fontbutton),
				   font);
	g_free (font);

	font = lapiz_prefs_manager_get_print_font_header ();
	ctk_font_chooser_set_font (CTK_FONT_CHOOSER (job->priv->headers_fontbutton),
				   font);
	g_free (font);

	font = lapiz_prefs_manager_get_print_font_numbers ();
	ctk_font_chooser_set_font (CTK_FONT_CHOOSER (job->priv->numbers_fontbutton),
				   font);
	g_free (font);

	can_set = lapiz_prefs_manager_print_font_body_can_set ();
	ctk_widget_set_sensitive (job->priv->body_fontbutton, can_set);
	ctk_widget_set_sensitive (job->priv->body_font_label, can_set);

	can_set = lapiz_prefs_manager_print_font_header_can_set ();
	ctk_widget_set_sensitive (job->priv->headers_fontbutton, can_set);
	ctk_widget_set_sensitive (job->priv->headers_font_label, can_set);

	can_set = lapiz_prefs_manager_print_font_numbers_can_set ();
	ctk_widget_set_sensitive (job->priv->numbers_fontbutton, can_set);
	ctk_widget_set_sensitive (job->priv->numbers_font_label, can_set);

	g_signal_connect (job->priv->line_numbers_checkbutton,
			  "toggled",
			  G_CALLBACK (line_numbers_checkbutton_toggled),
			  job);
	g_signal_connect (job->priv->text_wrapping_checkbutton,
			  "toggled",
			  G_CALLBACK (wrap_mode_checkbutton_toggled),
			  job);
	g_signal_connect (job->priv->do_not_split_checkbutton,
			  "toggled",
			  G_CALLBACK (wrap_mode_checkbutton_toggled),
			  job);
	g_signal_connect (job->priv->restore_button,
			  "clicked",
			  G_CALLBACK (restore_button_clicked),
			  job);

	return G_OBJECT (widget);
}

static void
custom_widget_apply_cb (CtkPrintOperation *operation G_GNUC_UNUSED,
			CtkWidget         *widget G_GNUC_UNUSED,
			LapizPrintJob     *job)
{
	lapiz_prefs_manager_set_print_syntax_hl (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (job->priv->syntax_checkbutton)));

	lapiz_prefs_manager_set_print_header (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (job->priv->page_header_checkbutton)));

	if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (job->priv->line_numbers_checkbutton)))
	{
		lapiz_prefs_manager_set_print_line_numbers (
			MAX (1, ctk_spin_button_get_value_as_int (CTK_SPIN_BUTTON (job->priv->line_numbers_spinbutton))));
	}
	else
	{
		lapiz_prefs_manager_set_print_line_numbers (0);
	}

	if (!ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (job->priv->text_wrapping_checkbutton)))
	{
		lapiz_prefs_manager_set_print_wrap_mode (CTK_WRAP_NONE);
	}
	else
	{
		if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (job->priv->do_not_split_checkbutton)))
		{
			lapiz_prefs_manager_set_print_wrap_mode (CTK_WRAP_WORD);
		}
		else
		{
			lapiz_prefs_manager_set_print_wrap_mode (CTK_WRAP_CHAR);
		}
	}

	lapiz_prefs_manager_set_print_font_body (ctk_font_chooser_get_font (CTK_FONT_CHOOSER (job->priv->body_fontbutton)));
	lapiz_prefs_manager_set_print_font_header (ctk_font_chooser_get_font (CTK_FONT_CHOOSER (job->priv->headers_fontbutton)));
	lapiz_prefs_manager_set_print_font_numbers (ctk_font_chooser_get_font (CTK_FONT_CHOOSER (job->priv->numbers_fontbutton)));
}

static void
create_compositor (LapizPrintJob *job)
{
	gchar *print_font_body;
	gchar *print_font_header;
	gchar *print_font_numbers;

	/* Create and initialize print compositor */
	print_font_body = lapiz_prefs_manager_get_print_font_body ();
	print_font_header = lapiz_prefs_manager_get_print_font_header ();
	print_font_numbers = lapiz_prefs_manager_get_print_font_numbers ();

	job->priv->compositor = CTK_SOURCE_PRINT_COMPOSITOR (
					g_object_new (CTK_SOURCE_TYPE_PRINT_COMPOSITOR,
						     "buffer", CTK_SOURCE_BUFFER (job->priv->doc),
						     "tab-width", ctk_source_view_get_tab_width (CTK_SOURCE_VIEW (job->priv->view)),
						     "highlight-syntax", ctk_source_buffer_get_highlight_syntax (CTK_SOURCE_BUFFER (job->priv->doc)) &&
					   				 lapiz_prefs_manager_get_print_syntax_hl (),
						     "wrap-mode", lapiz_prefs_manager_get_print_wrap_mode (),
						     "print-line-numbers", lapiz_prefs_manager_get_print_line_numbers (),
						     "print-header", lapiz_prefs_manager_get_print_header (),
						     "print-footer", FALSE,
						     "body-font-name", print_font_body,
						     "line-numbers-font-name", print_font_numbers,
						     "header-font-name", print_font_header,
						     NULL));

	g_free (print_font_body);
	g_free (print_font_header);
	g_free (print_font_numbers);

	if (lapiz_prefs_manager_get_print_header ())
	{
		gchar *doc_name;
		gchar *name_to_display;
		gchar *left;

		doc_name = lapiz_document_get_uri_for_display (job->priv->doc);
		name_to_display = lapiz_utils_str_middle_truncate (doc_name, 60);

		left = g_strdup_printf (_("File: %s"), name_to_display);

		/* Translators: %N is the current page number, %Q is the total
		 * number of pages (ex. Page 2 of 10)
		 */
		ctk_source_print_compositor_set_header_format (job->priv->compositor,
							       TRUE,
							       left,
							       NULL,
							       _("Page %N of %Q"));

		g_free (doc_name);
		g_free (name_to_display);
		g_free (left);
	}
}

static void
begin_print_cb (CtkPrintOperation *operation G_GNUC_UNUSED,
	        CtkPrintContext   *context G_GNUC_UNUSED,
	        LapizPrintJob     *job)
{
	create_compositor (job);

	job->priv->status = LAPIZ_PRINT_JOB_STATUS_PAGINATING;

	job->priv->progress = 0.0;

	g_signal_emit (job, print_job_signals[PRINTING], 0, job->priv->status);
}

static void
preview_ready (CtkPrintOperationPreview *ctk_preview G_GNUC_UNUSED,
	       CtkPrintContext          *context G_GNUC_UNUSED,
	       LapizPrintJob            *job)
{
	job->priv->is_preview = TRUE;

	g_signal_emit (job, print_job_signals[SHOW_PREVIEW], 0, job->priv->preview);
}

static void
preview_destroyed (CtkWidget                *preview G_GNUC_UNUSED,
		   CtkPrintOperationPreview *ctk_preview)
{
	ctk_print_operation_preview_end_preview (ctk_preview);
}

static gboolean
preview_cb (CtkPrintOperation        *op,
	    CtkPrintOperationPreview *ctk_preview,
	    CtkPrintContext          *context,
	    CtkWindow                *parent G_GNUC_UNUSED,
	    LapizPrintJob            *job)
{
	job->priv->preview = lapiz_print_preview_new (op, ctk_preview, context);

	g_signal_connect_after (ctk_preview,
			        "ready",
				G_CALLBACK (preview_ready),
				job);

	/* FIXME: should this go in the preview widget itself? */
	g_signal_connect (job->priv->preview,
			  "destroy",
			  G_CALLBACK (preview_destroyed),
			  ctk_preview);

	return TRUE;
}

static gboolean
paginate_cb (CtkPrintOperation *operation G_GNUC_UNUSED,
	     CtkPrintContext   *context,
	     LapizPrintJob     *job)
{
	gboolean res;

	job->priv->status = LAPIZ_PRINT_JOB_STATUS_PAGINATING;

	res = ctk_source_print_compositor_paginate (job->priv->compositor, context);

	if (res)
	{
		gint n_pages;

		n_pages = ctk_source_print_compositor_get_n_pages (job->priv->compositor);
		ctk_print_operation_set_n_pages (job->priv->operation, n_pages);
	}

	job->priv->progress = ctk_source_print_compositor_get_pagination_progress (job->priv->compositor);

	/* When previewing, the progress is just for pagination, when printing
	 * it's split between pagination and rendering */
	if (!job->priv->is_preview)
		job->priv->progress /= 2.0;

	g_signal_emit (job, print_job_signals[PRINTING], 0, job->priv->status);

	return res;
}

static void
draw_page_cb (CtkPrintOperation *operation G_GNUC_UNUSED,
	      CtkPrintContext   *context,
	      gint               page_nr,
	      LapizPrintJob     *job)
{
	gint n_pages;

	/* In preview, pages are drawn on the fly, so rendering is
	 * not part of the progress */
	if (!job->priv->is_preview)
	{
		g_free (job->priv->status_string);

		n_pages = ctk_source_print_compositor_get_n_pages (job->priv->compositor);

		job->priv->status = LAPIZ_PRINT_JOB_STATUS_DRAWING;

		job->priv->status_string = g_strdup_printf ("Rendering page %d of %d...",
							    page_nr + 1,
							    n_pages);

		job->priv->progress = page_nr / (2.0 * n_pages) + 0.5;

		g_signal_emit (job, print_job_signals[PRINTING], 0, job->priv->status);
	}

	ctk_source_print_compositor_draw_page (job->priv->compositor, context, page_nr);
}

static void
end_print_cb (CtkPrintOperation *operation G_GNUC_UNUSED,
	      CtkPrintContext   *context G_GNUC_UNUSED,
	      LapizPrintJob     *job)
{
	g_object_unref (job->priv->compositor);
	job->priv->compositor = NULL;
}

static void
done_cb (CtkPrintOperation       *operation,
	 CtkPrintOperationResult  result,
	 LapizPrintJob           *job)
{
	GError *error = NULL;
	LapizPrintJobResult print_result;

	switch (result)
	{
		case CTK_PRINT_OPERATION_RESULT_CANCEL:
			print_result = LAPIZ_PRINT_JOB_RESULT_CANCEL;
			break;

		case CTK_PRINT_OPERATION_RESULT_APPLY:
			print_result = LAPIZ_PRINT_JOB_RESULT_OK;
			break;

		case CTK_PRINT_OPERATION_RESULT_ERROR:
			print_result = LAPIZ_PRINT_JOB_RESULT_ERROR;
			ctk_print_operation_get_error (operation, &error);
			break;

		default:
			g_return_if_reached ();
	}

	/* Avoid job is destroyed in the handler of the "done" message */
	g_object_ref (job);

	g_signal_emit (job, print_job_signals[DONE], 0, print_result, error);

	g_object_unref (operation);
	job->priv->operation = NULL;

	g_object_unref (job);
}

/* Note that lapiz_print_job_print can can only be called once on a given LapizPrintJob */
CtkPrintOperationResult
lapiz_print_job_print (LapizPrintJob            *job,
		       CtkPrintOperationAction   action,
		       CtkPageSetup             *page_setup,
		       CtkPrintSettings         *settings,
		       CtkWindow                *parent,
		       GError                  **error)
{
	LapizPrintJobPrivate *priv;
	gchar *job_name;

	g_return_val_if_fail (job->priv->compositor == NULL, CTK_PRINT_OPERATION_RESULT_ERROR);

	priv = job->priv;

	/* Check if we are previewing */
	priv->is_preview = (action == CTK_PRINT_OPERATION_ACTION_PREVIEW);

	/* Create print operation */
	job->priv->operation = ctk_print_operation_new ();

	if (settings)
		ctk_print_operation_set_print_settings (priv->operation,
							settings);

	if (page_setup != NULL)
		ctk_print_operation_set_default_page_setup (priv->operation,
							    page_setup);

	job_name = lapiz_document_get_short_name_for_display (priv->doc);
	ctk_print_operation_set_job_name (priv->operation, job_name);
	g_free (job_name);

	ctk_print_operation_set_embed_page_setup (priv->operation, TRUE);

	ctk_print_operation_set_custom_tab_label (priv->operation,
						  _("Text Editor"));

	ctk_print_operation_set_allow_async (priv->operation, TRUE);

	g_signal_connect (priv->operation,
			  "create-custom-widget",
			  G_CALLBACK (create_custom_widget_cb),
			  job);
	g_signal_connect (priv->operation,
			  "custom-widget-apply",
			  G_CALLBACK (custom_widget_apply_cb),
			  job);
  	g_signal_connect (priv->operation,
			  "begin-print",
			  G_CALLBACK (begin_print_cb),
			  job);
	g_signal_connect (priv->operation,
			  "preview",
			  G_CALLBACK (preview_cb),
			  job);
  	g_signal_connect (priv->operation,
			  "paginate",
			  G_CALLBACK (paginate_cb),
			  job);
	g_signal_connect (priv->operation,
			  "draw-page",
			  G_CALLBACK (draw_page_cb),
			  job);
	g_signal_connect (priv->operation,
			  "end-print",
			  G_CALLBACK (end_print_cb),
			  job);
	g_signal_connect (priv->operation,
			  "done",
			  G_CALLBACK (done_cb),
			  job);

	return ctk_print_operation_run (priv->operation,
					action,
					parent,
					error);
}

static void
lapiz_print_job_init (LapizPrintJob *job)
{
	job->priv = lapiz_print_job_get_instance_private (job);

	job->priv->status = LAPIZ_PRINT_JOB_STATUS_INIT;

	job->priv->status_string = g_strdup (_("Preparing..."));
}

LapizPrintJob *
lapiz_print_job_new (LapizView *view)
{
	LapizPrintJob *job;

	g_return_val_if_fail (LAPIZ_IS_VIEW (view), NULL);

	job = LAPIZ_PRINT_JOB (g_object_new (LAPIZ_TYPE_PRINT_JOB,
					     "view", view,
					      NULL));

	return job;
}

void
lapiz_print_job_cancel (LapizPrintJob *job)
{
	g_return_if_fail (LAPIZ_IS_PRINT_JOB (job));

	ctk_print_operation_cancel (job->priv->operation);
}

const gchar *
lapiz_print_job_get_status_string (LapizPrintJob *job)
{
	g_return_val_if_fail (LAPIZ_IS_PRINT_JOB (job), NULL);
	g_return_val_if_fail (job->priv->status_string != NULL, NULL);

	return job->priv->status_string;
}

gdouble
lapiz_print_job_get_progress (LapizPrintJob *job)
{
	g_return_val_if_fail (LAPIZ_IS_PRINT_JOB (job), 0.0);

	return job->priv->progress;
}

CtkPrintSettings *
lapiz_print_job_get_print_settings (LapizPrintJob *job)
{
	g_return_val_if_fail (LAPIZ_IS_PRINT_JOB (job), NULL);

	return ctk_print_operation_get_print_settings (job->priv->operation);
}

CtkPageSetup *
lapiz_print_job_get_page_setup (LapizPrintJob *job)
{
	g_return_val_if_fail (LAPIZ_IS_PRINT_JOB (job), NULL);

	return ctk_print_operation_get_default_page_setup (job->priv->operation);
}
