#ifndef PRECHARGE_LOGIC_H
#define PRECHARGE_LOGIC_H

// Precharge Logic ------------------------------------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2026.02.24
//
// Description: Functions responsible for handling the accumulator's precharge logic. Precharge is the process of charging the
//   inverters' DC bus before closing both isolation relays. This must be done in order to prevent a large current inrush. See
//   FSAE rules for more info.
//
// Note: Care must be taken when modifying the logic in this file. There is no hardware safeguard for closing the positive
//   isolation relay early.

// Includes -------------------------------------------------------------------------------------------------------------------

// ChibiOS
#include "ch.h"

// Functions ------------------------------------------------------------------------------------------------------------------

/**
 * @brief Gets the tractive system DC bus voltage on the inverter-side of the isolation relays.
 * @note This can only be called when the BMS is installed in the vehicle.
 * @return The minimum voltage of the bus (minimum of the 4 reported voltages).
 */
float prechargeGetInverterVoltage (void);

/**
 * @brief Gets the tractive system DC bus voltage on the charger-side of the isolation relays.
 * @note This can only be called when the BMS is installed on the charger.
 * @return The reported voltage of the bus.
 */
float prechargeGetChargerVoltage (void);

/**
 * @brief Checks whether the precharge sequence is completed or not. Precharge is complete if both the negative isolation relay
 * is closed and the inverter/charger voltage exceeds the precharge threshold.
 * @param accumulatorVoltage The voltage of the accumulator, as measured by the LTC ICs.
 * @param inverterVoltage The voltage of the inverter/charger, as measured by said device(s). Use
 * @c prechargeGetInverterVoltage or @c prechargeGetChargerVoltage to get this value.
 * @return True if the positive isolation relay can be closed, false otherwise.
 */
bool prechargeCheck (float accumulatorVoltage, float inverterVoltage);

#endif // PRECHARGE_LOGIC_H