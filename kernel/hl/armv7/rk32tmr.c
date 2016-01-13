/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    rk32tmr.c

Abstract:

    This module implements support for the RK32xx APB timers.

Author:

    Evan Green 9-Jul-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "rk32xx.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an RK32 timer. _Base should be a pointer, and
// _Register should be a RK32_TIMER_REGISTER value.
//

#define READ_TIMER_REGISTER(_Base, _Register) \
    HlRk32KernelServices->ReadRegister32((_Base) + (_Register))

//
// This macro writes to an RK32 timer. _Base should be a pointer,
// _Register should be RK32_TIMER_REGISTER value, and _Value should be a
// 32-bit integer.
//

#define WRITE_TIMER_REGISTER(_Base, _Register, _Value)                     \
    HlRk32KernelServices->WriteRegister32((_Base) + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the internal state associated with an RK32xx APB
    timer.

Members:

    Base - Stores the virtual address of the timer.

    CountDown - Stores a boolean indicating whether this timer counts up
        (FALSE) or down (TRUE).

    Index - Stores the zero-based index of this timer number.

    PhysicalAddress - Stores the physical address of the timer base.

--*/

typedef struct _RK32_TIMER_DATA {
    PVOID Base;
    BOOL CountDown;
    ULONG Index;
    PHYSICAL_ADDRESS PhysicalAddress;
} RK32_TIMER_DATA, *PRK32_TIMER_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpRk32TimerInitialize (
    PVOID Context
    );

ULONGLONG
HlpRk32TimerRead (
    PVOID Context
    );

KSTATUS
HlpRk32TimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpRk32TimerDisarm (
    PVOID Context
    );

VOID
HlpRk32TimerAcknowledgeInterrupt (
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

PRK32XX_TABLE HlRk32Table = NULL;
PHARDWARE_MODULE_KERNEL_SERVICES HlRk32KernelServices = NULL;

//
// Store a pointer to the first timer mapping, so the VAs can be reused.
//

PVOID HlRk32TimerBase;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpRk32TimerModuleEntry (
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    )

/*++

Routine Description:

    This routine is the entry point for the RK32xx APB Timer hardware module.
    Its role is to detect and report the prescense of RK32xx Timers.

Arguments:

    Services - Supplies a pointer to the services/APIs made available by the
        kernel to the hardware module.

Return Value:

    None.

--*/

{

    KSTATUS Status;
    TIMER_DESCRIPTION Timer;
    PRK32_TIMER_DATA TimerData;
    ULONG TimerIndex;

    HlRk32KernelServices = Services;
    HlRk32Table = HlRk32KernelServices->GetAcpiTable(RK32XX_SIGNATURE, NULL);
    if (HlRk32Table == NULL) {
        goto Rk32TimerModuleEntryEnd;
    }

    //
    // Register each of the independent timers in the timer block.
    //

    for (TimerIndex = 0; TimerIndex < RK32_TIMER_COUNT; TimerIndex += 1) {

        //
        // Skip the timer if it has no address or is not enabled.
        //

        if ((HlRk32Table->TimerBase[TimerIndex] == INVALID_PHYSICAL_ADDRESS) ||
            (((1 << TimerIndex) & HlRk32Table->TimerEnabledMask) == 0)) {

            continue;
        }

        HlRk32KernelServices->ZeroMemory(&Timer, sizeof(TIMER_DESCRIPTION));
        Timer.TableVersion = TIMER_DESCRIPTION_VERSION;
        Timer.FunctionTable.Initialize = HlpRk32TimerInitialize;
        Timer.FunctionTable.ReadCounter = HlpRk32TimerRead;
        Timer.FunctionTable.Arm = HlpRk32TimerArm;
        Timer.FunctionTable.Disarm = HlpRk32TimerDisarm;
        Timer.FunctionTable.AcknowledgeInterrupt =
                                             HlpRk32TimerAcknowledgeInterrupt;

        TimerData = HlRk32KernelServices->AllocateMemory(
                                                       sizeof(RK32_TIMER_DATA),
                                                       RK32_ALLOCATION_TAG,
                                                       FALSE,
                                                       NULL);

        if (TimerData == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Rk32TimerModuleEntryEnd;
        }

        HlRk32KernelServices->ZeroMemory(TimerData, sizeof(RK32_TIMER_DATA));
        TimerData->PhysicalAddress = HlRk32Table->TimerBase[TimerIndex];
        TimerData->Index = TimerIndex;
        if (((1 << TimerIndex) & HlRk32Table->TimerCountDownMask) != 0) {
            TimerData->CountDown = TRUE;
        }

        Timer.Context = TimerData;
        Timer.Features = TIMER_FEATURE_READABLE |
                         TIMER_FEATURE_PERIODIC |
                         TIMER_FEATURE_ONE_SHOT;

        Timer.CounterBitWidth = RK32_TIMER_BIT_WIDTH;
        Timer.CounterFrequency = RK32_TIMER_FREQUENCY;
        Timer.Interrupt.Line.Type = InterruptLineControllerSpecified;
        Timer.Interrupt.Line.Controller = 0;
        Timer.Interrupt.Line.Line = HlRk32Table->TimerGsi[TimerIndex];
        Timer.Interrupt.TriggerMode = InterruptModeLevel;
        Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;
        Timer.Identifier = TimerIndex;

        //
        // Register the timer with the system.
        //

        Status = HlRk32KernelServices->Register(HardwareModuleTimer, &Timer);
        if (!KSUCCESS(Status)) {
            goto Rk32TimerModuleEntryEnd;
        }
    }

Rk32TimerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpRk32TimerInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an RK32xx timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    PRK32_TIMER_DATA Timer;

    Timer = (PRK32_TIMER_DATA)Context;
    if (Timer->Base == NULL) {
        Timer->Base = HlRk32KernelServices->MapPhysicalAddress(
                                                        Timer->PhysicalAddress,
                                                        RK32_TIMER_BLOCK_SIZE,
                                                        TRUE);

        if (Timer->Base == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Rk32TimerInitializeEnd;
        }
    }

    //
    // Program the timer in free running mode with no interrupt.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         Rk32TimerControl,
                         RK32_TIMER_CONTROL_ENABLE);

    //
    // Set the load count register to the maximum period.
    //

    WRITE_TIMER_REGISTER(Timer->Base, Rk32TimerLoadCountHigh, 0xFFFFFFFF);
    WRITE_TIMER_REGISTER(Timer->Base, Rk32TimerLoadCountLow, 0xFFFFFFFF);

    //
    // Clear any previously pending interrupts.
    //

    WRITE_TIMER_REGISTER(Timer->Base, Rk32TimerInterruptStatus, 1);
    Status = STATUS_SUCCESS;

Rk32TimerInitializeEnd:
    return Status;
}

ULONGLONG
HlpRk32TimerRead (
    PVOID Context
    )

/*++

Routine Description:

    This routine returns the hardware counter's raw value.

Arguments:

    Context - Supplies the pointer to the timer's context.

Return Value:

    Returns the timer's current count.

--*/

{

    ULONG High1;
    ULONG High2;
    ULONG Low;
    PRK32_TIMER_DATA Timer;
    ULONGLONG Value;

    Timer = (PRK32_TIMER_DATA)Context;

    //
    // Do a high-low-high read to make sure sure the words didn't tear.
    //

    do {
        High1 = READ_TIMER_REGISTER(Timer->Base, Rk32TimerCurrentValueHigh);
        Low = READ_TIMER_REGISTER(Timer->Base, Rk32TimerCurrentValueLow);
        High2 = READ_TIMER_REGISTER(Timer->Base, Rk32TimerCurrentValueHigh);

    } while (High1 != High2);

    Value = (((ULONGLONG)High1) << 32) | Low;
    if (Timer->CountDown != FALSE) {
        Value = ~Value;
    }

    return Value;
}

KSTATUS
HlpRk32TimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    )

/*++

Routine Description:

    This routine arms the timer to fire an interrupt after the specified number
    of ticks.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    Mode - Supplies the mode to arm the timer in (periodic or one-shot). The
        system will never request a mode not supported by the timer's feature
        bits.

    TickCount - Supplies the interval, in ticks, from now for the timer to fire
        in.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    ULONG Control;
    PRK32_TIMER_DATA Timer;

    Timer = (PRK32_TIMER_DATA)Context;
    if (Timer->CountDown == FALSE) {
        TickCount = 0 - TickCount;
    }

    //
    // The RK32 timer does not interrupt if the count is set to 0.
    //

    if (TickCount == 0) {
        TickCount = 1;
    }

    //
    // Stop the timer before programming it, as demanded by the TRM.
    //

    WRITE_TIMER_REGISTER(Timer->Base, Rk32TimerControl, 0);

    //
    // Program the new tick count.
    //

    WRITE_TIMER_REGISTER(Timer->Base, Rk32TimerLoadCountHigh, TickCount >> 32);
    WRITE_TIMER_REGISTER(Timer->Base, Rk32TimerLoadCountLow, TickCount);
    Control = RK32_TIMER_CONTROL_ENABLE | RK32_TIMER_CONTROL_INTERRUPT_ENABLE;
    if (Mode == TimerModeOneShot) {
        Control |= RK32_TIMER_CONTROL_ONE_SHOT;
    }

    WRITE_TIMER_REGISTER(Timer->Base, Rk32TimerControl, Control);
    return STATUS_SUCCESS;
}

VOID
HlpRk32TimerDisarm (
    PVOID Context
    )

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    PRK32_TIMER_DATA Timer;

    Timer = (PRK32_TIMER_DATA)Context;

    //
    // Just stop the timer completely.
    //

    WRITE_TIMER_REGISTER(Timer->Base, Rk32TimerControl, 0);
    return;
}

VOID
HlpRk32TimerAcknowledgeInterrupt (
    PVOID Context
    )

/*++

Routine Description:

    This routine performs any actions necessary upon reciept of a timer's
    interrupt. This may involve writing to an acknowledge register to re-enable
    the timer to fire again, or other hardware specific actions.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    PRK32_TIMER_DATA Timer;

    Timer = (PRK32_TIMER_DATA)Context;

    //
    // Clear the interrupt by writing a 1 to the status bit.
    //

    WRITE_TIMER_REGISTER(Timer->Base, Rk32TimerInterruptStatus, 1);
    return;
}

