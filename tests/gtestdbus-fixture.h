/*
 * Copyright 2016 Canonical Ltd.
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

#pragma once

#include <datetime/actions-live.h>

#include "glib-fixture.h"

#include <datetime/dbus-shared.h>
#include <datetime/timezone.h>

/***
****
***/

struct GTestDBusFixture: public GlibFixture
{
private:

    using super = GlibFixture;

protected:

    GDBusConnection* m_bus {};
    GTestDBus* m_test_bus {};

    virtual void SetUp() override
    {
        super::SetUp();

        // set up a test bus
        m_test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(m_test_bus);
        const char * address = g_test_dbus_get_bus_address(m_test_bus);
        g_setenv("DBUS_SYSTEM_BUS_ADDRESS", address, true);
        g_setenv("DBUS_SESSION_BUS_ADDRESS", address, true);
        g_debug("test_dbus's address is %s", address);

        // get the bus
        m_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        g_dbus_connection_set_exit_on_close(m_bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(m_bus), (gpointer*)&m_bus);
    }

    virtual void TearDown() override
    {
        // take down the bus
        bool bus_finished = false;
        g_object_weak_ref(
            G_OBJECT(m_bus),
            [](gpointer gbus_finished, GObject*){*static_cast<bool*>(gbus_finished) = true;},
            &bus_finished
        );
        g_clear_object(&m_bus);
        EXPECT_TRUE(wait_for([&bus_finished](){return bus_finished;})) << "bus shutdown took too long";

        // take down the GTestBus
        g_clear_object(&m_test_bus);

        super::TearDown();
    }

    void start_dbusmock(const std::vector<std::string>& args)
    {
        // build the spawn args
        std::vector<const char*> child_argv {
            "python3", "-m", "dbusmock"
        };
        for(const auto& arg : args)
            child_argv.push_back(arg.c_str());
        child_argv.push_back(nullptr);
       
        // start it
        GError* error {};
        g_spawn_async(
            nullptr,
            const_cast<gchar**>(&child_argv.front()),
            nullptr,
            G_SPAWN_SEARCH_PATH,
            nullptr,
            nullptr,
            nullptr,
            &error
        );
        g_assert_no_error(error);
    }


    void start_dbusmock_template(
         const char* template_name,
         const char* json_parameters=nullptr)
    {
        std::vector<std::string> args {
            "--template", template_name
        };
        if (json_parameters != nullptr) {
            args.insert(args.end(), {"--parameters", json_parameters});
        }
        start_dbusmock(args);
    }

    GVariant* get_property(
        GDBusConnection* connection,
        const char* bus_name,
        const char* object_path,
        const char* interface_name,
        const char* property_name,
        const GVariantType* reply_type=nullptr)
    {
        const GVariantType* wrapped_reply_type {};
        if (reply_type != nullptr) {
            std::string tmp("()");
            tmp.insert(
                1,
                g_variant_type_peek_string(reply_type),
                g_variant_type_get_string_length(reply_type)
            );
            wrapped_reply_type = G_VARIANT_TYPE(tmp.c_str());
        }

        GError* error {};
        auto v = g_dbus_connection_call_sync(
            connection,
            bus_name,
            object_path,
            Bus::Properties::IFACE,
            Bus::Properties::Methods::GET,
            g_variant_new("(ss)", interface_name, property_name),
            wrapped_reply_type,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error);
        g_assert_no_error(error);

        GVariant* child {};
        if (g_variant_n_children(v) > 0)
            child = g_variant_get_child_value (v, 0);
        g_clear_pointer(&v, g_variant_unref);
        return child;
    }
};

