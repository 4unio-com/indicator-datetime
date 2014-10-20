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

#ifndef INDICATOR_DATETIME_APPOINTMENT_H
#define INDICATOR_DATETIME_APPOINTMENT_H

#include <datetime/date-time.h>
#include <string>
#include <chrono>
#include <utility>
#include <vector>

namespace unity {
namespace indicator {
namespace datetime {

struct Alarm
{
    std::string text;
    std::string audio_url;
    DateTime time;
    std::chrono::seconds duration;

    bool operator== (const Alarm& that) const;
};

/**
 * \brief Plain Old Data Structure that represents a calendar appointment.
 *
 * @see Planner
 */
struct Appointment
{
public:
    //enum Type { EVENT, TODO };
    //Type type = EVENT;
    bool ubuntu_alarm = false;

    std::string uid;
    std::string color; 
    std::string summary;
    DateTime begin;
    DateTime end;

    std::vector<Alarm> alarms;

    bool operator== (const Appointment& that) const;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_APPOINTMENT_H
