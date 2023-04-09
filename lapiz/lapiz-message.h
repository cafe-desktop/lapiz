#ifndef __LAPIZ_MESSAGE_H__
#define __LAPIZ_MESSAGE_H__

#include <glib-object.h>
#include <stdarg.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_MESSAGE			(lapiz_message_get_type ())
#define LAPIZ_MESSAGE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_MESSAGE, LapizMessage))
#define LAPIZ_MESSAGE_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_MESSAGE, LapizMessage const))
#define LAPIZ_MESSAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_MESSAGE, LapizMessageClass))
#define LAPIZ_IS_MESSAGE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_MESSAGE))
#define LAPIZ_IS_MESSAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_MESSAGE))
#define LAPIZ_MESSAGE_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_MESSAGE, LapizMessageClass))

typedef struct _LapizMessage		LapizMessage;
typedef struct _LapizMessageClass	LapizMessageClass;
typedef struct _LapizMessagePrivate	LapizMessagePrivate;

struct _LapizMessage {
	GObject parent;

	LapizMessagePrivate *priv;
};

struct _LapizMessageClass {
	GObjectClass parent_class;
};

GType lapiz_message_get_type (void) G_GNUC_CONST;

struct _LapizMessageType lapiz_message_get_message_type (LapizMessage *message);

void lapiz_message_get			(LapizMessage	 *message,
					 ...) G_GNUC_NULL_TERMINATED;
void lapiz_message_get_valist		(LapizMessage	 *message,
					 va_list 	  var_args);
void lapiz_message_get_value		(LapizMessage	 *message,
					 const gchar	 *key,
					 GValue		 *value);

void lapiz_message_set			(LapizMessage	 *message,
					 ...) G_GNUC_NULL_TERMINATED;
void lapiz_message_set_valist		(LapizMessage	 *message,
					 va_list	  	  var_args);
void lapiz_message_set_value		(LapizMessage	 *message,
					 const gchar 	 *key,
					 GValue		 *value);
void lapiz_message_set_valuesv		(LapizMessage	 *message,
					 const gchar	**keys,
					 GValue		 *values,
					 gint		  n_values);

const gchar *lapiz_message_get_object_path (LapizMessage	*message);
const gchar *lapiz_message_get_method	(LapizMessage	 *message);

gboolean lapiz_message_has_key		(LapizMessage	 *message,
					 const gchar     *key);

GType lapiz_message_get_key_type 	(LapizMessage    *message,
			    		 const gchar     *key);

gboolean lapiz_message_validate		(LapizMessage	 *message);


G_END_DECLS

#endif /* __LAPIZ_MESSAGE_H__ */

// ex:ts=8:noet:
