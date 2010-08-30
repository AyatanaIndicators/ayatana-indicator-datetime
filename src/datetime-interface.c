#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "datetime-interface.h"
#include "datetime-service-server.h"
#include "dbus-shared.h"

static void datetime_interface_class_init (DatetimeInterfaceClass *klass);
static void datetime_interface_init       (DatetimeInterface *self);
static void datetime_interface_dispose    (GObject *object);
static void datetime_interface_finalize   (GObject *object);

G_DEFINE_TYPE (DatetimeInterface, datetime_interface, G_TYPE_OBJECT);

static void
datetime_interface_class_init (DatetimeInterfaceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = datetime_interface_dispose;
	object_class->finalize = datetime_interface_finalize;

	dbus_g_object_type_install_info(DATETIME_INTERFACE_TYPE, &dbus_glib__datetime_service_server_object_info);

	return;
}

static void
datetime_interface_init (DatetimeInterface *self)
{
	DBusGConnection * connection = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
	dbus_g_connection_register_g_object(connection,
	                                    SERVICE_OBJ,
	                                    G_OBJECT(self));

	return;
}

static void
datetime_interface_dispose (GObject *object)
{

	G_OBJECT_CLASS (datetime_interface_parent_class)->dispose (object);
	return;
}

static void
datetime_interface_finalize (GObject *object)
{

	G_OBJECT_CLASS (datetime_interface_parent_class)->finalize (object);
	return;
}
