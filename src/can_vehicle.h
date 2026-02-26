#ifndef CAN_VEHICLE_H
#define CAN_VEHICLE_H

// Vehicle CAN Interface ------------------------------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2025.04.01
//
// Description: CAN interface for when the accumulator is in the vehicle. This bus runs at 1 Mbps.

// Includes -------------------------------------------------------------------------------------------------------------------

// Includes
#include "can/amk_inverter.h"

// ChibiOS
#include "ch.h"

// Constants ------------------------------------------------------------------------------------------------------------------

/// @brief The number of AMK inverters on the CAN bus.
#define AMK_COUNT 4

// Global Nodes ---------------------------------------------------------------------------------------------------------------

/// @brief The rear-left AMK inverter.
#define amkRl (amks [0])

/// @brief The rear-right AMK inverter.
#define amkRr (amks [1])

/// @brief The front-left AMK inverter.
#define amkFl (amks [2])

/// @brief The front-right AMK inverter.
#define amkFr (amks [3])

/// @brief Array of all the AMK inverters.
extern amkInverter_t amks [AMK_COUNT];

// Functions ------------------------------------------------------------------------------------------------------------------

bool canVehicleInit (tprio_t priority);

#endif // CAN_VEHICLE_H