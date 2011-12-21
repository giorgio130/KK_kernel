//------------------------------------------------------------------------------
// <copyright file="common_drv.c" company="Atheros">
//    Copyright (c) 2004-2007 Atheros Corporation.  All rights reserved.
// 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================
#include "a_config.h"
#include "athdefs.h"
#include "a_types.h"
#include "AR6002/hw/mbox_host_reg.h"
#if defined(CONFIG_AR6002_REV1_FORCE_HOST)
#include "AR6002/hw/vmc_reg.h"
#endif
#include "targaddrs.h"
#include "a_osapi.h"
#include "hif.h"
#include "htc_api.h"
#include "wmi.h"
#include "bmi.h"
#include "bmi_msg.h"
#include "common_drv.h"
#include "a_debug.h"

#define HOST_INTEREST_ITEM_ADDRESS(target, item)    \
(((target) == TARGET_TYPE_AR6001) ?     \
   AR6001_HOST_INTEREST_ITEM_ADDRESS(item) :    \
   AR6002_HOST_INTEREST_ITEM_ADDRESS(item))


#define AR6001_LOCAL_COUNT_ADDRESS      0x0c014080
#define AR6002_LOCAL_COUNT_ADDRESS      0x00018080
#define AR6001_RESET_CONTROL_ADDRESS    0x0C000000
#define AR6002_RESET_CONTROL_ADDRESS    0x00004000

#define RESET_CONTROL_COLD_RST_MASK     0x00000100
#define RESET_CONTROL_WARM_RST_MASK     0x00000080
#define RESET_CAUSE_LAST_MASK           0x00000007

/* Compile the 4BYTE version of the window register setup routine,
 * This mitigates host interconnect issues with non-4byte aligned bus requests, some
 * interconnects use bus adapters that impose strict limitations.
 * Since diag window access is not intended for performance critical operations, the 4byte mode should
 * be satisfactory even though it generates 4X the bus activity. */

#ifdef USE_4BYTE_REGISTER_ACCESS

    /* set the window address register (using 4-byte register access ). */
A_STATUS ar6000_SetAddressWindowRegister(HIF_DEVICE *hifDevice, A_UINT32 RegisterAddr, A_UINT32 Address)
{
    A_STATUS status;
    A_UINT8 addrValue[4];
    A_INT32 i;

        /* write bytes 1,2,3 of the register to set the upper address bytes, the LSB is written
         * last to initiate the access cycle */

    for (i = 1; i <= 3; i++) {
            /* fill the buffer with the address byte value we want to hit 4 times*/
        addrValue[0] = ((A_UINT8 *)&Address)[i];
        addrValue[1] = addrValue[0];
        addrValue[2] = addrValue[0];
        addrValue[3] = addrValue[0];

            /* hit each byte of the register address with a 4-byte write operation to the same address,
             * this is a harmless operation */
        status = HIFReadWrite(hifDevice,
                              RegisterAddr+i,
                              addrValue,
                              4,
                              HIF_WR_SYNC_BYTE_FIX,
                              NULL);
        if (status != A_OK) {
            break;
        }
    }

    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot write initial bytes of 0x%x to window reg: 0x%X \n",
             RegisterAddr, Address));
        return status;
    }

        /* write the address register again, this time write the whole 4-byte value.
         * The effect here is that the LSB write causes the cycle to start, the extra
         * 3 byte write to bytes 1,2,3 has no effect since we are writing the same values again */
    status = HIFReadWrite(hifDevice,
                          RegisterAddr,
                          (A_UCHAR *)(&Address),
                          4,
                          HIF_WR_SYNC_BYTE_INC,
                          NULL);

    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot write 0x%x to window reg: 0x%X \n",
            RegisterAddr, Address));
        return status;
    }

    return A_OK;



}


#else

    /* set the window address register */
A_STATUS ar6000_SetAddressWindowRegister(HIF_DEVICE *hifDevice, A_UINT32 RegisterAddr, A_UINT32 Address)
{
    A_STATUS status;

        /* write bytes 1,2,3 of the register to set the upper address bytes, the LSB is written
         * last to initiate the access cycle */
    status = HIFReadWrite(hifDevice,
                          RegisterAddr+1,  /* write upper 3 bytes */
                          ((A_UCHAR *)(&Address))+1,
                          sizeof(A_UINT32)-1,
                          HIF_WR_SYNC_BYTE_INC,
                          NULL);

    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot write initial bytes of 0x%x to window reg: 0x%X \n",
             RegisterAddr, Address));
        return status;
    }

        /* write the LSB of the register, this initiates the operation */
    status = HIFReadWrite(hifDevice,
                          RegisterAddr,
                          (A_UCHAR *)(&Address),
                          sizeof(A_UINT8),
                          HIF_WR_SYNC_BYTE_INC,
                          NULL);

    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot write 0x%x to window reg: 0x%X \n",
            RegisterAddr, Address));
        return status;
    }

    return A_OK;
}

#endif

/*
 * Read from the AR6000 through its diagnostic window.
 * No cooperation from the Target is required for this.
 */
A_STATUS
ar6000_ReadRegDiag(HIF_DEVICE *hifDevice, A_UINT32 *address, A_UINT32 *data)
{
    A_STATUS status;

        /* set window register to start read cycle */
    status = ar6000_SetAddressWindowRegister(hifDevice,
                                             WINDOW_READ_ADDR_ADDRESS,
                                             *address);

    if (status != A_OK) {
        return status;
    }

        /* read the data */
    status = HIFReadWrite(hifDevice,
                          WINDOW_DATA_ADDRESS,
                          (A_UCHAR *)data,
                          sizeof(A_UINT32),
                          HIF_RD_SYNC_BYTE_INC,
                          NULL);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot read from WINDOW_DATA_ADDRESS\n"));
        return status;
    }

    return status;
}


/*
 * Write to the AR6000 through its diagnostic window.
 * No cooperation from the Target is required for this.
 */
A_STATUS
ar6000_WriteRegDiag(HIF_DEVICE *hifDevice, A_UINT32 *address, A_UINT32 *data)
{
    A_STATUS status;

        /* set write data */
    status = HIFReadWrite(hifDevice,
                          WINDOW_DATA_ADDRESS,
                          (A_UCHAR *)data,
                          sizeof(A_UINT32),
                          HIF_WR_SYNC_BYTE_INC,
                          NULL);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot write 0x%x to WINDOW_DATA_ADDRESS\n", *data));
        return status;
    }

        /* set window register, which starts the write cycle */
    return ar6000_SetAddressWindowRegister(hifDevice,
                                           WINDOW_WRITE_ADDR_ADDRESS,
                                           *address);
    }

A_STATUS
ar6000_ReadDataDiag(HIF_DEVICE *hifDevice, A_UINT32 address,
                    A_UCHAR *data, A_UINT32 length)
{
    A_UINT32 count;
    A_STATUS status = A_OK;

    for (count = 0; count < length; count += 4, address += 4) {
        if ((status = ar6000_ReadRegDiag(hifDevice, &address,
                                         (A_UINT32 *)&data[count])) != A_OK)
        {
            break;
        }
    }

    return status;
}

A_STATUS
ar6000_WriteDataDiag(HIF_DEVICE *hifDevice, A_UINT32 address,
                    A_UCHAR *data, A_UINT32 length)
{
    A_UINT32 count;
    A_STATUS status = A_OK;

    for (count = 0; count < length; count += 4, address += 4) {
        if ((status = ar6000_WriteRegDiag(hifDevice, &address,
                                         (A_UINT32 *)&data[count])) != A_OK)
        {
            break;
        }
    }

    return status;
}

static A_STATUS
_do_write_diag(HIF_DEVICE *hifDevice, A_UINT32 addr, A_UINT32 value)
{
    A_STATUS status;

    status = ar6000_WriteRegDiag(hifDevice, &addr, &value);
    if (status != A_OK)
    {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot force Target to execute ROM!\n"));
    }

    return status;
}


/*
 * Delay up to wait_msecs millisecs to allow Target to enter BMI phase,
 * which is a good sign that it's alive and well.  This is used after
 * explicitly forcing the Target to reset.
 *
 * The wait_msecs time should be sufficiently long to cover any reasonable
 * boot-time delay.  For instance, AR6001 firmware allow one second for a
 * low frequency crystal to settle before it calibrates the refclk frequency.
 *
 * TBD: Might want to add special handling for AR6K_OPTION_BMI_DISABLE.
 */
static A_STATUS
_delay_until_target_alive(HIF_DEVICE *hifDevice, A_INT32 wait_msecs, A_UINT32 TargetType)
{
    A_INT32 actual_wait;
    A_INT32 i;
    A_UINT32 address;

    actual_wait = 0;

    /* Hardcode the address of LOCAL_COUNT_ADDRESS based on the target type */
    if (TargetType == TARGET_TYPE_AR6001) {
        address = AR6001_LOCAL_COUNT_ADDRESS;
    } else {
       address = AR6002_LOCAL_COUNT_ADDRESS;
    }
    address += 0x10;
    for (i=0; actual_wait < wait_msecs; i++) {
        A_UINT32 data;

        A_MDELAY(100);
        actual_wait += 100;

        data = 0;
        if (ar6000_ReadRegDiag(hifDevice, &address, &data) != A_OK) {
            return A_ERROR;
        }

        if (data != 0) {
            /* No need to wait longer -- we have a BMI credit */
            return A_OK;
        }
    }
    return A_ERROR; /* timed out */
}

A_STATUS
ar6000_reset_device_skipflash(HIF_DEVICE *hifDevice)
{
    struct forceROM_s {
        A_UINT32 addr;
        A_UINT32 data;
    };
    struct forceROM_s *ForceROM;
    A_INT32 szForceROM;
    A_UINT32 instruction;

    static struct forceROM_s ForceROM_REV2[] = {
        /* NB: This works for AR6001 REV2 ROM (old). */
        {0x00001ff0, 0x175b0027}, /* jump instruction at 0xa0001ff0 */
        {0x00001ff4, 0x00000000}, /* nop instruction at 0xa0001ff4 */

#define MC_REMAP_TARGET_ADDRESS                  0x0c004200
#define MC_REMAP_COMPARE_ADDRESS                 0x0c004180
#define MC_REMAP_SIZE_ADDRESS                    0x0c004100
#define MC_REMAP_VALID_ADDRESS                   0x0c004080
#define LOCAL_SCRATCH_ADDRESS                    0x0c0140c0

        {MC_REMAP_TARGET_ADDRESS, 0x00001ff0}, /* remap to 0xa0001ff0 */
        {MC_REMAP_COMPARE_ADDRESS, 0x01000040},/* ...from 0xbfc00040 */
        {MC_REMAP_SIZE_ADDRESS, 0x00000000},   /* ...1 cache line */
        {MC_REMAP_VALID_ADDRESS, 0x00000001},  /* ...remap is valid */

        /* Force is_host_present to return TRUE */
        {0x00001fe0,  0x8c620000},
        {0x00001fe4,  0x34420002},
        {0x00001fe8,  0xac620000},
        {0x00001fec,  0x00000000},
        {MC_REMAP_TARGET_ADDRESS+4, 0x00001fe0}, /* remap to 0xa0001fe0 */
        {MC_REMAP_COMPARE_ADDRESS+4, 0x01003de0},/* ...from 0x81003de0 */
        {MC_REMAP_SIZE_ADDRESS+4, 0x00000000},   /* ...1 cache line */
        {MC_REMAP_VALID_ADDRESS+4, 0x00000001},  /* ...remap is valid */
    };

    static struct forceROM_s ForceROM_NEW[] = {
        /* NB: This works for AR6001 ROM REV3 and beyond.  */
        {LOCAL_SCRATCH_ADDRESS, AR6K_OPTION_IGNORE_FLASH},
    };

    /*
     * Examine a semi-arbitrary instruction that's different
     * in REV2 and other revisions.
     * NB: If a Host port does not require simultaneous support
     * for multiple revisions of Target ROM, this code can be elided.
     */
    (void)ar6000_ReadDataDiag(hifDevice, 0x01000040,
                              (A_UCHAR *)&instruction, 4);

    AR_DEBUG_PRINTF(ATH_LOG_ERR, ("instruction=0x%x\n", instruction));

    if (instruction == 0x3c1aa200) {
        /* It's an old ROM */
        ForceROM = ForceROM_REV2;
        szForceROM = sizeof(ForceROM_REV2)/sizeof(*ForceROM);
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Using OLD method\n"));
    } else {
        ForceROM = ForceROM_NEW;
        szForceROM = sizeof(ForceROM_NEW)/sizeof(*ForceROM);
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Using NEW method\n"));
    }

    AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Force Target to execute from ROM....\n"));
    {
        A_INT32 i;

        for (i = 0; i < szForceROM; i++)
        {
            if (_do_write_diag(hifDevice, ForceROM[i].addr, ForceROM[i].data) != A_OK) {
                return A_ERROR;
            }
        }
    }


    {
        A_INT32 attempt_number;
        A_INT32 i;

        /*
         * We may need multiple consecutive warm resets.  Current belief is
         * that if the first warm reset occurs while there are outstanding
         * reads (e.g. to flash) then instruction/data loads in startup
         * code -- which executes just after the reset -- may return
         * bogus results.  The second warm reset occurs quickly enough
         * so that we're likely to still be in ROM code (i.e. not
         * accessing flash) when it occurs, and everything works well.
         *
         * The details depend on board and flash part, so this reset algorithm
         * tries a single warm reset first.  If that doesn't work, we try
         * multiple warm resets.
         *
         * TBDXXX: Hardware Eng to verify that a warm reset with pending
         * reads to flash may be problematic.
         */
        for (attempt_number = 1; attempt_number <= 3; attempt_number++)
        {
            /* Clear BMI credit counter */
            if (_do_write_diag(hifDevice,
                               AR6001_LOCAL_COUNT_ADDRESS+0x10,
                               0) != A_OK)
            {
                return A_ERROR;
            }

#if defined(AR6001)
            /* Clear any memctlr errors, since they're sticky */
            if (_do_write_diag(hifDevice,
                               ERROR_VALID_ADDRESS,
                               ERROR_VALID_ERROR_CAPTURE_ENABLE_MASK) != A_OK)
            {
                return A_ERROR;
            }
#endif

            /* Issue 1 or more consecutive WARM resets */
            for (i=0; i<attempt_number; i++) {
                if (_do_write_diag(hifDevice,
                                   AR6001_RESET_CONTROL_ADDRESS,
                                   RESET_CONTROL_WARM_RST_MASK) != A_OK)
                {
                    return A_ERROR;
                }
            }

            if (_delay_until_target_alive(hifDevice, 2000, TARGET_TYPE_AR6001) == A_OK) {
                AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Target executing from ROM\n"));
            } else {
                AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot force Target to execute ROM!\n"));
            }
        }
    }

    /*
     * Something has gone wrong -- we're unable to reset the Target.
     */
    AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Target failure: Will not execute from ROM\n"));
    return A_ERROR;
}

/* reset device */
A_STATUS ar6000_reset_device(HIF_DEVICE *hifDevice, A_UINT32 TargetType, A_BOOL waitForCompletion)
{

#if !defined(DWSIM)
    A_STATUS status = A_OK;
    A_UINT32 address;
    A_UINT32 data;

    do {

        /* 
         * Give some time for the mbox and sdio to settle. Doing a 
         * reset in the middle of throughput test, cause the reset 
         * to fail 
         */
        A_MDELAY(100);

        data = RESET_CONTROL_COLD_RST_MASK;

          /* Hardcode the address of RESET_CONTROL_ADDRESS based on the target type */
        if (TargetType == TARGET_TYPE_AR6001) {
            address = AR6001_RESET_CONTROL_ADDRESS;
        } else {
            if (TargetType == TARGET_TYPE_AR6002) {
                address = AR6002_RESET_CONTROL_ADDRESS;
            } else {
                A_ASSERT(0);
            }
        }


        status = ar6000_WriteRegDiag(hifDevice, &address, &data);

        if (A_FAILED(status)) {
            break;
        }

        if (!waitForCompletion) {
            break;
        }


        /* Up to 2 second delay to allow things to settle down */
        (void)_delay_until_target_alive(hifDevice, 2000, TargetType);

        /*
         * Read back the RESET CAUSE register to ensure that the cold reset
         * went through.
         */

        // address = RESET_CAUSE_ADDRESS;
        /* Hardcode the address of RESET_CAUSE_ADDRESS based on the target type */
        if (TargetType == TARGET_TYPE_AR6001) {
            address = 0x0C0000CC;
        } else {
            if (TargetType == TARGET_TYPE_AR6002) {
                address = 0x000040C0;
            } else {
                A_ASSERT(0);
            }
        }

        data = 0;
        status = ar6000_ReadRegDiag(hifDevice, &address, &data);

        if (A_FAILED(status)) {
            break;
        }

        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Reset Cause readback: 0x%X \n",data));
        data &= RESET_CAUSE_LAST_MASK;
        if (data != 2) {
            AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Unable to cold reset the target \n"));
        }

    } while (FALSE);

    if (A_FAILED(status)) {
        AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Failed to reset target \n"));
    }
#endif
    return A_OK;
}

#if defined(CONFIG_AR6002_REV1_FORCE_HOST)
#define AR6002_VERSION_REV1 0x20000086
/*
 * Call this function just before the call to BMIInit
 * in order to force* AR6002 rev 1.x firmware to detect a Host.
 * THIS IS FOR USE ONLY WITH AR6002 REV 1.x.
 * TBDXXX: Remove this function when REV 1.x is desupported.
 */
A_STATUS
ar6002_REV1_reset_force_host(HIF_DEVICE *hifDevice)
{
    A_INT32 i;
    struct forceROM_s {
        A_UINT32 addr;
        A_UINT32 data;
    };
    struct forceROM_s *ForceROM;
    A_INT32 szForceROM;
    A_STATUS status = A_OK;
    A_UINT32 address;
    A_UINT32 data;

    /* Force AR6002 REV1.x to recognize Host presence.
     *
     * Note: Use RAM at 0x52df80..0x52dfa0 with ROM Remap entry 0
     * so that this workaround functions with AR6002.war1.sh.  We
     * could fold that entire workaround into this one, but it's not
     * worth the effort at this point.  This workaround cannot be
     * merged into the other workaround because this must be done
     * before BMI.
     */

    static struct forceROM_s ForceROM_NEW[] = {
        {0x52df80, 0x20f31c07},
        {0x52df84, 0x92374420},
        {0x52df88, 0x1d120c03},
        {0x52df8c, 0xff8216f0},
        {0x52df90, 0xf01d120c},
        {0x52df94, 0x81004136},
        {0x52df98, 0xbc9100bd},
        {0x52df9c, 0x00bba100},

        {0x00008000|MC_TCAM_TARGET_ADDRESS, 0x0012dfe0}, /* Use remap entry 0 */
        {0x00008000|MC_TCAM_COMPARE_ADDRESS, 0x000e2380},
        {0x00008000|MC_TCAM_MASK_ADDRESS, 0x00000000},
        {0x00008000|MC_TCAM_VALID_ADDRESS, 0x00000001},

        {0x00018000|(AR6002_LOCAL_COUNT_ADDRESS+0x10), 0}, /* clear BMI credit counter */

        {0x00004000|AR6002_RESET_CONTROL_ADDRESS, RESET_CONTROL_WARM_RST_MASK},
    };

    address = 0x004ed4b0; /* REV1 target software ID is stored here */
    status = ar6000_ReadRegDiag(hifDevice, &address, &data);
    if (A_FAILED(status) || (data != AR6002_VERSION_REV1)) {
        return A_ERROR; /* Not AR6002 REV1 */
    }

    ForceROM = ForceROM_NEW;
    szForceROM = sizeof(ForceROM_NEW)/sizeof(*ForceROM);

    AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Force Target to recognize Host....\n"));
    for (i = 0; i < szForceROM; i++)
    {
        if (ar6000_WriteRegDiag(hifDevice,
                                &ForceROM[i].addr,
                                &ForceROM[i].data) != A_OK)
        {
            AR_DEBUG_PRINTF(ATH_LOG_ERR, ("Cannot force Target to recognize Host!\n"));
            return A_ERROR;
        }
    }

    A_MDELAY(1000);

    return A_OK;
}
#endif /* CONFIG_AR6002_REV1_FORCE_HOST */

#define REG_DUMP_COUNT_AR6001   38  /* WORDs, derived from AR6001_regdump.h */
#define REG_DUMP_COUNT_AR6002   32  /* WORDs, derived from AR6002_regdump.h */


#if REG_DUMP_COUNT_AR6001 <= REG_DUMP_COUNT_AR6002
#define REGISTER_DUMP_LEN_MAX  REG_DUMP_COUNT_AR6002
#else
#define REGISTER_DUMP_LEN_MAX  REG_DUMP_COUNT_AR6001
#endif

void ar6000_dump_target_assert_info(HIF_DEVICE *hifDevice, A_UINT32 TargetType)
{
    A_UINT32 address;
    A_UINT32 regDumpArea = 0;
    A_STATUS status;
    A_UINT32 regDumpValues[REGISTER_DUMP_LEN_MAX];
    A_UINT32 regDumpCount = 0;
    A_UINT32 i;

    do {

            /* the reg dump pointer is copied to the host interest area */
        address = HOST_INTEREST_ITEM_ADDRESS(TargetType, hi_failure_state);

        if (TargetType == TARGET_TYPE_AR6001) {
                /* for AR6001, this is a fixed location because the ptr is actually stuck in cache,
                 * this may be fixed in later firmware versions */
            address = 0x18a0;
            regDumpCount = REG_DUMP_COUNT_AR6001;

        } else  if (TargetType == TARGET_TYPE_AR6002) {

            regDumpCount = REG_DUMP_COUNT_AR6002;

        } else {
            A_ASSERT(0);
        }

            /* read RAM location through diagnostic window */
        status = ar6000_ReadRegDiag(hifDevice, &address, &regDumpArea);

        if (A_FAILED(status)) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("AR6K: Failed to get ptr to register dump area \n"));
            break;
        }
#ifndef ATHR_DISPLAY_MSG
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("AR6K: Location of register dump data: 0x%X \n",regDumpArea));
#else
        ATHR_DISPLAY_MSG (TRUE, (L"AR6K: Location of register dump data: 0x%X \n",regDumpArea));
#endif
        if (regDumpArea == 0) {
                /* no reg dump */
            break;
        }

        if (TargetType == TARGET_TYPE_AR6001) {
            regDumpArea &= 0x0FFFFFFF;  /* convert to physical address in target memory */
        }

            /* fetch register dump data */
        status = ar6000_ReadDataDiag(hifDevice,
                                     regDumpArea,
                                     (A_UCHAR *)&regDumpValues[0],
                                     regDumpCount * (sizeof(A_UINT32)));

        if (A_FAILED(status)) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("AR6K: Failed to get register dump \n"));
            break;
        }
#ifndef ATHR_DISPLAY_MSG
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("AR6K: Register Dump: \n"));
#else
        ATHR_DISPLAY_MSG (TRUE, (L"AR6K: Register Dump: \n"));
#endif

        for (i = 0; i < regDumpCount; i++) {
#ifndef ATHR_DISPLAY_MSG
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR,(" %d :  0x%8.8X \n",i, regDumpValues[i]));
#else
            ATHR_DISPLAY_MSG (TRUE, (L" %d :  0x%8.8X \n",i, regDumpValues[i]));
#endif
#ifdef UNDER_CE
            logPrintf(ATH_DEBUG_ERR," %d:  0x%8.8X \n",i, regDumpValues[i]);
#endif
        }

    } while (FALSE);

}

/* set HTC/Mbox operational parameters, this can only be called when the target is in the
 * BMI phase */
A_STATUS ar6000_set_htc_params(HIF_DEVICE *hifDevice,
                               A_UINT32    TargetType,
                               A_UINT32    MboxIsrYieldValue,
                               A_UINT8     HtcControlBuffers)
{
    A_STATUS status;
    A_UINT32 blocksizes[HTC_MAILBOX_NUM_MAX];

    do {
            /* get the block sizes */
        status = HIFConfigureDevice(hifDevice, HIF_DEVICE_GET_MBOX_BLOCK_SIZE,
                                    blocksizes, sizeof(blocksizes));

        if (A_FAILED(status)) {
            AR_DEBUG_PRINTF(ATH_LOG_ERR,("Failed to get block size info from HIF layer...\n"));
            break;
        }
            /* note: we actually get the block size for mailbox 1, for SDIO the block
             * size on mailbox 0 is artificially set to 1 */
            /* must be a power of 2 */
        A_ASSERT((blocksizes[1] & (blocksizes[1] - 1)) == 0);

        if (HtcControlBuffers != 0) {
                /* set override for number of control buffers to use */
            blocksizes[1] |=  ((A_UINT32)HtcControlBuffers) << 16;
        }

            /* set the host interest area for the block size */
        status = BMIWriteMemory(hifDevice,
                                HOST_INTEREST_ITEM_ADDRESS(TargetType, hi_mbox_io_block_sz),
                                (A_UCHAR *)&blocksizes[1],
                                4);

        if (A_FAILED(status)) {
            AR_DEBUG_PRINTF(ATH_LOG_ERR,("BMIWriteMemory for IO block size failed \n"));
            break;
        }

        AR_DEBUG_PRINTF(ATH_LOG_INF,("Block Size Set: %d (target address:0x%X)\n",
                blocksizes[1], HOST_INTEREST_ITEM_ADDRESS(TargetType, hi_mbox_io_block_sz)));

        if (MboxIsrYieldValue != 0) {
                /* set the host interest area for the mbox ISR yield limit */
            status = BMIWriteMemory(hifDevice,
                                    HOST_INTEREST_ITEM_ADDRESS(TargetType, hi_mbox_isr_yield_limit),
                                    (A_UCHAR *)&MboxIsrYieldValue,
                                    4);

            if (A_FAILED(status)) {
                AR_DEBUG_PRINTF(ATH_LOG_ERR,("BMIWriteMemory for yield limit failed \n"));
                break;
            }
        }

    } while (FALSE);

    return status;
}


static A_STATUS prepare_ar6002(HIF_DEVICE *hifDevice, A_UINT32 TargetVersion)
{
    A_STATUS status = A_OK;

    do {

        if (TargetVersion == AR6002_VERSION_REV1) {
            /* the following applies to REV1 silicon, we need to disable
             * sleep as soon as possible as the on-board crystal can be wildly off causing our
             * sleep activation timer to fire off early */
            A_UINT32 value;

                /* read host interest area for system sleep setting */
            status = BMIReadMemory(hifDevice,
                                   HOST_INTEREST_ITEM_ADDRESS(TARGET_TYPE_AR6002, hi_system_sleep_setting),
                                   (A_UCHAR *)&value,
                                   4);

            if (A_FAILED(status)) {
                break;
            }

                /* force the setting to disable sleep, this prevents our sleep activation timer
                 * from having any effect, the host must reset the sleep setting back to zero after it
                 * has completed all initialization */
            value |= 0x1;

            status = BMIWriteMemory(hifDevice,
                                    HOST_INTEREST_ITEM_ADDRESS(TARGET_TYPE_AR6002, hi_system_sleep_setting),
                                    (A_UCHAR *)&value,
                                    4);

            if (A_FAILED(status)) {
                break;
            }

            /* also disable sleep immediately by programming the sleep register directly */

            status = BMIReadSOCRegister(hifDevice,0x40c4,&value);

            if (A_FAILED(status)) {
                break;
            }

            value |= 0x1;

            status = BMIWriteSOCRegister(hifDevice,0x40c4,value);

            if (A_FAILED(status)) {
                break;
            }

        }

   } while (FALSE);

   return status;
}

/* this function assumes the caller has already initialized the BMI APIs */
A_STATUS ar6000_prepare_target(HIF_DEVICE *hifDevice,
                               A_UINT32    TargetType,
                               A_UINT32    TargetVersion)
{
    if (TargetType == TARGET_TYPE_AR6002) {
            /* do any preparations for AR6002 devices */
        return prepare_ar6002(hifDevice,TargetVersion);
    }

    return A_OK;
}

