/*
 * Copyright 2014 Canonical Ltd.
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

#ifndef AYATANA_INDICATOR_NOTIFICATIONS_SOUND_H
#define AYATANA_INDICATOR_NOTIFICATIONS_SOUND_H

#include <memory>
#include <string>

namespace ayatana {
namespace indicator {
namespace notifications {

/***
****
***/

/**
 * Plays a sound, possibly looping.
 *
 * @param uri the file to play
 * @param volume the volume at which to play the sound, [0..100]
 * @param loop if true, loop the sound for the lifespan of the object
 */
class Sound
{
public:
    Sound(const std::string& role, const std::string& uri, unsigned int volume, bool loop);
    ~Sound();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

/***
****
***/

class SoundBuilder
{
public:
    SoundBuilder() =default;
    virtual ~SoundBuilder() =default;
    virtual std::shared_ptr<Sound> create(const std::string& role, const std::string& uri, unsigned int volume, bool loop) =0;
};

class DefaultSoundBuilder: public SoundBuilder
{
public:
    DefaultSoundBuilder() =default;
    ~DefaultSoundBuilder() =default;
    virtual std::shared_ptr<Sound> create(const std::string& role, const std::string& uri, unsigned int volume, bool loop) override {
        return std::make_shared<Sound>(role, uri, volume, loop);
    }
};

/***
****
***/

} // namespace notifications
} // namespace indicator
} // namespace ayatana

#endif // AYATANA_INDICATOR_NOTIFICATIONS_SOUND_H
