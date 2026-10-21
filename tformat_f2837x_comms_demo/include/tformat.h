//###########################################################################
//
// FILE:   tformat.h
//
// TITLE:  Tamagawa T-Format System Example Support Functions
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

#ifndef _TFORMAT_SYSTEM_EXAMPLE_H
#define _TFORMAT_SYSTEM_EXAMPLE_H

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

#include "PM_tformat_include.h"
#include "driverlib.h"
#include "device.h"

//*****************************************************************************
//
// [doc-sys-config-start]
//
// Indicate which commands should be run.  This can be helpful
// for debug or benchmarking
// 1 == run command
// 0 == skip command
//
//*****************************************************************************
#define TFORMAT_RUN_ID0     1
#define TFORMAT_RUN_ID1     1
#define TFORMAT_RUN_ID2     1
#define TFORMAT_RUN_ID3     1
#define TFORMAT_RUN_IDD_ID6 1  // EEPROM read and write
#define TFORMAT_RUN_ID7     1
#define TFORMAT_RUN_ID8     1
#define TFORMAT_RUN_IDC     1

//*****************************************************************************
//
// The duration of a single bit is 2.5 MHz (400ns)
// First determine the number of CLB clocks within
// a 400 ns period. Then divide this by 2, in order
// to create a clock within the CLB with a 400ns period.
//
// Define the number of CLB clocks required to generate for the bus clock
// Define the number of CLB clocks within a bit-width
//
//*****************************************************************************
#if defined(_F2837X)
#define TFORMAT_CLB_CLOCK       100ul // MHz

#elif defined(_F2838X)
#define TFORMAT_CLB_CLOCK       100ul // MHz

#elif defined(_F28004X)
#define TFORMAT_CLB_CLOCK       100ul // MHz

#elif defined(_F28003X)
#define TFORMAT_CLB_CLOCK       120ul // MHz

#elif defined(_F28002X)
#define TFORMAT_CLB_CLOCK       100ul // MHz

#elif defined(_F28P65X)
#define TFORMAT_CLB_CLOCK       100ul // MHz

#else
#error Specify the CLB clock rate for the specific device
#endif
#define TFORMAT_BUS_CLOCK       2.5f // MHz
#define TFORMAT_FREQ_DIVIDER    (TFORMAT_CLB_CLOCK /                           \
                                    (uint16_t) (2.0f * TFORMAT_BUS_CLOCK) )
#define TFORMAT_BIT_WIDTH       (TFORMAT_CLB_CLOCK /                           \
                                    (uint16_t) (TFORMAT_BUS_CLOCK) )

//*****************************************************************************
//
// For EEPROM read/write testing, specify the range of addresses to be used
//
// Note: Select addresses between 0 and 126 (0x7E).
//       Address 127 (0x7F) is the EEPROM page control.
//
//*****************************************************************************
#define TFORMAT_EEPROM_TEST_ADDR_START  0x73u
#define TFORMAT_EEPROM_TEST_ADDR_END    0x7Eu

//*****************************************************************************
//
// Delay time, in micro-seconds, between each command sent
// The increment is used to vary the time between commands for testing
//
//*****************************************************************************
#define TFORMAT_IDLE_DELAY_US_START       10ul
#define TFORMAT_IDLE_DELAY_US_INCREMENT   15ul

//*****************************************************************************
//
// Time to wait after a write to the EEPROM before another write/read
//
//*****************************************************************************
#define TFORMAT_EEPROM_BUSY_US 18000ul

//*****************************************************************************
//
// Power-on to normal operation delay 1.5 seconds. Refer to the data manual
// for a specific encoder.
//
//*****************************************************************************
#define TFORMAT_POWER_ON_DELAY_US 1500000ul

//
// [doc-sys-config-end]
//*****************************************************************************
//
// Global structures used to share information to the user through
// the Code Composer Studio expressions window.
//
//! [TFORMAT_ENCODER_INFO_STRUCT_SNIPPET]
//
typedef struct PM_tformat_encoderStruct {
    uint32_t  position;
    uint32_t  turns;
    uint16_t  id;
} PM_tformat_encoderStruct;

//
//! [TFORMAT_ENCODER_INFO_STRUCT_SNIPPET]
//! [TFORMAT_ENCODER_ERROR_STRUCT_SNIPPET]
//
typedef struct PM_tformat_errorStruct {
    uint16_t  idCheckError;
    uint16_t  crcCheckError;
    uint16_t  eepromAddressError;
    uint16_t  eepromDataError;
} PM_tformat_errorStruct;

//
//! [TFORMAT_ENCODER_ERROR_STRUCT_SNIPPET]
//! [TFORMAT_COMMAND_STRUCT_SNIPPET]
//
typedef struct PM_tformat_commandStruct {
    uint32_t  controlField;
    uint32_t  eepromAddress;
    uint16_t  eepromData;
} PM_tformat_commandStruct;

//
//! [TFORMAT_COMMAND_STRUCT_SNIPPET]
//
//*****************************************************************************
//! \addtogroup TFORMAT_SYS_GLOBAL_STRUCTURES Information reporting structures
//!
//! Information reporting structures used by the system example.
//! @{
//*****************************************************************************
//*****************************************************************************
//!
//! The position, turns and encoder ID are updated
//! after receiving this information back from the encoder.
//!
//! \snippet this TFORMAT_ENCODER_INFO_STRUCT_SNIPPET
//!
//! \sa
//!     * System example function: tformat_exCommands()
//!     * API function: PM_tformat_updatePositionOrTurns()
//!     * API function: PM_tformat_updateEncoderID()
//
//*****************************************************************************
extern PM_tformat_encoderStruct encoderData;

//*****************************************************************************
//!
//! The variables within this structure are set to 1 if a specific error is
//! detected by the tformat_checkError() function.
//!
//! \snippet this TFORMAT_ENCODER_ERROR_STRUCT_SNIPPET
//!
//! \sa System example functions:
//!     * tformat_exCommands()
//!     * tformat_checkError()
//!
//
//*****************************************************************************
extern PM_tformat_errorStruct errorData;

//*****************************************************************************
//!
//! Global structure used to track the command data sent. This information
//! can be used for error checking after the response is received.
//!
//! \snippet this TFORMAT_COMMAND_STRUCT_SNIPPET
//!
//! \sa System example functions:
//!     * tformat_testEEPROMCommands()
//!     * tformat_exCommands()
//!     * tformat_checkError()
//
//*****************************************************************************
extern PM_tformat_commandStruct commandData;

//*****************************************************************************
//!
//! Global variable which increments once each time the SPI RX
//! interrupt is taken.
//!
//
//*****************************************************************************
extern volatile uint32_t tformatSpiRxIsrTicker;

//
//! @} //end addtogroup TFORMAT_SYS_GLOBAL_STRUCTURES
//

//*****************************************************************************
//! \addtogroup TFORMAT_SYS_RUN System example functions
//!
//! Functions to demonstrate usage of the T-Format encode interface library
//! @{
//*****************************************************************************

//*****************************************************************************
//!
//! \brief Execute read-out and reset commands
//!
//! This function is the main test for the system example.  It tests the
//! following command IDs: ID0, ID1, ID2, ID3, ID7, ID8 and IDC
//!
//! For each command ID:
//!
//! 1. Clear the tformatData structure (optional)
//!    Not required, but useful for debug.
//! 2. Wait...The idle time between commands is for demo purposes.
//!    The idle time is not tied to any specification
//! 3. Setup the SPI and CLB for the command
//! 4. Start the operation
//! 5. Wait for the dataReady flag to be set by the SPI RX ISR.
//! 6. Unpack the data from the SPI FIFO into the tformatData structure
//! 7. Check for errors using tformat_checkError();
//! 8. Update the turns, position, and ID when appropriate by calling the
//!    PM_tformat_updatePositionOrTurns() and PM_tformat_updateEncoderID()
//!    library functions.
//!
//! This function runs in a infinite while() loop.
//!
//! \sa The EEPROM read and write commands are tested in
//!     the function tformat_testEEPROMCommands()
//!
//! \return None.
//
//*****************************************************************************
void tformat_exCommands(void) __attribute__((ramfunc))
                              __attribute__((noreturn));

//*****************************************************************************
//!
//! \brief Execute EEPROM read and write commands
//!
//! Test read (ID6) and write (IDD) EEPROM commands.
//!
//! 1. Write all 0's to EEPROM address in the range defined
//!    by TFORMAT_EEPROM_TEST_ADDR_START and
//!    TFORMAT_EEPROM_TEST_ADDR_END.
//! 2. Read out the data and check that all
//!    data read is 0x0
//! 3. Write a decrementing pattern: 0xFF, 0xFE, 0xFD ...
//!    to the EEPROM address range
//! 4. Read out the pattern and check that the data
//!    matches what was written.
//!
//! After each read or write:
//!
//!   Check the response from the encoder against the command.
//!   The controlField (CF), eepromAddressField (ADF)
//!   and eepromDataField (EDF) received from the encoder
//!   should match the command that was sent.
//!
//! Note: Some write attempts will occur when
//!   the EEPROM is busy.  In this
//!   case the PM_TFORMAT_EEPROM_BUSY flag will
//!   be set and the write will be re-tried by
//!   the test.
//!
//! All of the other commands are tested in the tformat_exCommands() function.
//!
//! \return None.
//!
//
//*****************************************************************************
void tformat_testEEPROMCommands(void) __attribute__((ramfunc));

//*****************************************************************************
//!
//! \brief Basic error check for received fields
//!
//! Provides basic checks of the following:
//!
//! 1. The received control field (CF) should match the request CF
//! 2. The calculated CRC for the data should match the received CRC field
//! 3. For EEPROM read and write commands, check the request address
//!    and data fields against the received fields.
//!
//! \param controlFieldID  The control field (CF) of the request in the format
//! PM_TFORMAT_CFIDx.  For example PM_TFORMAT_CFID6 for command ID6.
//!
//*****************************************************************************
uint16_t tformat_checkError(uint16_t controlFieldID) __attribute__((ramfunc));

//*****************************************************************************
//!
//! \brief Service the SPI FIFO receive interrupt
//!
//! This interrupt service routine will move the contents of the SPI RX FIFO
//! into the tformatRxData array. Each FIFO word is masked with 0xFF to
//! extract only the 8-bits of data corresponding to the for the specific
//! T-Format field.
//!
//! \note This interrupt service routine is defined as a high-priority
//! interrupt for performance reasons.  This means the  ISR can not be
//! interrupted by another ISR (no nesting allowed). Refer to the
//! <a href="https://www.ti.com/lit/spru514"> C2000 compiler guide </a>
//! for more information.
//!
//
//*****************************************************************************
void tformat_spiRxISR(void) __attribute__((interrupt("HPI")))
                            __attribute__((ramfunc));


//
//! @} //end addtogroup TFORMAT_SYS_RUN
//

//*****************************************************************************
//! \addtogroup TFORMAT_SYS_INIT System example functions
//!
//! Functions to demonstrate usage of the T-Format encode interface library
//! @{
//*****************************************************************************
//*****************************************************************************
//!
//! \brief Initialize the T-Format subsystem
//!
//
//*****************************************************************************
void tformat_init(void);

//*****************************************************************************
//!
//! \brief Configure the SPI peripheral
//!
//! Configures the PM_TFORMAT_SPI peripheral as required for T-Format
//! communication
//!
//! \param devLSPCLKFreq SPI clock rate
//! \return None.
//!
//*****************************************************************************
void tformat_initSPIFIFO(uint32_t devLSPCLKFreq);


#if defined(_F2837X)
//*****************************************************************************
//!
//! \brief System ePWM trip configuration
//!
//!
//! This function is only used on F2837xD. Configure EPWM4 to drive a default
//! value on GPIO7 before the CLB tile takes over the output.
//!
//! The CLB Tile_4.out2 will override the EPWM4B output
//! to reach GPIO7.
//!
//!
//! \return None.
//!
//
//*****************************************************************************
void tformat_configEPWM4(void);
#endif

//
//! @} //end addtogroup TFORMAT_SYS_RUN
//

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif // PM_TFORMAT_SYSTEM_EXAMPLE_H
//

