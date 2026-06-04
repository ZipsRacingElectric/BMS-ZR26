# ZR26 BMS Commissioning Guide

## Setup

This guide assumes the user has the latest version of ZRE-CAN-Tools installed.

**IMPORTANT**: This test must be performed in a low-voltage environment. The positive / negative IRs cannot be connected to any high-voltage circuitry.

**IMPORTANT**: Do not connect the IMD or current sensor connectors until the *Initial Power On* test has been performed.

## Testing

### Initial Power On

Validate there is no smoke released and the current draw is normal (less than 150mA).

With a multimeter, measure the IMD 12V rail and validate it is 12V (within reason).

With a multimeter, measure the current sensor 5V rail and validate it is 5V (within reason).

### Software Programming

Program the microcontroller with the latest firmware.
- Validate the heartbeat LED is powered and flashing.

- Via the `dashboard-gui`, validate the BMS broadcasts the status message and the status reports `EEPROM UNPROGRAMMED`.

### EEPROM Programming

Using ZRE-CAN-Tools, program the device's EEPROM.

- Using the `zr26-eeprom-vehicle` command, set the `WATCHDOG_ENABLED` bit to `0` to disable the watchdog.

- Navigate to the `ZRE_CANTOOLS_DIR` directory.
- Run the below command (substituting the `<Device Name>` as nececssary).

```
./bin/can-eeprom-cli -p=config/zr26/bms_data.json <Device Name> config/zr26/bms_config.json
```

- This command should run with no errors. If it times out, you may need to program the fields manually.

- Print the EEPROM map via the `zr26-eeprom-vehicle` command, option `m`.

### Shutdown Loop

Start with all shutdown switches disabled. Validate on the `dashboard-gui` all shutdown switches are opened.

- Activate vehicle shutdown switch.
	- Validate on the `dashboard-gui` switch indicates correctly.
- Plug in the IMD connector.
	- Validate on the `dashboard-gui` the IMD switch is still opened.
- Write `1` to `BMS_RELAY_ENABLE`.
	- Validate on the `dashboard-gui` the BMS switch is still opened.
- Press the TS reset button.
	- Validate on the `dashboard-gui` the IMD and BMS switches are closed.
- Validate positive / negative IRs are both unpowered
- Active the MSD/TSMS shutdown switch.
	- Validate on the `dashboard-gui` the switch indicates correctly.
- Validate the negative IR is powered and the positive IR is unpowered.
- Write `1` to `POSITIVE_IR_ENABLE`.
- Validate the positive IR is powered.

### Faults

While the shutdown loop is closed:

- Unplug the IMD connector.
	- Validate the negative IR is unpowered.
- Plug the IMD connector back in.
	- Validate the negative IR does not re-power.
- Press the TS reset button.
	- Validate the negative IR is powered.

- Write `0` to `BMS_RELAY_ENABLE`.
	- Validate the negative IR is unpowered.
- Write `1` to `BMS_RELAY_ENABLE`.
	- Validate the negative IR does not re-power.
- Press the TS reset button.
	- Validate the negative IR is powered.

### TSSI, RTM, & Voltage Indicator

- With no faults asserted, validate the TSSI is solid green.
- Unplug the IMD connector to assert a fault.
- Validate the TSSI flashes red at 2-5 Hz.

- Validate that the voltage indicator and ready-to-move light are both unpowered.
- Energize the high-voltage inverter positive / negative sense lines with a voltage greater than 60V.
	- This voltage must be isolated from the GLV power supply.
- Validate the voltage indicator and ready-to-move light are both powered.
- Validate the ready-to-move light flashes at 2-5Hz.