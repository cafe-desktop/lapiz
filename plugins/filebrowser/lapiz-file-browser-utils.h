#ifndef __PLUMA_FILE_BROWSER_UTILS_H__
#define __PLUMA_FILE_BROWSER_UTILS_H__

#include <lapiz/lapiz-window.h>
#include <gio/gio.h>

GdkPixbuf *lapiz_file_browser_utils_pixbuf_from_theme     (gchar const *name,
                                                           GtkIconSize size);

GdkPixbuf *lapiz_file_browser_utils_pixbuf_from_icon	  (GIcon * icon,
                                                           GtkIconSize size);
GdkPixbuf *lapiz_file_browser_utils_pixbuf_from_file	  (GFile * file,
                                                           GtkIconSize size);

gchar * lapiz_file_browser_utils_file_basename		  (GFile * file);
gchar * lapiz_file_browser_utils_uri_basename             (gchar const * uri);

gboolean lapiz_file_browser_utils_confirmation_dialog     (PlumaWindow * window,
                                                           GtkMessageType type,
                                                           gchar const *message,
                                                           gchar const *secondary);

#endif /* __PLUMA_FILE_BROWSER_UTILS_H__ */

// ex:ts=8:noet:
