//###########################################################################
//
// FILE:   pm_tformat_crc.c
//
// TITLE:  Tamagawa T-Format Encoder Interface CRC
//
//###########################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated
//     http://www.ti.com/ ALL RIGHTS RESERVED
// $
//###########################################################################

#include "PM_tformat_include.h"
#include "PM_tformat_internal_include.h"

#if defined(PM_TFORMAT_RX_CRC_BY_C28X)
uint16_t
tformat_getCRCID2()
{
    uint32_t crcCheck;
    //
    // The CRC is performed by the C28x CPU
    // Create a rxPacket to calculate the expected CRC of the
    // received data.  The calculated CRC is compared with the
    // CRC received from the encoder
    //
    uint32_t rxPkts;
    rxPkts = ((uint32_t) tformatData.controlField << 16)
                            | ((uint32_t) tformatData.statusField << 8)
                            | ((uint32_t) tformatData.dataField0);

    crcCheck = tformat_getCRC(PM_TFORMAT_RX_CRC_BITS_ID2,
                              (uint16_t *)&rxPkts,
                              tformatCRCtable,
                              PM_TFORMAT_RX_CRC_BYTES_ID2);
    return(crcCheck);
}
#endif

#if defined(PM_TFORMAT_RX_CRC_BY_C28X)
uint16_t
tformat_getCRCID3()
{
    uint32_t crcCheck;
    //
    // The CRC is performed by the C28x CPU
    // Create a rxPacket to calculate the expected CRC of the
    // received data.  The calculated CRC is compared with the
    // CRC received from the encoder
    //
    uint32_t rxPkts[3];
    rxPkts[0] = ((uint32_t) tformatData.controlField << 24UL)
                          | ((uint32_t) tformatData.statusField << 16UL)
                          | ((uint32_t) tformatData.dataField0 << 8UL)
                          | (uint32_t)  tformatData.dataField1;
    rxPkts[1] = ((uint32_t) tformatData.dataField2 << 24UL) |
                            ((uint32_t) tformatData.dataField3 << 16UL) |
                            ((uint32_t) tformatData.dataField4 << 8UL) |
                            ((uint32_t) tformatData.dataField5);
    rxPkts[2] = ((uint32_t) tformatData.dataField6 << 8UL) |
                              ((uint32_t) tformatData.dataField7);


    crcCheck = tformat_getCRC(PM_TFORMAT_RX_CRC_BITS_ID3,
                              (uint16_t *)&rxPkts,
                              tformatCRCtable,
                              PM_TFORMAT_RX_CRC_BYTES_ID3);
    return(crcCheck);
}
#endif

#if defined(PM_TFORMAT_RX_CRC_BY_C28X)
uint16_t
tformat_getCRCIDD()
{
    uint16_t crcCheck;
    //
    // The CRC is performed by the C28x CPU
    // Create a rxPacket to calculate the expected CRC of the
    // received data.  The calculated CRC is compared with the
    // CRC received from the encoder
    //
    uint32_t rxPkts;
    rxPkts = ((uint32_t) tformatData.controlField << 16)
                          | ((uint32_t) tformatData.eepromAddressField << 8)
                          | ((uint32_t) tformatData.eepromRdDataField);

    crcCheck = tformat_getCRC(PM_TFORMAT_RX_CRC_BITS_IDD,
                                  (uint16_t *)&rxPkts,
                                  tformatCRCtable,
                                  PM_TFORMAT_RX_CRC_BYTES_IDD);
    return(crcCheck);
}
#endif

#if defined(PM_TFORMAT_RX_CRC_BY_C28X)
uint16_t
tformat_getCRCID6()
{
    uint16_t crcCheck;
    //
    // The CRC is performed by the C28x CPU
    // Create a rxPacket to calculate the expected CRC of the
    // received data.  The calculated CRC is compared with the
    // CRC received from the encoder
    //
    uint32_t rxPkts;
    rxPkts = ((uint32_t) tformatData.controlField << 16)
                          | ((uint32_t) tformatData.eepromAddressField << 8)
                          | ((uint32_t) tformatData.eepromWrDataField);

    crcCheck = tformat_getCRC(PM_TFORMAT_RX_CRC_BITS_ID6,
                              (uint16_t *)&rxPkts,
                              tformatCRCtable,
                              PM_TFORMAT_RX_CRC_BYTES_ID6);
    return(crcCheck);
}
#endif


#if defined(PM_TFORMAT_RX_CRC_BY_CLB)
uint16_t
tformat_getRxCRCbyCLB(void)
{
    uint16_t crcCheck;
    if(CLB_getInterruptTag(PM_TFORMAT_RX_CRC_BASE) != 5)
    {
        //
        // Something went wrong with the CRC CLB logic
        //
        crcCheck = PM_TFORMAT_CRC_CLB_ERROR;
    }
    else
    {
        CLB_clearInterruptTag(PM_TFORMAT_RX_CRC_BASE);
        crcCheck = CLB_getRegister(PM_TFORMAT_RX_CRC_BASE, CLB_REG_CTR_C2);
        crcCheck &= (PM_TFORMAT_CRC_MASK);
    }
    return(crcCheck);
}
#endif

#if defined(PM_TFORMAT_TX_CRC_BY_C28X) || defined(PM_TFORMAT_RX_CRC_BY_C28X)
void PM_tformat_generateCRCTable(uint16_t nBits, uint16_t polynomial,          \
        uint16_t *pTable)
{
    uint16_t i, j;
    uint16_t accum;

    polynomial <<= (8 - nBits);
    for(i = 0; i < 256 ; i++)
    {
        accum  = i;
        for( j = 0; j < 8; j++)
        {
            if(accum & 0x80)
            {

                //
                // If leading bit is 1, shift accum to left, mask off unwanted
                // MSbs and xor the rest with poly
                //
                accum = ((accum << 1) & 0xFF) ^ polynomial;
            }
            else
            {

                //
                // If leading bit is 0, shift accum to left,
                // mask off unwanted most significant bits
                //
                accum = ((accum << 1) & 0xFF);
            }
        }
        pTable[i] = accum;
    }
    return;
}


uint16_t
tformat_getCRC(uint16_t nBitsData,
                        uint16_t *msg,
                        uint16_t *crcTable,
                        uint16_t rxLen)
{
    uint16_t i;
    uint16_t j;
    uint16_t index;
    uint16_t crcAccum;
    uint16_t crcValue;
    int *pdata;

    index = rxLen - 1;
    crcAccum = 0xFF;
    pdata = (int *)msg;

    //
    // Start from the end
    // Do the first two bytes, special case
    //
    i = crcAccum  ^ (__byte(pdata, index--));
    crcAccum = crcTable[i];
    i = crcAccum ^ (__byte(pdata, index--));
    crcAccum = crcTable[i];

    //
    // Do the CRC for the bytes
    //
    for (j = 0; j < rxLen - 2; j++, index--)
    {
        i = crcAccum  ^ (__byte(pdata, index));
        crcAccum = crcTable[i];
    }
    crcValue = crcAccum ^ (PM_TFORMAT_CRC_MASK);
    return(crcValue);
}

#endif
//
// End of file
//
