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

#include "mock-sound.h"

namespace unity {
namespace indicator {
namespace notifications {

/***
****
***/

MockSound::~MockSound()
{
}

std::string MockSound::m_last_role;
std::string MockSound::m_last_uri;
bool MockSound::m_last_loop = false;

/***
****
***/

} // namespace notifications
} // namespace indicator
} // namespace unity
