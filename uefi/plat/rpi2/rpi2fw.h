/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    rpi2fw.h

Abstract:

    This header contains internal definitions for the UEFI Raspberry Pi 2
    firmware.

Author:

    Chris Stevens 19-Mar-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <dev/bcm2709.h>
#include <cpu/bcm2836.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Based on the Raspberry Pi 2's APB clock frequency of 250MHz, a predivider
// value of 0xF9 can be used to achieve the target clock frequency of 1MHz.
//

#define RASPBERRY_PI_2_BCM2836_APB_CLOCK_FREQUENCY 250000000
#define RASPBERRY_PI_2_BCM2836_TIMER_PREDIVIDER_VALUE 0xF9

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipEnumerateRamDisks (
    VOID
    );

/*++

Routine Description:

    This routine enumerates any RAM disks embedded in the firmware.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipPlatformSetInterruptLineState (
    UINT32 LineNumber,
    BOOLEAN Enabled,
    BOOLEAN EdgeTriggered
    );

/*++

Routine Description:

    This routine enables or disables an interrupt line.

Arguments:

    LineNumber - Supplies the line number to enable or disable.

    Enabled - Supplies a boolean indicating if the line should be enabled or
        disabled.

    EdgeTriggered - Supplies a boolean indicating if the interrupt is edge
        triggered (TRUE) or level triggered (FALSE).

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipRpi2CreateSmbiosTables (
    VOID
    );

/*++

Routine Description:

    This routine creates the SMBIOS tables.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

//
// Runtime functions.
//

EFIAPI
VOID
EfipBcm2836ResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    );

/*++

Routine Description:

    This routine resets the entire platform.

Arguments:

    ResetType - Supplies the type of reset to perform.

    ResetStatus - Supplies the status code for this reset.

    DataSize - Supplies the size of the reset data.

    ResetData - Supplies an optional pointer for reset types of cold, warm, or
        shutdown to a null-terminated string, optionally followed by additional
        binary data.

Return Value:

    None. This routine does not return.

--*/

