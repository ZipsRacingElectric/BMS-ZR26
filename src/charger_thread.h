#ifndef CHARGER_THREAD_H
#define CHARGER_THREAD_H

// Battery Management System Charger Thread -----------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2026.01.15
//
// Description: TODO(Barach)

// Includes -------------------------------------------------------------------------------------------------------------------

// ChibiOS
#include "ch.h"

// Functions ------------------------------------------------------------------------------------------------------------------

/**
 * @brief Starts the charger BMS thread.
 * @param priority The priority to start the thread at.
 */
void chargerThreadStart (tprio_t priority);

#endif // CHARGER_THREAD_H