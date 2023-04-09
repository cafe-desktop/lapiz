#ifndef __LAPIZ_MESSAGE_TYPE_H__
#define __LAPIZ_MESSAGE_TYPE_H__

#include <glib-object.h>
#include <stdarg.h>

#include "lapiz-message.h"

G_BEGIN_DECLS

#define LAPIZ_TYPE_MESSAGE_TYPE			(lapiz_message_type_get_type ())
#define LAPIZ_MESSAGE_TYPE(x)			((LapizMessageType *)(x))

typedef void (*LapizMessageTypeForeach)		(const gchar *key,
						 GType 	      type,
						 gboolean     required,
						 gpointer     user_data);

typedef struct _LapizMessageType			LapizMessageType;

GType lapiz_message_type_get_type 		 (void) G_GNUC_CONST;

gboolean lapiz_message_type_is_supported 	 (GType type);
gchar *lapiz_message_type_identifier		 (const gchar *object_path,
						  const gchar *method);
gboolean lapiz_message_type_is_valid_object_path (const gchar *object_path);

LapizMessageType *lapiz_message_type_new	 (const gchar *object_path,
						  const gchar *method,
						  guint	      num_optional,
						  ...) G_GNUC_NULL_TERMINATED;
LapizMessageType *lapiz_message_type_new_valist	 (const gchar *object_path,
						  const gchar *method,
						  guint	      num_optional,
						  va_list      va_args);

void lapiz_message_type_set			 (LapizMessageType *message_type,
						  guint		   num_optional,
						  ...) G_GNUC_NULL_TERMINATED;
void lapiz_message_type_set_valist		 (LapizMessageType *message_type,
						  guint		   num_optional,
						  va_list	           va_args);

LapizMessageType *lapiz_message_type_ref 	 (LapizMessageType *message_type);
void lapiz_message_type_unref			 (LapizMessageType *message_type);


LapizMessage *lapiz_message_type_instantiate_valist (LapizMessageType *message_type,
				       		     va_list	      va_args);
LapizMessage *lapiz_message_type_instantiate 	 (LapizMessageType *message_type,
				       		  ...) G_GNUC_NULL_TERMINATED;

const gchar *lapiz_message_type_get_object_path	 (LapizMessageType *message_type);
const gchar *lapiz_message_type_get_method	 (LapizMessageType *message_type);

GType lapiz_message_type_lookup			 (LapizMessageType *message_type,
						  const gchar      *key);

void lapiz_message_type_foreach 		 (LapizMessageType 	  *message_type,
						  LapizMessageTypeForeach  func,
						  gpointer	   	   user_data);

G_END_DECLS

#endif /* __LAPIZ_MESSAGE_TYPE_H__ */

// ex:ts=8:noet:
