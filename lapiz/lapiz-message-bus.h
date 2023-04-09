#ifndef __LAPIZ_MESSAGE_BUS_H__
#define __LAPIZ_MESSAGE_BUS_H__

#include <glib-object.h>
#include <lapiz/lapiz-message.h>
#include <lapiz/lapiz-message-type.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_MESSAGE_BUS			(lapiz_message_bus_get_type ())
#define LAPIZ_MESSAGE_BUS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_MESSAGE_BUS, LapizMessageBus))
#define LAPIZ_MESSAGE_BUS_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_MESSAGE_BUS, LapizMessageBus const))
#define LAPIZ_MESSAGE_BUS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_MESSAGE_BUS, LapizMessageBusClass))
#define LAPIZ_IS_MESSAGE_BUS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_MESSAGE_BUS))
#define LAPIZ_IS_MESSAGE_BUS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_MESSAGE_BUS))
#define LAPIZ_MESSAGE_BUS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_MESSAGE_BUS, LapizMessageBusClass))

typedef struct _LapizMessageBus		LapizMessageBus;
typedef struct _LapizMessageBusClass	LapizMessageBusClass;
typedef struct _LapizMessageBusPrivate	LapizMessageBusPrivate;

struct _LapizMessageBus {
	GObject parent;

	LapizMessageBusPrivate *priv;
};

struct _LapizMessageBusClass {
	GObjectClass parent_class;

	void (*dispatch)		(LapizMessageBus  *bus,
					 LapizMessage     *message);
	void (*registered)		(LapizMessageBus  *bus,
					 LapizMessageType *message_type);
	void (*unregistered)		(LapizMessageBus  *bus,
					 LapizMessageType *message_type);
};

typedef void (* LapizMessageCallback) 	(LapizMessageBus *bus,
					 LapizMessage	 *message,
					 gpointer	  userdata);

typedef void (* LapizMessageBusForeach) (LapizMessageType *message_type,
					 gpointer	   userdata);

GType lapiz_message_bus_get_type (void) G_GNUC_CONST;

LapizMessageBus *lapiz_message_bus_get_default	(void);
LapizMessageBus *lapiz_message_bus_new		(void);

/* registering messages */
LapizMessageType *lapiz_message_bus_lookup	(LapizMessageBus 	*bus,
						 const gchar		*object_path,
						 const gchar		*method);
LapizMessageType *lapiz_message_bus_register	(LapizMessageBus		*bus,
					   	 const gchar 		*object_path,
					  	 const gchar		*method,
					  	 guint		 	 num_optional,
					  	 ...) G_GNUC_NULL_TERMINATED;

void lapiz_message_bus_unregister	  (LapizMessageBus	*bus,
					   LapizMessageType	*message_type);

void lapiz_message_bus_unregister_all	  (LapizMessageBus	*bus,
					   const gchar		*object_path);

gboolean lapiz_message_bus_is_registered  (LapizMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method);

void lapiz_message_bus_foreach		  (LapizMessageBus        *bus,
					   LapizMessageBusForeach  func,
					   gpointer		   userdata);


/* connecting to message events */
guint lapiz_message_bus_connect	 	  (LapizMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   LapizMessageCallback	 callback,
					   gpointer		 userdata,
					   GDestroyNotify        destroy_data);

void lapiz_message_bus_disconnect	  (LapizMessageBus	*bus,
					   guint		 id);

void lapiz_message_bus_disconnect_by_func (LapizMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   LapizMessageCallback	 callback,
					   gpointer		 userdata);

/* blocking message event callbacks */
void lapiz_message_bus_block		  (LapizMessageBus	*bus,
					   guint		 id);
void lapiz_message_bus_block_by_func	  (LapizMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   LapizMessageCallback	 callback,
					   gpointer		 userdata);

void lapiz_message_bus_unblock		  (LapizMessageBus	*bus,
					   guint		 id);
void lapiz_message_bus_unblock_by_func	  (LapizMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   LapizMessageCallback	 callback,
					   gpointer		 userdata);

/* sending messages */
void lapiz_message_bus_send_message	  (LapizMessageBus	*bus,
					   LapizMessage		*message);
void lapiz_message_bus_send_message_sync  (LapizMessageBus	*bus,
					   LapizMessage		*message);

void lapiz_message_bus_send		  (LapizMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   ...) G_GNUC_NULL_TERMINATED;
LapizMessage *lapiz_message_bus_send_sync (LapizMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* __LAPIZ_MESSAGE_BUS_H__ */

// ex:ts=8:noet:
