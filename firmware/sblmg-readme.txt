DISCLAIMER:
-----------
Unless it is marked as Beta, the Firmware is tested and validated by Roboteq and is believed to be fault-free.
The possibility always exists, however, that the particular configuration and/or use condition uncovers a fault
that escaped our validation test coverage. Always extensively test the firmware under your use conditions prior
to deploying it in the field.

3.0f 07/15/2026
--------------
- Improved MOSFail and STO indication functionality.
- Fixed Amps Limit configuration via CANopen.
- Fixed a Motor/Sensor fault issue triggered on E-Stop release when using an SSI sensor.
- Fixed ExtendedCAN message acceptance.
- Improved motor current readings when operating with a resolver.

3.0e 03/17/2026
--------------
- Fixed Modbus messaging when STO is enabled.
- Fixed a zero-speed issue in CLCP mode.
- Corrected erroneous behavior of the EtherCAT status word.
- Various minor fixes and improvements.

3.0d 12/23/2025
--------------
- Implemented EtherCAT communication.
- Fixed unintended power during NMT reset.
- Improvements for motor thermistor measurement.
- Fixed RoboCAN message acceptance issue.
- MultiPWM improvements for Pulse Input 4.

3.0c 10/15/2025
--------------
- Added CANOpen objects 0x310F, 0x3110, 0x3112, 0x3114.
- Improved short detection in single channel controllers.
- Fixed false triggering of DefConfig flag.
- Fixed DS402 objects with multiple indexes.
- Fixed MicroBasic script upload through RoboCAN.
- Added MicroBasic script upload through CANOpen.
- Improved Hall Sensor performance in low speeds.
- Fixed DS402 homing speed and status word.

3.0b 02/28/2025
--------------
- Fixed Analog Input Min/Center/Max behavior.
- Improved Resolver functionality.
- Improved RC5 and RC6 functionality.

3.0a 01/31/2025
--------------
- Fixed Modbus RS232 RTU Operation.
- Fixed LED pattern.
- Improved count position track during FetsOff.
- Improved handling for position counter overflow.
- Fixed DS402 Voltage Enabled bit functionality in SW.
- Fixed PWM Brake voltage calculation.
- Added CLCP to Encoder Stall condition check.
- Fixed Modbus ASCII for RS485.
- Fixed issue with STO fault when Hall trapezoidal.
- Fixed %STIME configuration and fixed RTC clock.
- Fixed EElog printing from RS485.
- Implemented 3 Phase Short as Loop Error Trigger Action.
- Added config option for thermistor pullup value.
- Added 330KHz clock option in SSI sensors.
- Optimized FDCAN interface.

3.0 01/22/2024
--------------
- Renamed Safety Stop command to Quick Stop.
- Fixed issue with changing states in DS402.
- Improved speed reading from the internal sensor in Hall.
- Updated VendorID & Device Name on USB.
- Fixed issues with Pulse and Analog readings for FBLG, SBLG, GBLG boards.
- Fixed Amps limit triggering mechanism in Torque mode.
- Fixed no tracking command when switching from CL Speed Position to Count Position mode.
- Fixed position relative mode with ROM command.
- Fixed STO Fault reporting issues in SBL(M)G boards.
- Fixed Modbus RTU over RS232.
- Fixed Modbus issue of reading negative value from s8, s16, and s32.
- Fixed Modbus write access violation for objects less than 4 bytes.
- Fixed the indetermined state when applying serial interface related configuration fields (DMOD, ISM, RSBR) from RS232, RS485, or ModBus.
- Added filtering for STO inputs.
- Fixed driver undervoltage and short fault on STO recovery and on power-up, when Clear Faults is disabled.
- Fixed short fault recovery in dual channel controllers.
- Improved field weakening operation.
- Fixed script auto-start timer.
- Fixed ramping in torque mode.
- Fixed Pulse input 6 on single channel controllers.
- Fixed TC command when applying negative values.
- Changed TC command to per thousand value of the rated torque of the motor for DS402.
- Fixed Amps trigger for channel 2.
- Fixed incorrect count reading in Trapezoidal.
- Fixed Battery Current sign.
- Fixed several isssues concerning SSI sesor handling (speed spikes, absolute feedback option, home command).
- Optimised speed limiting for torque mode.
- Changed OTL command (Over Temp) max value.
- Fixed Amps Limit maximum value for SBLG2 product family.
- Added speed and current PID output limits (slew rates).
- Fixed current sensing for single channel controllers.
- Fixed current limit triggering at constant power region.
- Fixed the torque mode performance when we have initial negative force at the shaft of the motor.
- Fixed motor distortion in torque mode when command watchdog expires.
- Fixed motor distortion in speed mode during power-up.
- Fixed misfunctionality when switching to closed loop speed mode.
- Fixed distorted acceleration/deceleration values when changing mode.
- Fixed incorrect COBID of disabled TPDOs.
- Optimised sin/cos and resolver sensor handling (delay compensation, speed calculation).
- Improved Analog Readings on SBLMG1,SBLG1,GBLG2 and FBLG2 product families.
- Fixed 5Volt reading for SBLMG1XXX.
- Enabled Ethernet and disabled CAN configuration fields by default for FBGL3XXXE and GBLG2XXXE.
- Fixed Exponential/Logarithmic Command Adjustments.
- Fixed the reported Ramp value in torque mode.
- Added change baud rate over RS232 and RS485.
- Changed TNM configuration command from mNm/Apeak to mNm/Arms.
- Fixed Clear Safety Stop on Count Position.
- Fixed tracking miscalculation when we send repeatable position command.
- Fixed motor start up after Loop Error, on HallEncoder.
- Fixed Motor Sensor Setup while other motor channel is running.
- Fixed IqCmd calculation for IPM motor in constant power region operation.
- Fix unwanted '+' print response, when configuration command was running from Script.

3.0.rc 07/19/2022
-----------------
- Implemented decoupling terms.
- Implemented automatic velocity and acceleration feedforward.
- Implemented multi-turn SSI encoders' support.
- Added Runtime commands and Queries for all PID gains.
- Implmentented FLCL in order to configure whether the faults will be cleared automatically or not.
- Implemented Sensor Fault Detection for SinCos/Resolver sensors.
- Implemented FEW and FET configuration commands for Loop Error Detection customisation.
- Implemented Emergency Messages support.
- Implemented EtherCAT support.
- Implemented Profinet support.
- Implemented Automatic Field Weakening Control.
- Added digital filter on encoder inputs.
