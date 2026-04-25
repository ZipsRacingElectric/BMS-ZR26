// Header
#include "receive.h"

// Includes
#include "peripherals.h"
#include "can/can_thread.h"
#include "can/eeprom_can.h"

// Constants ------------------------------------------------------------------------------------------------------------------

#define EEPROM_COMMAND_MESSAGE_ID 0x012

// Functions ------------------------------------------------------------------------------------------------------------------

int8_t receiveBmsMessage (void* arg, CANRxFrame* frame)
{
	// First argument is config
	const canThreadConfig_t* config = (canThreadConfig_t*) arg;

	if (frame->SID == EEPROM_COMMAND_MESSAGE_ID)
		eepromHandleCanCommand (frame, config->driver, (eeprom_t*) &virtualEeprom);

	return 0;
}