/*
 * Copyright 2013 Canonical Ltd.
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
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#include <datetime/clock.h>
#include <datetime/dbus-shared.h>

#include <glib.h>
#include <gio/gio.h>

#include <set>
#include <string>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class Clock::Impl
{
public:

    Impl(Clock& owner):
        m_owner{owner},
        m_cancellable{g_cancellable_new()}
    {
        g_bus_get(G_BUS_TYPE_SYSTEM, m_cancellable, on_system_bus_ready_static, this);
    }

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);

        for(const auto& tag : m_watched_names)
            g_bus_unwatch_name(tag);

        for(const auto& subscription : m_subscriptions)
            g_dbus_connection_signal_unsubscribe(m_system_bus, subscription);

        g_clear_object(&m_system_bus);
    }

private:

    static void on_system_bus_ready_static(
        GObject* /*source_object*/,
        GAsyncResult* res,
        gpointer gself)
    {
        GError* error {};
        auto system_bus = g_bus_get_finish(res, &error);

        if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("Unable to get system bus for clock: %s", error->message);

            g_clear_error(&error);
        }
        else if (system_bus != nullptr)
        {
            static_cast<Impl*>(gself)->on_system_bus_ready(system_bus);
            g_clear_object(&system_bus);
        }
    }

    void on_system_bus_ready(GDBusConnection* system_bus)
    {
        g_return_if_fail(m_system_bus == nullptr); // should only get called once...

        m_system_bus = G_DBUS_CONNECTION(g_object_ref(system_bus));
     
        m_watched_names.insert(
            g_bus_watch_name_on_connection(
                m_system_bus,
                "org.freedesktop.login1",
                G_BUS_NAME_WATCHER_FLAGS_NONE,
                on_login1_appeared,
                on_login1_vanished,
                this,
                nullptr
            )
        );

        m_watched_names.insert(
            g_bus_watch_name_on_connection(
                m_system_bus,
                BUS_POWERD_NAME,
                G_BUS_NAME_WATCHER_FLAGS_NONE,
                on_powerd_appeared,
                on_powerd_vanished,
                this,
                nullptr
            )
        );

        m_subscriptions.insert(
            g_dbus_connection_signal_subscribe(
                m_system_bus,
                nullptr, // sender
                "org.freedesktop.login1.Manager", // interface
                "PrepareForSleep", // signal name
                "/org/freedesktop/login1", // object path
                nullptr, // arg0
                G_DBUS_SIGNAL_FLAGS_NONE,
                on_prepare_for_sleep,
                this,
                nullptr
            )
        );

        m_subscriptions.insert(
            g_dbus_connection_signal_subscribe(
                m_system_bus,
                nullptr, // sender
                BUS_POWERD_INTERFACE,
                "SysPowerStateChange",
                BUS_POWERD_PATH,
                nullptr, // arg0
                G_DBUS_SIGNAL_FLAGS_NONE,
                on_sys_power_state_change,
                this, // user_data
                nullptr
            )
        );
    }

private:

    /**
    ***  DBus Chatter: org.freedesktop.login1
    ***
    ***  Fire Clock::minute_changed() signal on login1's PrepareForSleep signal
    **/

    static void on_login1_appeared(GDBusConnection* /*connection*/,
                                   const gchar* /*name*/,
                                   const gchar* name_owner,
                                   gpointer gself)
    {
        static_cast<Impl*>(gself)->m_login1_owner = name_owner;
    }

    static void on_login1_vanished(GDBusConnection* /*connection*/,
                                   const gchar* /*name*/,
                                   gpointer gself)
    {
        static_cast<Impl*>(gself)->m_login1_owner.clear();
    }

    static void on_prepare_for_sleep(GDBusConnection* /*connection*/,
                                     const gchar* sender_name,
                                     const gchar* /*object_path*/,
                                     const gchar* /*interface_name*/,
                                     const gchar* signal_name,
                                     GVariant* /*parameters*/,
                                     gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        const char* owner = self->m_login1_owner.c_str();

        if(!g_strcmp0(sender_name, owner))
            self->m_owner.minute_changed();
        else
            g_warning("discarding signal '%s' from '%s', expected sender '%s'", signal_name, sender_name, owner);
    }

    /**
    ***  DBus Chatter: com.canonical.powerd
    ***
    ***  Fire Clock::minute_changed() signal when powerd says the system's
    ***  has awoken from sleep -- the old timestamp is likely out-of-date
    **/

    static void on_powerd_appeared(GDBusConnection* /*connection*/,
                                   const gchar* /*name*/,
                                   const gchar* name_owner,
                                   gpointer gself)
    {
        static_cast<Impl*>(gself)->m_powerd_owner = name_owner;
    }

    static void on_powerd_vanished(GDBusConnection* /*connection*/,
                                   const gchar* /*name*/,
                                   gpointer gself)
    {
        static_cast<Impl*>(gself)->m_powerd_owner.clear();
    }

    static void on_sys_power_state_change(GDBusConnection* /*connection*/,
                                          const gchar* sender_name,
                                          const gchar* /*object_path*/,
                                          const gchar* /*interface_name*/,
                                          const gchar* signal_name,
                                          GVariant* /*parameters*/,
                                          gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        const char* owner = self->m_powerd_owner.c_str();

        if (!g_strcmp0(sender_name, owner))
            self->m_owner.minute_changed();
        else
            g_warning("discarding signal '%s' from '%s', expected sender '%s'", signal_name, sender_name, owner);
    }

    /***
    ****
    ***/

    Clock& m_owner;
    GCancellable * m_cancellable {};
    GDBusConnection* m_system_bus {};
    std::set<guint> m_watched_names;
    std::set<guint> m_subscriptions;
    std::string m_login1_owner;
    std::string m_powerd_owner;
};

/***
****
***/

Clock::Clock():
   m_impl{new Impl{*this}}
{
}

Clock::~Clock()
{
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
