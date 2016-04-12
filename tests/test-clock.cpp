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

#include "gtestdbus-fixture.h"
#include "timezone-mock.h"

#include <datetime/clock.h>
#include <datetime/clock-mock.h>
#include <datetime/timezone.h>

#include <notifications/dbus-shared.h>

/***
****
***/

using namespace unity::indicator::datetime;

using ClockFixture = GTestDBusFixture;

TEST_F(ClockFixture, MinuteChangedSignalShouldTriggerOncePerMinute)
{
    // start up a live clock
    auto timezone_ = std::make_shared<MockTimezone>();
    timezone_->timezone.set("America/New_York");
    LiveClock clock(timezone_);

    // count how many times clock.minute_changed() is emitted over the next minute
    const DateTime now = clock.localtime();
    const auto gnow = now.get();
    auto gthen = g_date_time_add_minutes(gnow, 1);
    int count = 0;
    clock.minute_changed.connect([&count](){count++;});
    const auto msec = g_date_time_difference(gthen,gnow) / 1000;
    wait_msec(msec);
    EXPECT_EQ(1, count);
    g_date_time_unref(gthen);
}

/***
****
***/

TEST_F(ClockFixture, HelloFixture)
{
    auto timezone_ = std::make_shared<MockTimezone>();
    timezone_->timezone.set("America/New_York");
    LiveClock clock(timezone_);
}


TEST_F(ClockFixture, TimezoneChangeTriggersSkew)
{
    auto timezone_ = std::make_shared<MockTimezone>();
    timezone_->timezone.set("America/New_York");
    LiveClock clock(timezone_);

    auto tz_nyc = g_time_zone_new("America/New_York");
    auto now_nyc = g_date_time_new_now(tz_nyc);
    auto now = clock.localtime();
    EXPECT_EQ(g_date_time_get_utc_offset(now_nyc), g_date_time_get_utc_offset(now.get()));
    EXPECT_LE(abs(g_date_time_difference(now_nyc,now.get())), G_USEC_PER_SEC);
    g_date_time_unref(now_nyc);
    g_time_zone_unref(tz_nyc);

    /// change the timezones!
    clock.minute_changed.connect([this](){
                   g_main_loop_quit(loop);
               });
    g_idle_add([](gpointer gs){
                   static_cast<Timezone*>(gs)->timezone.set("America/Los_Angeles");
                   return G_SOURCE_REMOVE;
               }, timezone_.get());
    g_main_loop_run(loop);

    auto tz_la = g_time_zone_new("America/Los_Angeles");
    auto now_la = g_date_time_new_now(tz_la);
    now = clock.localtime();
    EXPECT_EQ(g_date_time_get_utc_offset(now_la), g_date_time_get_utc_offset(now.get()));
    EXPECT_LE(abs(g_date_time_difference(now_la,now.get())), G_USEC_PER_SEC);
    g_date_time_unref(now_la);
    g_time_zone_unref(tz_la);
}

/**
 * Confirm that a "PrepareForSleep" event triggers a skew event
 */
TEST_F(ClockFixture, SleepTriggersSkew)
{
    auto timezone_ = std::make_shared<MockTimezone>();
    timezone_->timezone.set("America/New_York");
    LiveClock clock(timezone_);

    bool skewed {};
    clock.minute_changed.connect([&skewed, this](){
        skewed = true;
        g_main_loop_quit(loop);
        return G_SOURCE_REMOVE;
    });

    // start up a login1 mock
    start_dbusmock_template("logind");
    ASSERT_NAME_OWNED_EVENTUALLY(m_bus, "org.freedesktop.login1");

    // have the login1 mock emit a 'PrepareForSleep' signal
    GError* error {};
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("av"));
    g_variant_builder_add(&builder, "v", g_variant_new("(b)", FALSE));
    g_dbus_connection_call_sync(
        m_bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.DBus.Mock",
        "EmitSignal",
        g_variant_new("(sssav)", "org.freedesktop.login1.Manager", "PrepareForSleep", "b", &builder),
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error);
    g_assert_no_error(error);

    // wait for the clock.minute_changed event
    EXPECT_TRUE(wait_for([&skewed](){return skewed;}, 2000));
}

/**
 * Confirm that powerd's SysPowerStateChange triggers
 * a timestamp change
 */
TEST_F(ClockFixture, SysPowerStateChange)
{
    // set up the mock clock
    bool minute_changed {};
    auto clock = std::make_shared<MockClock>(DateTime::NowLocal());
    clock->minute_changed.connect([&minute_changed]() {
        g_message("hello world");
        minute_changed = true;
    });

    // set up the powerd bus owner  
    auto tag = g_bus_own_name_on_connection(
        m_bus,
        BUS_POWERD_NAME,
        G_BUS_NAME_OWNER_FLAGS_NONE,
        nullptr /*on_name_acquired*/,
        nullptr /*on_name_vanished*/,
        nullptr /*user_data*/,
        nullptr /*user_data closure*/
    );
    ASSERT_NAME_OWNED_EVENTUALLY(m_bus, BUS_POWERD_NAME);

    // control test -- minute_changed shouldn't get triggered
    // when the clock is silently changed
    const DateTime not_now {DateTime::Local(1999, 12, 31, 23, 59, 59)};
    clock->set_localtime_quietly(not_now);
    wait_msec();
    ASSERT_FALSE(minute_changed);

    // now for the actual test,
    // confirm that SysPowerStateChange triggers a minute_changed() signal
    GError* error {};
    g_dbus_connection_emit_signal(
        m_bus,
        nullptr,
        BUS_POWERD_PATH,
        BUS_POWERD_INTERFACE,
        "SysPowerStateChange",
        g_variant_new("(i)", 1),
        &error
    );
    g_assert_no_error(error);
    EXPECT_TRUE(wait_for([&minute_changed](){return minute_changed;}));

    // cleanup
    g_bus_unown_name(tag);
}
