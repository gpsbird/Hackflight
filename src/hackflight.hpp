/*
   hackflight.hpp : general header, plus init and update methods

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Hackflight is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MEReceiverHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with Hackflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cmath>

#include "board.hpp"
#include "mixer.hpp"
#include "receiver.hpp"
#include "stabilizer.hpp"
#include "debug.hpp"
#include "datatypes.hpp"
#include "altitude.hpp"

namespace hf {

    class Hackflight {

        private: 

            // Passed to Hackflight::init() for a particular board and receiver
            Board      * board;
            Receiver   * receiver;
            Stabilizer * stabilizer;

            // Altitude-estimation task
            // NB: Try ALT P 50; VEL PID 50;5;30
            // based on https://github.com/betaflight/betaflight/issues/1003 (Glowhead comment at bottom)
            AltitudeEstimator altitudeEstimator = AltitudeEstimator(
                    15,  // Alt P
                    15,  // Vel P
                    15,  // Vel I
                    1);  // Vel D 

            // Eventually we might want to support mixers for different kinds of configurations (tricopter, etc.)
            Mixer      mixer;

            // Vehicle state
            float eulerAngles[3];
            bool armed;

            // Auxiliary switch state for change detection
            uint8_t auxState;

            // Safety
            bool failsafe;

            // Support for headless mode
            float yawInitial;

            uint32_t gcount, acount, qcount, bcount, rcount;

            bool safeAngle(uint8_t axis)
            {
                return fabs(eulerAngles[axis]) < stabilizer->maxArmingAngle;
            }

            void checkEulerAngles(void)
            {
                if (board->getEulerAngles(eulerAngles)) {

                    qcount++;

                    // Convert heading from [-pi,+pi] to [0,2*pi]
                    if (eulerAngles[AXIS_YAW] < 0) {
                        eulerAngles[AXIS_YAW] += 2*M_PI;
                    }

                    // Update stabilizer with new Euler angles
                    stabilizer->updateEulerAngles(eulerAngles);

                    // Do serial comms
                    board->doSerialComms(eulerAngles, armed, receiver, &mixer);
                }
            }

            void checkGyroRates(void)
            {
                float gyroRates[3];

                if (board->getGyroRates(gyroRates)) {

                    gcount++;

                    // Start with demands from receiver
                    demands_t demands;
                    memcpy(&demands, &receiver->demands, sizeof(demands_t));

                    // Run stabilization to get updated demands
                    stabilizer->modifyDemands(gyroRates, demands);

                    // Run altitude estimator PIDs
                    altitudeEstimator.modifyDemands(demands);

                    // Sync failsafe to gyro loop
                    checkFailsafe();

                    // Use updated demands to run motors
                    if (armed && !failsafe && !receiver->throttleIsDown()) {
                        mixer.runArmed(demands);
                    }
                }
            }

            void checkBarometer(void)
            {
                float pressure;
                if (board->getBarometer(pressure)) {
                    bcount++;
                    altitudeEstimator.updateBaro(armed, pressure, board->getMicroseconds());
                }
            }

            void checkAccelerometer(void)
            {
                float accelGs[3];
                if (board->getAccelerometer(accelGs)) {
                    acount++;
                    altitudeEstimator.updateAccel(accelGs, board->getMicroseconds());
                    //Debug::printf("%+3.3f    %+3.3f    %+3.3f\n", accelGs[0], accelGs[1], accelGs[2]);
                }
            }

            void checkFailsafe(void)
            {
                if (armed && receiver->lostSignal()) {
                    mixer.cutMotors();
                    armed = false;
                    failsafe = true;
                    board->showArmedStatus(false);
                }
            } 

            void checkReceiver(void)
            {
                // Acquire receiver demands, passing yaw angle for headless mode
                if (!receiver->getDemands(eulerAngles[AXIS_YAW] - yawInitial)) return;

                rcount++;

                // Update stabilizer with cyclic demands
                stabilizer->updateDemands(receiver->demands);

                // When landed, reset integral component of PID
                if (receiver->throttleIsDown()) {
                    stabilizer->resetIntegral();
                }

                // Disarm
                if (armed && receiver->disarming()) {
                    armed = false;
                } 

                // Arm (after lots of safety checks!)
                if (!armed && receiver->arming() && !auxState && !failsafe && safeAngle(AXIS_ROLL) && safeAngle(AXIS_PITCH)) {
                    armed = true;
                    yawInitial = eulerAngles[AXIS_YAW]; // grab yaw for headless mode
                }

                // Detect aux switch changes for altitude-hold, loiter, etc.
                if (receiver->demands.aux != auxState) {
                    auxState = receiver->demands.aux;
                    altitudeEstimator.handleAuxSwitch(receiver->demands);
                }

                // Cut motors on throttle-down
                if (armed && receiver->throttleIsDown()) {
                    mixer.cutMotors();
                }

                // Set LED based on arming status
                board->showArmedStatus(armed);

            } // checkReceiver

        public:

            void init(Board * _board, Receiver * _receiver, Stabilizer * _stabilizer)
            {  
                // Store the essentials
                board = _board;
                receiver = _receiver;
                stabilizer = _stabilizer;

                // Do hardware initialization for board
                board->init();

                // Initialize the receiver
                receiver->init();

                // Initialize our stabilization, mixing, and MSP (serial comms)
                stabilizer->init();
                mixer.init(board); 

                // Initialize the atitude estimator
                altitudeEstimator.init();

                // Start unarmed
                armed = false;
                failsafe = false;

            } // init

            void update(void)
            {
                //Debug::printf("G: %d    A: %d    Q: %d    B: %d    R: %d\n", gcount, acount, qcount, bcount, rcount);

                checkGyroRates();
                checkEulerAngles();
                checkReceiver();
                checkAccelerometer();
                checkBarometer();
            } 

    }; // class Hackflight

} // namespace
