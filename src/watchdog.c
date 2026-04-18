// Header
#include "watchdog.h"

void hardFaultCallback (void);

// Configuration --------------------------------------------------------------------------------------------------------------

static const WDGConfig WDG1_CONFIG =
{
	.pr		= 0b011,	// 1kHz
	.rlr	= 1500		// 1.5s timeout at 1kHz
};

// Globals --------------------------------------------------------------------------------------------------------------------

static bool triggerWatchdog = false;

// Functions ------------------------------------------------------------------------------------------------------------------

void watchdogStart ()
{
	// If the device was just reset due to the watchdog, enter the fault state.
	if ((RCC->CSR & RCC_CSR_IWDGRSTF) == RCC_CSR_IWDGRSTF)
	{
		hardFaultCallback ();
		while (true);
	}

	wdgStart (&WDGD1, &WDG1_CONFIG);
}

void watchdogTrigger ()
{
	triggerWatchdog = true;
}

void watchdogReset ()
{
	if (!triggerWatchdog)
		wdgReset (&WDGD1);
}