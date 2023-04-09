#ifndef __LAPIZ_MESSAGE_TYPE_H__
#define __LAPIZ_MESSAGE_TYPE_H__

#include <glib-object.h>
#include <stdarg.h>

#include "lapiz-message.h"

G_BEGIN_DECLS

#define LAPIZ_TYPE_MESSAGE_TYPE			(lapiz_message_type_get_type ())
#define LAPIZ_MESSAGE_TYPE(x)			((PlumaMessageType *)(x))

typedef void (*PlumaMessageTypeForeach)		(const gchar *key,
						 GType 	      type,
						 gboolean     required,
						 gpointer     user_data);

typedef struct _PlumaMessageType			PlumaMessageType;

GType lapiz_message_type_get_type 		 (void) G_GNUC_CONST;

gboolean lapiz_message_type_is_supported 	 (GType type);
gchar *lapiz_message_type_identifier		 (const gchar *object_path,
						  const gchar *method);
gboolean lapiz_message_type_is_valid_object_path (const gchar *object_path);

PlumaMessageType *lapiz_message_type_new	 (const gchar *object_path,
						  const gchar *method,
						  guint	      num_optional,
						  ...) G_GNUC_NULL_TERMINATED;
PlumaMessageType *lapiz_message_type_new_valist	 (const gchar *object_path,
						  const gchar *method,
						  guint	      num_optional,
						  va_list      va_args);

void lapiz_message_type_set			 (PlumaMessageType *message_type,
						  guint		   num_optional,
						  ...) G_GNUC_NULL_TERMINATED;
void lapiz_message_type_set_valist		 (PlumaMessageType *message_type,
						  guint		   num_optional,
						  va_list	           va_args);

PlumaMessageType *lapiz_message_type_ref 	 (PlumaMessageType *message_type);
void lapiz_message_type_unref			 (PlumaMessageType *message_type);


PlumaMessage *lapiz_message_type_instantiate_valist (PlumaMessageType *message_type,
				       		     va_list	      va_args);
PlumaMessage *lapiz_message_type_instantiate 	 (PlumaMessageType *message_type,
				       		  ...) G_GNUC_NULL_TERMINATED;

const gchar *lapiz_message_type_get_object_path	 (PlumaMessageType *message_type);
const gchar *lapiz_message_type_get_method	 (PlumaMessageType *message_type);

GType lapiz_message_type_lookup			 (PlumaMessageType *message_type,
						  const gchar      *key);

void lapiz_message_type_foreach 		 (PlumaMessageType 	  *message_type,
						  PlumaMessageTypeForeach  func,
						  gpointer	   	   user_data);

G_END_DECLS

#endif /* __LAPIZ_MESSAGE_TYPE_H__ */

// ex:ts=8:noet:
