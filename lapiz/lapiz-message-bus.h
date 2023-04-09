#ifndef __LAPIZ_MESSAGE_BUS_H__
#define __LAPIZ_MESSAGE_BUS_H__

#include <glib-object.h>
#include <lapiz/lapiz-message.h>
#include <lapiz/lapiz-message-type.h>

G_BEGIN_DECLS

#define LAPIZ_TYPE_MESSAGE_BUS			(lapiz_message_bus_get_type ())
#define LAPIZ_MESSAGE_BUS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_MESSAGE_BUS, PlumaMessageBus))
#define LAPIZ_MESSAGE_BUS_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_MESSAGE_BUS, PlumaMessageBus const))
#define LAPIZ_MESSAGE_BUS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_MESSAGE_BUS, PlumaMessageBusClass))
#define LAPIZ_IS_MESSAGE_BUS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_MESSAGE_BUS))
#define LAPIZ_IS_MESSAGE_BUS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_MESSAGE_BUS))
#define LAPIZ_MESSAGE_BUS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_MESSAGE_BUS, PlumaMessageBusClass))

typedef struct _PlumaMessageBus		PlumaMessageBus;
typedef struct _PlumaMessageBusClass	PlumaMessageBusClass;
typedef struct _PlumaMessageBusPrivate	PlumaMessageBusPrivate;

struct _PlumaMessageBus {
	GObject parent;

	PlumaMessageBusPrivate *priv;
};

struct _PlumaMessageBusClass {
	GObjectClass parent_class;

	void (*dispatch)		(PlumaMessageBus  *bus,
					 PlumaMessage     *message);
	void (*registered)		(PlumaMessageBus  *bus,
					 PlumaMessageType *message_type);
	void (*unregistered)		(PlumaMessageBus  *bus,
					 PlumaMessageType *message_type);
};

typedef void (* PlumaMessageCallback) 	(PlumaMessageBus *bus,
					 PlumaMessage	 *message,
					 gpointer	  userdata);

typedef void (* PlumaMessageBusForeach) (PlumaMessageType *message_type,
					 gpointer	   userdata);

GType lapiz_message_bus_get_type (void) G_GNUC_CONST;

PlumaMessageBus *lapiz_message_bus_get_default	(void);
PlumaMessageBus *lapiz_message_bus_new		(void);

/* registering messages */
PlumaMessageType *lapiz_message_bus_lookup	(PlumaMessageBus 	*bus,
						 const gchar		*object_path,
						 const gchar		*method);
PlumaMessageType *lapiz_message_bus_register	(PlumaMessageBus		*bus,
					   	 const gchar 		*object_path,
					  	 const gchar		*method,
					  	 guint		 	 num_optional,
					  	 ...) G_GNUC_NULL_TERMINATED;

void lapiz_message_bus_unregister	  (PlumaMessageBus	*bus,
					   PlumaMessageType	*message_type);

void lapiz_message_bus_unregister_all	  (PlumaMessageBus	*bus,
					   const gchar		*object_path);

gboolean lapiz_message_bus_is_registered  (PlumaMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method);

void lapiz_message_bus_foreach		  (PlumaMessageBus        *bus,
					   PlumaMessageBusForeach  func,
					   gpointer		   userdata);


/* connecting to message events */
guint lapiz_message_bus_connect	 	  (PlumaMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   PlumaMessageCallback	 callback,
					   gpointer		 userdata,
					   GDestroyNotify        destroy_data);

void lapiz_message_bus_disconnect	  (PlumaMessageBus	*bus,
					   guint		 id);

void lapiz_message_bus_disconnect_by_func (PlumaMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   PlumaMessageCallback	 callback,
					   gpointer		 userdata);

/* blocking message event callbacks */
void lapiz_message_bus_block		  (PlumaMessageBus	*bus,
					   guint		 id);
void lapiz_message_bus_block_by_func	  (PlumaMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   PlumaMessageCallback	 callback,
					   gpointer		 userdata);

void lapiz_message_bus_unblock		  (PlumaMessageBus	*bus,
					   guint		 id);
void lapiz_message_bus_unblock_by_func	  (PlumaMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   PlumaMessageCallback	 callback,
					   gpointer		 userdata);

/* sending messages */
void lapiz_message_bus_send_message	  (PlumaMessageBus	*bus,
					   PlumaMessage		*message);
void lapiz_message_bus_send_message_sync  (PlumaMessageBus	*bus,
					   PlumaMessage		*message);

void lapiz_message_bus_send		  (PlumaMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   ...) G_GNUC_NULL_TERMINATED;
PlumaMessage *lapiz_message_bus_send_sync (PlumaMessageBus	*bus,
					   const gchar		*object_path,
					   const gchar		*method,
					   ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* __LAPIZ_MESSAGE_BUS_H__ */

// ex:ts=8:noet:
