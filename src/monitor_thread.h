#ifndef MONITOR_THREAD_H
#define MONITOR_THREAD_H

// ChibiOS
#include "ch.h"

void monitorThreadStart (tprio_t priority);

void monitorThreadSetRollingAverageCount (uint16_t count);

#endif // MONITOR_THREAD_H