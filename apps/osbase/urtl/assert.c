/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    assert.c

Abstract:

    This module implements assertions for the user mode runtime library.

Author:

    Evan Green 25-Jul-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <osbase.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

RTL_API
VOID
RtlRaiseAssertion (
    PSTR Expression,
    PSTR SourceFile,
    ULONG SourceLine
    )

/*++

Routine Description:

    This routine raises an assertion failure exception. If a debugger is
    connected, it will attempt to connect to the debugger.

Arguments:

    Expression - Supplies the string containing the expression that failed.

    SourceFile - Supplies the string describing the source file of the failure.

    SourceLine - Supplies the source line number of the failure.

Return Value:

    None.

--*/

{

    RtlDebugPrint("\n\n *** Assertion Failure: %s\n *** File: %s, Line %d\n\n",
                  Expression,
                  SourceFile,
                  SourceLine);

    OsSendSignal(SignalTargetCurrentProcess,
                 0,
                 SIGNAL_ABORT,
                 SIGNAL_CODE_USER,
                 0);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//
