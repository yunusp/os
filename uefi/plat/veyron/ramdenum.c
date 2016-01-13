/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    ramdenum.c

Abstract:

    This module implements support for creating a Block I/O protocol from a
    RAM Disk device.

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// The RAM disk is embedded in the firmware image.
//

extern CHAR8 _binary_ramdisk_start;
extern CHAR8 _binary_ramdisk_end;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipEnumerateRamDisks (
    VOID
    )

/*++

Routine Description:

    This routine enumerates any RAM disks embedded in the firmware.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    EFI_PHYSICAL_ADDRESS Base;
    UINT64 Length;
    EFI_STATUS Status;

    Base = (EFI_PHYSICAL_ADDRESS)(UINTN)(&_binary_ramdisk_start);
    Length = (UINTN)(&_binary_ramdisk_end) - (UINTN)(&_binary_ramdisk_start);
    if (Length <= 0x100) {
        return EFI_SUCCESS;
    }

    Status = EfiCoreEnumerateRamDisk(Base, Length);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

