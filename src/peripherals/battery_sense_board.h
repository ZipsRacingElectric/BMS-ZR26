#ifndef BATTERY_SENSE_BOARD_H
#define BATTERY_SENSE_BOARD_H

// Battery Sense Board --------------------------------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2026.01.14
//
// Description: Objects and functions related to the battery management system's battery sense board.

// TODO(Barach): How to do this?

// Includes -------------------------------------------------------------------------------------------------------------------

// Includes
#include "peripherals/spi/ltc6811.h"

// Constants ------------------------------------------------------------------------------------------------------------------

// Datatypes ------------------------------------------------------------------------------------------------------------------

typedef struct
{

} senseBoardConfig_t;

typedef struct
{
	ltc6811_t ltcLo;
	ltc6811_t ltcHi;
} senseBoard_t;

// Functions ------------------------------------------------------------------------------------------------------------------



#endif // BATTERY_SENSE_BOARD_H