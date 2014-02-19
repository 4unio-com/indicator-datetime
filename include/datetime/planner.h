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

#ifndef INDICATOR_DATETIME_PLANNER_H
#define INDICATOR_DATETIME_PLANNER_H

#include <datetime/appointment.h>
#include <datetime/date-time.h>

#include <core/property.h>

#include <vector>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Simple appointment book
 *
 * @see EdsPlanner
 * @see State
 */
class Planner
{
public:
    virtual ~Planner() =default;

    /**
     * \brief Timestamp used to determine the appointments in the `upcoming' and `this_month' properties.
     * Setting this value will cause the planner to re-query its backend and
     * update the `upcoming' and `this_month' properties.
     */
    core::Property<DateTime> time;

    /**
     * \brief The next few appointments that follow the time specified in the time property.
     */
    core::Property<std::vector<Appointment>> upcoming;

    /**
     * \brief The appointments that occur in the same month as the time property
     */
    core::Property<std::vector<Appointment>> this_month;

protected:
    Planner() =default;

private:

    // disable copying
    Planner(const Planner&) =delete;
    Planner& operator=(const Planner&) =delete;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_PLANNER_H