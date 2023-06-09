/*
 * lapiz-help.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "lapiz-help.h"

#include <glib/gi18n.h>
#include <string.h>
#include <ctk/ctk.h>

gboolean
lapiz_help_display (CtkWindow   *parent,
		    const gchar *name, /* "lapiz" if NULL */
		    const gchar *link_id)
{
	GError *error = NULL;
	gboolean ret;
	gchar *link;

	g_return_val_if_fail ((parent == NULL) || CTK_IS_WINDOW (parent), FALSE);

	if (name == NULL)
		name = "lapiz";
	else if (strcmp (name, "lapiz.xml") == 0)
	{
		g_warning ("%s: Using \"lapiz.xml\" for the help name is deprecated, use \"lapiz\" or simply NULL instead", G_STRFUNC);

		name = "lapiz";
	}

	if (link_id)
		link = g_strdup_printf ("help:%s/%s", name, link_id);
	else
		link = g_strdup_printf ("help:%s", name);

	ret = ctk_show_uri_on_window (parent,
	                              link,
	                              CDK_CURRENT_TIME,
	                              &error);

	g_free (link);

	if (error != NULL)
	{
		CtkWidget *dialog;

		dialog = ctk_message_dialog_new (parent,
						 CTK_DIALOG_DESTROY_WITH_PARENT,
						 CTK_MESSAGE_ERROR,
						 CTK_BUTTONS_CLOSE,
						 _("There was an error displaying the help."));

		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
							  "%s", error->message);

		g_signal_connect (G_OBJECT (dialog),
				  "response",
				  G_CALLBACK (ctk_widget_destroy),
				  NULL);

		ctk_window_set_resizable (CTK_WINDOW (dialog), FALSE);

		ctk_widget_show (dialog);

		g_error_free (error);
	}

	return ret;
}
