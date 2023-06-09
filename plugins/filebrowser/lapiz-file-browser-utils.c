/*
 * lapiz-file-bookmarks-store.c - Lapiz plugin providing easy file access
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

#include "lapiz-file-browser-utils.h"
#include <lapiz/lapiz-utils.h>

#include <glib/gi18n.h>


static GdkPixbuf *
process_icon_pixbuf (GdkPixbuf * pixbuf,
		     gchar const * name,
		     gint size,
		     GError * error)
{
	GdkPixbuf * scale;

	if (error != NULL) {
		g_warning ("Could not load theme icon %s: %s",
			   name,
			   error->message);
		g_error_free (error);
	}

	if (pixbuf && gdk_pixbuf_get_width (pixbuf) > size) {
		scale = gdk_pixbuf_scale_simple (pixbuf,
		                                 size,
		                                 size,
		                                 GDK_INTERP_BILINEAR);
		g_object_unref (pixbuf);
		pixbuf = scale;
	}

	return pixbuf;
}

GdkPixbuf *
lapiz_file_browser_utils_pixbuf_from_theme (gchar const * name,
                                            CtkIconSize size)
{
	gint width;
	GError *error = NULL;
	GdkPixbuf *pixbuf;

	ctk_icon_size_lookup (size, &width, NULL);

	pixbuf = ctk_icon_theme_load_icon (ctk_icon_theme_get_default (),
					   name,
					   width,
					   0,
					   &error);

	pixbuf = process_icon_pixbuf (pixbuf, name, width, error);

	return pixbuf;
}

GdkPixbuf *
lapiz_file_browser_utils_pixbuf_from_icon (GIcon * icon,
                                           CtkIconSize size)
{
	GdkPixbuf * ret = NULL;
	CtkIconTheme *theme;
	CtkIconInfo *info;
	gint width;

	if (!icon)
		return NULL;

	theme = ctk_icon_theme_get_default ();
	ctk_icon_size_lookup (size, &width, NULL);

	info = ctk_icon_theme_lookup_by_gicon (theme,
					       icon,
					       width,
					       CTK_ICON_LOOKUP_USE_BUILTIN);

	if (!info)
		return NULL;

	ret = ctk_icon_info_load_icon (info, NULL);
	g_object_unref (info);

	return ret;
}

GdkPixbuf *
lapiz_file_browser_utils_pixbuf_from_file (GFile * file,
                                           CtkIconSize size)
{
	GIcon * icon;
	GFileInfo * info;
	GdkPixbuf * ret = NULL;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_ICON,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  NULL);

	if (!info)
		return NULL;

	icon = g_file_info_get_icon (info);
	if (icon != NULL)
		ret = lapiz_file_browser_utils_pixbuf_from_icon (icon, size);

	g_object_unref (info);

	return ret;
}

gchar *
lapiz_file_browser_utils_file_basename (GFile * file)
{
	gchar *uri;
	gchar *ret;

	uri = g_file_get_uri (file);
	ret = lapiz_file_browser_utils_uri_basename (uri);
	g_free (uri);

	return ret;
}

gchar *
lapiz_file_browser_utils_uri_basename (gchar const * uri)
{
	return lapiz_utils_basename_for_display (uri);
}

gboolean
lapiz_file_browser_utils_confirmation_dialog (LapizWindow * window,
                                              CtkMessageType type,
                                              gchar const *message,
                                              gchar const *secondary)
{
	CtkWidget *dlg;
	gint ret;
	CtkWidget *button;

	dlg = ctk_message_dialog_new (CTK_WINDOW (window),
				      CTK_DIALOG_MODAL |
				      CTK_DIALOG_DESTROY_WITH_PARENT,
				      type,
				      CTK_BUTTONS_NONE, "%s", message);

	if (secondary)
		ctk_message_dialog_format_secondary_text
		    (CTK_MESSAGE_DIALOG (dlg), "%s", secondary);

	/* Add a cancel button */
	button = ctk_button_new_with_mnemonic (_("_Cancel"));
	ctk_button_set_image (CTK_BUTTON (button), ctk_image_new_from_icon_name ("process-stop", CTK_ICON_SIZE_BUTTON));

	ctk_widget_show (button);

	ctk_widget_set_can_default (button, TRUE);
	ctk_dialog_add_action_widget (CTK_DIALOG (dlg),
                                      button,
                                      CTK_RESPONSE_CANCEL);

	/* Add delete button */
	button = ctk_button_new_with_mnemonic (_("_Delete"));
	ctk_button_set_image (CTK_BUTTON (button), ctk_image_new_from_icon_name ("edit-delete", CTK_ICON_SIZE_BUTTON));

	ctk_widget_show (button);
	ctk_widget_set_can_default (button, TRUE);
	ctk_dialog_add_action_widget (CTK_DIALOG (dlg),
                                      button,
                                      CTK_RESPONSE_OK);

	ret = ctk_dialog_run (CTK_DIALOG (dlg));
	ctk_widget_destroy (dlg);

	return (ret == CTK_RESPONSE_OK);
}

// ex:ts=8:noet:
