/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <locale.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "service.h"

/* FIXME: remove -test */
#define BUS_NAME "com.canonical.indicator.datetime-test"
#define BUS_PATH "/com/canonical/indicator/datetime"

G_DEFINE_TYPE (IndicatorDatetimeService,
               indicator_datetime_service,
               G_TYPE_OBJECT)

/* signals enum */
enum
{
  NAME_LOST,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,
  PROP_REPLACE,
  PROP_LAST
};

static GParamSpec * properties[PROP_LAST];

enum
{
  SECTION_HEADER        = (1<<0),
  SECTION_CALENDAR      = (1<<1),
  SECTION_APPOINTMENTS  = (1<<2),
  SECTION_LOCATIONS     = (1<<3),
  SECTION_SETTINGS      = (1<<4),
};

enum
{
  PROFILE_DESKTOP,
  PROFILE_GREETER,
  N_PROFILES
};

static const char * const menu_names[N_PROFILES] =
{
  "desktop",
  "desktop_greeter"
};

struct ProfileMenuInfo
{
  /* the root level -- the header is the only child of this */
  GMenu * menu;

  /* parent of the sections. This is the header's submenu */
  GMenu * submenu;

  guint export_id;
};

struct _IndicatorDatetimeServicePrivate
{
  guint own_id;
  GSimpleActionGroup * actions;
  guint actions_export_id;
  struct ProfileMenuInfo menus[N_PROFILES];
  guint rebuild_id;
  int rebuild_flags;
  GDBusConnection * conn;
  GCancellable * cancellable;
  GSimpleAction * header_action;

  gboolean replace;
};

typedef IndicatorDatetimeServicePrivate priv_t;

/***
****
***/

static void rebuild_now (IndicatorDatetimeService * self, int section);
static void rebuild_soon (IndicatorDatetimeService * self, int section);

static inline void
rebuild_header_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_HEADER);
}
static inline void
rebuild_calendar_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_CALENDAR);
}
static inline void
rebuild_appointments_section_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_APPOINTMENTS);
}
static inline void
rebuild_locations_section_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_LOCATIONS);
}
static inline void
rebuild_settings_section_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_SETTINGS);
}

/***
****
***/

static void
update_header_action (IndicatorDatetimeService * self)
{
  GVariant * v;
  gchar * a11y = g_strdup ("a11y");
  const gchar * label = "Hello World";
  const gchar * iconstr = "icon";
  const priv_t * const p = self->priv;

  g_return_if_fail (p->header_action != NULL);

  v = g_variant_new ("(sssb)", label, iconstr, a11y, TRUE);
  g_simple_action_set_state (p->header_action, v);
  g_free (a11y);
}

/***
****
***/

static GMenuModel *
create_calendar_section (IndicatorDatetimeService * self G_GNUC_UNUSED)
{
  GMenu * menu;

  menu = g_menu_new ();

  return G_MENU_MODEL (menu);
}

static GMenuModel *
create_appointments_section (IndicatorDatetimeService * self G_GNUC_UNUSED)
{
  GMenu * menu;

  menu = g_menu_new ();

  return G_MENU_MODEL (menu);
}

static GMenuModel *
create_locations_section (IndicatorDatetimeService * self G_GNUC_UNUSED)
{
  GMenu * menu;

  menu = g_menu_new ();

  return G_MENU_MODEL (menu);
}

static GMenuModel *
create_settings_section (IndicatorDatetimeService * self G_GNUC_UNUSED)
{
  GMenu * menu;

  menu = g_menu_new ();

  g_menu_append (menu, _("Date and Time Settings\342\200\246"), "indicator.activateSettings");

  return G_MENU_MODEL (menu);
}

static void
create_menu (IndicatorDatetimeService * self, int profile)
{
  GMenu * menu;
  GMenu * submenu;
  GMenuItem * header;
  GMenuModel * sections[16];
  int i;
  int n = 0;

  g_assert (0<=profile && profile<N_PROFILES);
  g_assert (self->priv->menus[profile].menu == NULL);

  if (profile == PROFILE_DESKTOP)
    {
      sections[n++] = create_calendar_section (self);
      sections[n++] = create_appointments_section (self);
      sections[n++] = create_locations_section (self);
      sections[n++] = create_settings_section (self);
    }
  else if (profile == PROFILE_GREETER)
    {
      /* FIXME: what goes here? */
    }

  /* add sections to the submenu */
  submenu = g_menu_new ();
  for (i=0; i<n; ++i)
    {
      g_menu_append_section (submenu, NULL, sections[i]);
      g_object_unref (sections[i]);
    }

  /* add submenu to the header */
  header = g_menu_item_new (NULL, "indicator._header");
  g_menu_item_set_attribute (header, "x-canonical-type", "s", "com.canonical.indicator.root");
  g_menu_item_set_submenu (header, G_MENU_MODEL (submenu));
  g_object_unref (submenu);

  /* add header to the menu */
  menu = g_menu_new ();
  g_menu_append_item (menu, header);
  g_object_unref (header);

  self->priv->menus[profile].menu = menu;
  self->priv->menus[profile].submenu = submenu;
}

/***
****  GActions
***/

static void
on_settings_activated (GSimpleAction * a      G_GNUC_UNUSED,
                       GVariant      * param  G_GNUC_UNUSED,
                       gpointer        gself  G_GNUC_UNUSED)
{
  g_message ("settings activated");
}

static void
init_gactions (IndicatorDatetimeService * self)
{
  GVariant * v;
  GSimpleAction * a;
  priv_t * p = self->priv;

  GActionEntry entries[] = {
    { "activateSettings", on_settings_activated, NULL, NULL, NULL },
  };

  p->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP(p->actions),
                                   entries,
                                   G_N_ELEMENTS(entries),
                                   self);

  /* add the header action */
  v = g_variant_new ("(sssb)", "Hello World", "icon", "a11y", TRUE);
  a = g_simple_action_new_stateful ("_header", NULL, v);
  g_simple_action_group_insert (p->actions, G_ACTION(a));
  p->header_action = a;

  rebuild_now (self, SECTION_HEADER);
}

/***
****
***/

/**
 * A small helper function for rebuild_now().
 * - removes the previous section
 * - adds and unrefs the new section
 */
static void
rebuild_section (GMenu * parent, int pos, GMenuModel * new_section)
{
  g_menu_remove (parent, pos);
  g_menu_insert_section (parent, pos, NULL, new_section);
  g_object_unref (new_section);
}

static void
rebuild_now (IndicatorDatetimeService * self, int sections)
{
  priv_t * p = self->priv;
  struct ProfileMenuInfo * desktop = &p->menus[PROFILE_DESKTOP];
  //struct ProfileMenuInfo * greeter = &p->menus[PROFILE_GREETER];

  if (sections & SECTION_HEADER)
    {
      update_header_action (self);
    }

  if (sections & SECTION_CALENDAR)
    {
      rebuild_section (desktop->submenu, 0, create_calendar_section (self));
    }

  if (sections & SECTION_APPOINTMENTS)
    {
      rebuild_section (desktop->submenu, 1, create_appointments_section (self));
    }

  if (sections & SECTION_LOCATIONS)
    {
      rebuild_section (desktop->submenu, 2, create_locations_section (self));
    }

  if (sections & SECTION_SETTINGS)
    {
      rebuild_section (desktop->submenu, 3, create_settings_section (self));
      //rebuild_section (greeter->submenu, 0, create_datetime_section(self));
    }
}

static int
rebuild_timeout_func (IndicatorDatetimeService * self)
{
  priv_t * p = self->priv;
  rebuild_now (self, p->rebuild_flags);
  p->rebuild_flags = 0;
  p->rebuild_id = 0;
  return G_SOURCE_REMOVE;
}

static void
rebuild_soon (IndicatorDatetimeService * self, int section)
{
  priv_t * p = self->priv;

  p->rebuild_flags |= section;

  if (p->rebuild_id == 0)
    {
      /* Change events seem to come over the bus in small bursts. This msec
         value is an arbitrary number that tries to be large enough to fold
         multiple events into a single rebuild, but small enough that the
         user won't notice any lag. */
      static const int REBUILD_INTERVAL_MSEC = 500;

      p->rebuild_id = g_timeout_add (REBUILD_INTERVAL_MSEC,
                                     (GSourceFunc)rebuild_timeout_func,
                                     self);
    }
}

/***
**** GDBus
***/

static void
on_bus_acquired (GDBusConnection * connection,
                 const gchar     * name,
                 gpointer          gself)
{
  int i;
  guint id;
  GError * err = NULL;
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE(gself);
  priv_t * p = self->priv;

  g_debug ("bus acquired: %s", name);

  p->conn = g_object_ref (G_OBJECT (connection));

  /* export the actions */
  if ((id = g_dbus_connection_export_action_group (connection,
                                                   BUS_PATH,
                                                   G_ACTION_GROUP (p->actions),
                                                   &err)))
    {
      p->actions_export_id = id;
    }
  else
    {
      g_warning ("cannot export action group: %s", err->message);
      g_clear_error (&err);
    }

  /* export the menus */
  for (i=0; i<N_PROFILES; ++i)
    {
      char * path = g_strdup_printf ("%s/%s", BUS_PATH, menu_names[i]);
      struct ProfileMenuInfo * menu = &p->menus[i];

      if (menu->menu == NULL)
        create_menu (self, i);

      if ((id = g_dbus_connection_export_menu_model (connection,
                                                     path,
                                                     G_MENU_MODEL (menu->menu),
                                                     &err)))
        {
          menu->export_id = id;
        }
      else
        {
          g_warning ("cannot export %s menu: %s", menu_names[i], err->message);
          g_clear_error (&err);
        }

      g_free (path);
    }
}

static void
unexport (IndicatorDatetimeService * self)
{
  int i;
  priv_t * p = self->priv;

  /* unexport the menus */
  for (i=0; i<N_PROFILES; ++i)
    {
      guint * id = &self->priv->menus[i].export_id;

      if (*id)
        {
          g_dbus_connection_unexport_menu_model (p->conn, *id);
          *id = 0;
        }
    }

  /* unexport the actions */
  if (p->actions_export_id)
    {
      g_dbus_connection_unexport_action_group (p->conn, p->actions_export_id);
      p->actions_export_id = 0;
    }
}

static void
on_name_lost (GDBusConnection * connection G_GNUC_UNUSED,
              const gchar     * name,
              gpointer          gself)
{
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE (gself);

  g_debug ("%s %s name lost %s", G_STRLOC, G_STRFUNC, name);

  unexport (self);

  g_signal_emit (self, signals[NAME_LOST], 0, NULL);
}

/***
****  GObject virtual functions
***/

static void
my_constructed (GObject * o)
{
  GBusNameOwnerFlags owner_flags;
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE(o);

  /* own the name in constructed() instead of init() so that
     we'll know the value of the 'replace' property */
  owner_flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
  if (self->priv->replace)
    owner_flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

  self->priv->own_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                       BUS_NAME,
                                       owner_flags,
                                       on_bus_acquired,
                                       NULL,
                                       on_name_lost,
                                       self,
                                       NULL);
}

static void
my_get_property (GObject     * o,
                  guint         property_id,
                  GValue      * value,
                  GParamSpec  * pspec)
{
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE (o);
 
  switch (property_id)
    {
      case PROP_REPLACE:
        g_value_set_boolean (value, self->priv->replace);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
my_set_property (GObject       * o,
                 guint           property_id,
                 const GValue  * value,
                 GParamSpec    * pspec)
{
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE (o);

  switch (property_id)
    {
      case PROP_REPLACE:
        self->priv->replace = g_value_get_boolean (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
my_dispose (GObject * o)
{
  int i;
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE(o);
  priv_t * p = self->priv;

  if (p->own_id)
    {
      g_bus_unown_name (p->own_id);
      p->own_id = 0;
    }

  unexport (self);

  if (p->cancellable != NULL)
    {
      g_cancellable_cancel (p->cancellable);
      g_clear_object (&p->cancellable);
    }

  if (p->rebuild_id)
    {
      g_source_remove (p->rebuild_id);
      p->rebuild_id = 0;
    }

  g_clear_object (&p->actions);

  for (i=0; i<N_PROFILES; ++i)
    g_clear_object (&p->menus[i].menu);

  g_clear_object (&p->header_action);
  g_clear_object (&p->conn);

  G_OBJECT_CLASS (indicator_datetime_service_parent_class)->dispose (o);
}

/***
****  Instantiation
***/

static void
indicator_datetime_service_init (IndicatorDatetimeService * self)
{
  priv_t * p;

  /* init our priv pointer */
  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_DATETIME_SERVICE,
                                   IndicatorDatetimeServicePrivate);
  self->priv = p;

  /* init the backend objects */
  p->cancellable = g_cancellable_new ();

  init_gactions (self);
}

static void
indicator_datetime_service_class_init (IndicatorDatetimeServiceClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = my_dispose;
  object_class->constructed = my_constructed;
  object_class->get_property = my_get_property;
  object_class->set_property = my_set_property;

  g_type_class_add_private (klass, sizeof (IndicatorDatetimeServicePrivate));

  signals[NAME_LOST] = g_signal_new (INDICATOR_DATETIME_SERVICE_SIGNAL_NAME_LOST,
                                     G_TYPE_FROM_CLASS(klass),
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET (IndicatorDatetimeServiceClass, name_lost),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

  properties[PROP_0] = NULL;

  properties[PROP_REPLACE] = g_param_spec_boolean ("replace",
                                                   "Replace Service",
                                                   "Replace existing service",
                                                   FALSE,
                                                   G_PARAM_READWRITE |
                                                   G_PARAM_CONSTRUCT_ONLY |
                                                   G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

/***
****  Public API
***/

IndicatorDatetimeService *
indicator_datetime_service_new (gboolean replace)
{
  GObject * o = g_object_new (INDICATOR_TYPE_DATETIME_SERVICE,
                              "replace", replace,
                              NULL);

  return INDICATOR_DATETIME_SERVICE (o);
}
