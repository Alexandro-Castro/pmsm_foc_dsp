//###########################################################################
//
// FILE:   pm_tformat_main.c
//
// TITLE:  Tamagawa T-Format System Example
//
//###########################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated - http://www.ti.com/
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//   Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the
//   distribution.
//
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//###########################################################################


#include "tformat.h"
#include "device.h"
#include "driverlib.h"

    //
    // [doc-library-globals-start]
    //
    // Globals used by the library
    // - Lookup table for CRC calculations
    // - tformatData structure to track key parameters.
    //
    PM_tformat_DataStruct tformatData;
    uint16_t tformatCRCtable[PM_TFORMAT_CRCTABLE_SIZE];
    volatile uint16_t tformatRxData[PM_TFORMAT_FIELDS_MAX];

    //
    // [doc-library-globals-end]
    //

    //
    // [doc-system-globals-start]
    //
    // These structures are used to display the received
    // data in Code Composer Studio's Expressions
    // window during development.  Because the structure is
    // set, but never used, the pragma RETAIN
    // has been applied.  Volatile is used for the
    // tformatSpiRxIsrTicker since it is updated within
    // the SPI ISR.
    //
    #pragma RETAIN (encoderData);
    #pragma RETAIN (errorData);
    #pragma RETAIN (commandData);

    PM_tformat_encoderStruct encoderData;
    PM_tformat_errorStruct errorData;
    PM_tformat_commandStruct commandData;
    volatile uint32_t tformatSpiRxIsrTicker;

    //
    // [doc-system-globals-end]
    //

int16_t main(void) {

    //
    // Initialize device clocks and peripherals
    // Disable interrupts
    // Initialize PIE and clear PIE registers.
    //
    // Initialize the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISRs).
    //
    // Disable pin locks and enable internal pull-ups.
    //
    Device_init();
    Interrupt_disableMaster();
    Interrupt_initModule();
    Interrupt_initVectorTable();
    Device_initGPIO();

    //
    // On this device, the CLB TILE is clocked by the
    // corresponding ePWM module.
    //
    // The CLB will override the EPWM4B output pin
    // and EPWM4B_OE (output enable).  Otherwise the ePWM4
    // functionality is not used.
    //
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_EPWM4);
    tformat_configEPWM4();

    //
    // Initialization routine for the T-Format Encoder Interface
    // Configures and enables clocks
    // Configures GPIO, XBAR, SPI and CLB
    // Registers the SPI RX interrupt
    //
    tformat_init();

    //
    // Enable interrupts
    //
    Interrupt_enableMaster();

    //
    // Clear the error and encoder data before we start
    //
    errorData = (struct PM_tformat_errorStruct) {0};
    encoderData = (struct PM_tformat_encoderStruct) {0};
    tformatSpiRxIsrTicker = 0;
#if TFORMAT_RUN_IDD_ID6

    //
    // Test read/write to the encoder's EEPROM memory
    //
    tformat_testEEPROMCommands();
#endif

    //
    // Test all other commands in an infinite loop
    //
    tformat_exCommands();
}
