/*
   arduino_dsmx.hpp : Spektrum DSMX support for Arduino flight controllers

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Hackflight is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with Hackflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "debug.hpp"
#include "receiver.hpp"
#include <SpektrumDSM.h>

static SpektrumDSM2048 rx;

namespace hf {

    class DSMX_Receiver : public Receiver {

        public:

            DSMX_Receiver(float trimRoll=0, float trimPitch=0, float trimYaw=0) : Receiver(trimRoll, trimPitch, trimYaw) { }

         protected:

            void begin(void)
            {
                rx.begin();
            }

            bool gotNewFrame(void)
            {
                return rx.gotNewFrame();
            }

            void readRawvals(void)
            {
                rx.getChannelValuesNormalized(rawvals, CHANNELS);
            }

            bool lostSignal(void)
            {
                return rx.timedOut();
            }

    }; // class DSMX_Receiver

} // namespace
