# Battery Management System - Zips Racing ZR25
Embedded firmware for the battery management system of ZR25. The BMS is responsible for monitoring:

- All series cell voltages
- All sense line statuses (open circuit)
- The temperature of at least 20% of all cells

To do this, the BMS uses the LTC68XX series of battery management ICs. These ICs act like an isolated I/O expander for the microcontroller, allowing all of the above measurements to be performed.

## Usage
For help on how to setup this project, see the below file:

[Firmware Toolchain Setup Guide (common/doc/firmware_toolchain_guide.md)](https://github.com/ZipsRacingElectric/STM32F405-Common/blob/main/doc/firmware_toolchain_guide.md)

## Directory Structure
```
.
├── build                               - Directory for compilation output.
├── common                              - STM32 common library, see the readme in here for more details.
├── config                              - ChibiOS configuration files.
│   ├── board.chcfg                     - Defines the pin mapping and clock frequency of the board.
│   ├── chconf.h                        - ChibiOS RT configuration.
│   ├── halconf.h                       - ChibiOS HAL configuration.
│   ├── mcuconf.h                       - ChibiOS HAL driver configuration.
├── doc                                 - Documentation folder.
│   ├── chibios                         - ChibiOS documentation.
│   ├── datasheets                      - Datasheets of important components on this board.
│   ├── schematics                      - Schematics of this and related boards.
│   └── software                        - Software documentation.
├── makefile                            - Makefile for this application.
└── src                                 - C source / include files.
    ├── can                             - Code related to this device's CAN interface. This defines the messages this board
    │                                     transmits and receives.
    └── peripherals                     - Code related to board hardware and peripherals.
```