/*
 * lapiz-close-button.c
 * This file is part of lapiz
 *
 * Copyright (C) 2010 - Paolo Borelli
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

#include "lapiz-close-button.h"

G_DEFINE_TYPE (LapizCloseButton, lapiz_close_button, CTK_TYPE_BUTTON)

static void
lapiz_close_button_class_init (LapizCloseButtonClass *klass)
{
}

static void
lapiz_close_button_init (LapizCloseButton *button)
{
	CtkWidget *image;
	CtkCssProvider *css;
	GError *error = NULL;
	const gchar button_style[] =
		"* {\n"
		"	padding: 0;\n"
		"}";

	image = ctk_image_new_from_icon_name ("ctk-close",
	                                      CTK_ICON_SIZE_MENU);
	ctk_widget_show (image);

	ctk_container_add (CTK_CONTAINER (button), image);

	/* make it as small as possible */
	css = ctk_css_provider_new ();
	if (ctk_css_provider_load_from_data (css, button_style,
	                                     -1, &error))
	{
		CtkStyleContext *context;

		context = ctk_widget_get_style_context (CTK_WIDGET (button));
		ctk_style_context_add_provider (context, CTK_STYLE_PROVIDER (css),
			                        CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref (css);
	}
	else
	{
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

CtkWidget *
lapiz_close_button_new ()
{
	return CTK_WIDGET (g_object_new (LAPIZ_TYPE_CLOSE_BUTTON,
	                                 "relief", CTK_RELIEF_NONE,
	                                 "focus-on-click", FALSE,
	                                 NULL));
}

