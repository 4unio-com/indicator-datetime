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

#pragma once

#include "gtestdbus-fixture.h"

#include <datetime/actions-live.h>
#include <datetime/dbus-shared.h>
#include <datetime/timezone.h>

#include <sstream>

/***
****
***/

struct TimedatedFixture: public GTestDBusFixture
{
protected:

    void start_timedate1(const std::string& tzid)
    {
        std::string json_parameters;
        if (!tzid.empty()) {
            std::ostringstream tmp;
            tmp << "{\"Timezone\": \"" << tzid << "\"}";
            json_parameters = tmp.str();
        }

        start_dbusmock_template("timedated", json_parameters.c_str());

        wait_for_name_owned(m_bus, Bus::Timedate1::BUSNAME);
    }

    void set_timedate1_timezone(const std::string& tzid)
    {
        GError* error {};
        auto v = g_dbus_connection_call_sync(
            m_bus,
            Bus::Timedate1::BUSNAME,
            Bus::Timedate1::ADDR,
            Bus::Timedate1::IFACE,
            Bus::Timedate1::Methods::SET_TIMEZONE,
            g_variant_new("(sb)", tzid.c_str(), FALSE),
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error);
        g_assert_no_error(error);

        g_clear_pointer(&v, g_variant_unref);
    }

    std::string get_timedate1_timezone()
    {
        auto v = get_property(
            m_bus,
            Bus::Timedate1::BUSNAME,
            Bus::Timedate1::ADDR,
            Bus::Timedate1::IFACE,
            Bus::Timedate1::Properties::TIMEZONE,
            G_VARIANT_TYPE_VARIANT);

        std::string tzid;
        auto unboxed = g_variant_get_variant(v); 
        if (unboxed != nullptr) {
            const char* tz = g_variant_get_string(unboxed, nullptr); 
            if (tz != nullptr)
                tzid = tz;
            g_clear_pointer(&unboxed, g_variant_unref);
        }

        g_clear_pointer(&v, g_variant_unref);
        return tzid;
    }

    bool wait_for_tzid(const std::string& tzid, unity::indicator::datetime::Timezone& tz)
    {
        return wait_for([&tzid, &tz](){return tzid == tz.timezone.get();});
    }
};

#define EXPECT_TZID(expected_tzid, tmp) \
    EXPECT_TRUE(wait_for_tzid(expected_tzid, tmp)) \
        << "expected " << expected_tzid \
        << " got " << tmp.timezone.get();

