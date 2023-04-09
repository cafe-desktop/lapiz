#ifndef __LAPIZ_MESSAGE_H__
#define __LAPIZ_MESSAGE_H__

#include <glib-object.h>
#include <stdarg.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_MESSAGE			(lapiz_message_get_type ())
#define LAPIZ_MESSAGE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_MESSAGE, PlumaMessage))
#define LAPIZ_MESSAGE_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_MESSAGE, PlumaMessage const))
#define LAPIZ_MESSAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_MESSAGE, PlumaMessageClass))
#define LAPIZ_IS_MESSAGE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_MESSAGE))
#define LAPIZ_IS_MESSAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_MESSAGE))
#define LAPIZ_MESSAGE_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_MESSAGE, PlumaMessageClass))

typedef struct _PlumaMessage		PlumaMessage;
typedef struct _PlumaMessageClass	PlumaMessageClass;
typedef struct _PlumaMessagePrivate	PlumaMessagePrivate;

struct _PlumaMessage {
	GObject parent;

	PlumaMessagePrivate *priv;
};

struct _PlumaMessageClass {
	GObjectClass parent_class;
};

GType lapiz_message_get_type (void) G_GNUC_CONST;

struct _PlumaMessageType lapiz_message_get_message_type (PlumaMessage *message);

void lapiz_message_get			(PlumaMessage	 *message,
					 ...) G_GNUC_NULL_TERMINATED;
void lapiz_message_get_valist		(PlumaMessage	 *message,
					 va_list 	  var_args);
void lapiz_message_get_value		(PlumaMessage	 *message,
					 const gchar	 *key,
					 GValue		 *value);

void lapiz_message_set			(PlumaMessage	 *message,
					 ...) G_GNUC_NULL_TERMINATED;
void lapiz_message_set_valist		(PlumaMessage	 *message,
					 va_list	  	  var_args);
void lapiz_message_set_value		(PlumaMessage	 *message,
					 const gchar 	 *key,
					 GValue		 *value);
void lapiz_message_set_valuesv		(PlumaMessage	 *message,
					 const gchar	**keys,
					 GValue		 *values,
					 gint		  n_values);

const gchar *lapiz_message_get_object_path (PlumaMessage	*message);
const gchar *lapiz_message_get_method	(PlumaMessage	 *message);

gboolean lapiz_message_has_key		(PlumaMessage	 *message,
					 const gchar     *key);

GType lapiz_message_get_key_type 	(PlumaMessage    *message,
			    		 const gchar     *key);

gboolean lapiz_message_validate		(PlumaMessage	 *message);


G_END_DECLS

#endif /* __LAPIZ_MESSAGE_H__ */

// ex:ts=8:noet:
