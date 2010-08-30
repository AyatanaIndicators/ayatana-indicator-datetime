#ifndef __DATETIME_INTERFACE_H__
#define __DATETIME_INTERFACE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define DATETIME_INTERFACE_TYPE            (datetime_interface_get_type ())
#define DATETIME_INTERFACE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), DATETIME_INTERFACE_TYPE, DatetimeInterface))
#define DATETIME_INTERFACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), DATETIME_INTERFACE_TYPE, DatetimeInterfaceClass))
#define IS_DATETIME_INTERFACE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DATETIME_INTERFACE_TYPE))
#define IS_DATETIME_INTERFACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DATETIME_INTERFACE_TYPE))
#define DATETIME_INTERFACE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), DATETIME_INTERFACE_TYPE, DatetimeInterfaceClass))

typedef struct _DatetimeInterface      DatetimeInterface;
typedef struct _DatetimeInterfaceClass DatetimeInterfaceClass;

struct _DatetimeInterfaceClass {
	GObjectClass parent_class;

	void (*update_time) (void);
};

struct _DatetimeInterface {
	GObject parent;
};

GType datetime_interface_get_type (void);

G_END_DECLS

#endif
