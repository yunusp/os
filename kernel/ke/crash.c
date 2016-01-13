/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    crash.c

Abstract:

    This module implements support for the unfortunate event of a fatal system
    error.

Author:

    Evan Green 25-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/kdebug.h>
#include "kep.h"

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
// ------------------------------------------------------------------ Functions
//

KERNEL_API
VOID
KeCrashSystemEx (
    ULONG CrashCode,
    PSTR CrashCodeString,
    ULONGLONG Parameter1,
    ULONGLONG Parameter2,
    ULONGLONG Parameter3,
    ULONGLONG Parameter4
    )

/*++

Routine Description:

    This routine officially takes the system down after a fatal system error
    has occurred. This function does not return.

Arguments:

    CrashCode - Supplies the reason for the system crash.

    CrashCodeString - Supplies the string corresponding to the given crash
        code. This parameter is generated by the macro, and should not be
        filled in directly.

    Parameter1 - Supplies an optional parameter regarding the crash.

    Parameter2 - Supplies an optional parameter regarding the crash.

    Parameter3 - Supplies an optional parameter regarding the crash.

    Parameter4 - Supplies an optional parameter regarding the crash.

Return Value:

    None. This function does not return.

--*/

{

    KSTATUS Status;

    KeRaiseRunLevel(RunLevelHigh);

    //
    // TODO: Freeze all processors before writing crash dump.
    //

    ArDisableInterrupts();
    RtlDebugPrint("\n\n"
                  "**********************************************************"
                  "**********************\n"
                  "*                                                         "
                  "                     *\n"
                  "*                            Fatal System Error           "
                  "                     *\n"
                  "*                                                         "
                  "                     *\n"
                  "**********************************************************"
                  "**********************\n\n"
                  "Error Code: %s (0x%x)\n"
                  "Parameter1: 0x%08I64x\n"
                  "Parameter2: 0x%08I64x\n"
                  "Parameter3: 0x%08I64x\n"
                  "Parameter4: 0x%08I64x\n\n",
                  CrashCodeString,
                  CrashCode,
                  Parameter1,
                  Parameter2,
                  Parameter3,
                  Parameter4);

    KdBreak();

    //
    // Proceed to attempt writing a crash dump to disk. If this succeeds then
    // reset the system.
    //

    Status = KepWriteCrashDump(CrashCode,
                               Parameter1,
                               Parameter2,
                               Parameter3,
                               Parameter4);

    //
    // TODO: Remove the forced failure when crash dumps are finished.
    //

    Status = STATUS_UNSUCCESSFUL;
    if (KSUCCESS(Status)) {
        KdDisconnect();
        Status = HlResetSystem(SystemResetWarm);
        KdConnect();
        RtlDebugPrint("System reset unsuccessful: 0x%08x\n", Status);
    }

    //
    // Spin forever.
    //

    while (TRUE) {
        KdBreak();
    }
}

//
// --------------------------------------------------------- Internal Functions
//

