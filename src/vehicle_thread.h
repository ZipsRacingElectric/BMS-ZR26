#ifndef VEHICLE_THREAD_H
#define VEHICLE_THREAD_H

// Battery Management System Vehicle Thread -----------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2026.01.14
//
// Description: Thread responsible for monitoring the battery management system while it is installed in the vehicle. This
//   thread measures cell voltages, pack voltage, pack current. TODO(Barach)

// Includes -------------------------------------------------------------------------------------------------------------------

// ChibiOS
#include "ch.h"

// Functions ------------------------------------------------------------------------------------------------------------------

/**
 * @brief Starts the vehicle BMS thread.
 * @param priority The priority to start the thread at.
 */
void vehicleThreadStart (tprio_t priority);

#endif // VEHICLE_THREAD_H