#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "datetime-interface.h"

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

	return;
}

static void
datetime_interface_init (DatetimeInterface *self)
{

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
