#ifndef MONITOR_THREAD_H
#define MONITOR_THREAD_H

// Battery Management System Vehicle Thread -----------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2026.01.14
//
// Description: Thread responsible for monitoring the battery management system while it is installed in the vehicle. This
//   thread measures cell voltages, pack voltage, pack current. TODO(Barach)

// TODO(Barach): rebrand

// Includes -------------------------------------------------------------------------------------------------------------------

// ChibiOS
#include "ch.h"

// Functions ------------------------------------------------------------------------------------------------------------------

/**
 * @brief Starts the vehicle BMS thread.
 * @param priority The priority to start the thread at.
 */
void monitorThreadStart (tprio_t priority);

#endif // MONITOR_THREAD_H