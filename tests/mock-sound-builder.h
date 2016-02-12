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

#include "mock-sound.h"

#include <notifications/sound.h>

namespace unity {
namespace indicator {
namespace notifications {

/***
****
***/

/**
 * A DefaultSoundBuilder wrapper which remembers the parameters of the last sound created.
 */
class MockSoundBuilder: public unity::indicator::notifications::SoundBuilder
{
public:
    MockSoundBuilder() =default;
    ~MockSoundBuilder() =default;

    virtual std::shared_ptr<unity::indicator::notifications::Sound> create(const std::string& role, const std::string& uri, bool loop) override {
        m_role = role;
        m_uri = uri;
        m_loop = loop;
        return std::make_shared<MockSound>(role, uri, loop);
    }

    const std::string& role() { return m_role; }
    const std::string& uri() { return m_uri; }
    bool loop() { return m_loop; }

private:
    std::string m_role;
    std::string m_uri;
    bool m_loop;
};

/***
****
***/

} // namespace notifications
} // namespace indicator
} // namespace unity

