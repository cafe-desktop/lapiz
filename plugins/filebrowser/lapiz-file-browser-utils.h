#ifndef __LAPIZ_FILE_BROWSER_UTILS_H__
#define __LAPIZ_FILE_BROWSER_UTILS_H__

#include <lapiz/lapiz-window.h>
#include <gio/gio.h>

CdkPixbuf *lapiz_file_browser_utils_pixbuf_from_theme     (gchar const *name,
                                                           CtkIconSize size);

CdkPixbuf *lapiz_file_browser_utils_pixbuf_from_icon	  (GIcon * icon,
                                                           CtkIconSize size);
CdkPixbuf *lapiz_file_browser_utils_pixbuf_from_file	  (GFile * file,
                                                           CtkIconSize size);

gchar * lapiz_file_browser_utils_file_basename		  (GFile * file);
gchar * lapiz_file_browser_utils_uri_basename             (gchar const * uri);

gboolean lapiz_file_browser_utils_confirmation_dialog     (LapizWindow * window,
                                                           CtkMessageType type,
                                                           gchar const *message,
                                                           gchar const *secondary);

#endif /* __LAPIZ_FILE_BROWSER_UTILS_H__ */

// ex:ts=8:noet:
