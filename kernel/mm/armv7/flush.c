/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    flush.c

Abstract:

    This module implements cache flushing routines for the memory manager.

Author:

    Evan Green 20-Aug-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "../mmp.h"

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
// Remember the size of the data cache line.
//

ULONG MmDataCacheLineSize;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
VOID
MmFlushBufferForDataIn (
    PVOID Buffer,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine flushes a buffer in preparation for incoming I/O from a device.

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    None.

--*/

{

    PHYSICAL_ADDRESS PhysicalAddress;

    //
    // Invalidate the data in any second level cache followed by the first
    // level cache.
    //

    PhysicalAddress = MmpVirtualToPhysical(Buffer, NULL);
    ArSerializeExecution();
    ArInvalidateCacheRegion(Buffer, SizeInBytes);
    HlFlushCacheRegion(PhysicalAddress, SizeInBytes, HL_CACHE_FLAG_INVALIDATE);
    ArInvalidateCacheRegion(Buffer, SizeInBytes);
    return;
}

KERNEL_API
VOID
MmFlushBufferForDataOut (
    PVOID Buffer,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine flushes a buffer in preparation for outgoing I/O to a device.

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    None.

--*/

{

    PHYSICAL_ADDRESS PhysicalAddress;

    //
    // Clean the data in the first level cache followed by any second level
    // cache. Since the device is not modifying this data, there's no need to
    // invalidate.
    //

    PhysicalAddress = MmpVirtualToPhysical(Buffer, NULL);
    ArSerializeExecution();
    ArCleanCacheRegion(Buffer, SizeInBytes);
    HlFlushCacheRegion(PhysicalAddress, SizeInBytes, HL_CACHE_FLAG_CLEAN);
    return;
}

KERNEL_API
VOID
MmFlushBufferForDataIo (
    PVOID Buffer,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine flushes a buffer in preparation for data that is both
    incoming and outgoing (ie the buffer is read from and written to by an
    external device).

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    None.

--*/

{

    ULONG Flags;
    PHYSICAL_ADDRESS PhysicalAddress;

    //
    // Data is both going out to the device and coming in from the device, so
    // clean and then invalidate the cache region. Start with a first level
    // clean, then a clean and invalidate at any second level cache, and
    // complete with a clean and invalidate of the first level cache.
    //

    Flags = HL_CACHE_FLAG_CLEAN | HL_CACHE_FLAG_INVALIDATE;
    PhysicalAddress = MmpVirtualToPhysical(Buffer, NULL);
    ArSerializeExecution();
    ArCleanCacheRegion(Buffer, SizeInBytes);
    HlFlushCacheRegion(PhysicalAddress, SizeInBytes, Flags);
    ArInvalidateCacheRegion(Buffer, SizeInBytes);
    return;
}

KERNEL_API
VOID
MmFlushBuffer (
    PVOID Buffer,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine flushes a buffer to the point of unification.

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    None.

--*/

{

    //
    // Just clean the first level data cache to bring the data to the point of
    // unification.
    //

    ArSerializeExecution();
    ArCleanCacheRegion(Buffer, SizeInBytes);
    return;
}

VOID
MmSysFlushCache (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter,
    PTRAP_FRAME TrapFrame,
    PULONG ResultSize
    )

/*++

Routine Description:

    This routine responds to system calls from user mode requesting to
    invalidate the instruction cache after changing a code region.

Arguments:

    SystemCallNumber - Supplies the system call number that was requested.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

    TrapFrame - Supplies a pointer to the trap frame generated by this jump
        from user mode to kernel mode.

    ResultSize - Supplies a pointer where the system call routine returns the
        size of the parameter structure to be copied back to user mode. The
        value returned here must be no larger than the original parameter
        structure size. The default is the original size of the parameters.

Return Value:

    None.

--*/

{

    PVOID Address;
    PSYSTEM_CALL_FLUSH_CACHE Parameters;
    UINTN Size;

    ASSERT(SystemCallNumber == SystemCallFlushCache);

    Parameters = SystemCallParameter;
    Address = Parameters->Address;
    Size = Parameters->Size;
    if (Address >= KERNEL_VA_START) {
        Address = KERNEL_VA_START - 1;
    }

    if ((Address + Size > KERNEL_VA_START) || (Address + Size < Address)) {
        Size = KERNEL_VA_START - Address;
    }

    MmFlushInstructionCache(Address, Size);
    return;
}

VOID
MmFlushInstructionCache (
    PVOID Address,
    UINTN Size
    )

/*++

Routine Description:

    This routine flushes the given cache region and invalidates the
    instruction cache.

Arguments:

    Address - Supplies the address to flush.

    Size - Supplies the number of bytes in the region to flush.

Return Value:

    None.

--*/

{

    ULONG Attributes;
    PVOID CurrentAddress;
    ULONG DataLineSize;
    UINTN PageSize;
    UINTN ThisRegionSize;

    //
    // Clean the data cache, then clean the instruction cache. Ensure each
    // page is mapped before touching it.
    //

    DataLineSize = MmDataCacheLineSize;
    if (DataLineSize == 0) {
        DataLineSize = ArGetDataCacheLineSize();
        MmDataCacheLineSize = DataLineSize;
    }

    CurrentAddress = ALIGN_POINTER_DOWN(Address, DataLineSize);
    Size += REMAINDER((UINTN)Address, DataLineSize);
    Size = ALIGN_RANGE_UP(Size, DataLineSize);
    PageSize = MmPageSize();
    ArSerializeExecution();
    while (Size != 0) {
        ThisRegionSize = PageSize;
        ThisRegionSize -= REMAINDER((UINTN)CurrentAddress, PageSize);
        if (ThisRegionSize > Size) {
            ThisRegionSize = Size;
        }

        if ((MmpVirtualToPhysical(CurrentAddress, &Attributes) !=
             INVALID_PHYSICAL_ADDRESS) &&
            ((Attributes & MAP_FLAG_PRESENT) != 0)) {

            ArCleanCacheRegion(CurrentAddress, ThisRegionSize);
            ArInvalidateInstructionCacheRegion(CurrentAddress, ThisRegionSize);
        }

        CurrentAddress += ThisRegionSize;
        Size -= ThisRegionSize;
    }

    ArSerializeExecution();
    return;
}

VOID
MmFlushDataCache (
    PVOID Address,
    UINTN Size,
    BOOL ValidateAddress
    )

/*++

Routine Description:

    This routine flushes the given date cache region.

Arguments:

    Address - Supplies the address to flush.

    Size - Supplies the number of bytes in the region to flush.

    ValidateAddress - Supplies a boolean indicating whether or not to make sure
        the given address is mapped before flushing.

Return Value:

    None.

--*/

{

    ULONG Attributes;
    PVOID CurrentAddress;
    ULONG DataLineSize;
    UINTN PageSize;
    UINTN ThisRegionSize;

    //
    // Clean the data cache, then clean the instruction cache. Ensure each
    // page is mapped before touching it.
    //

    DataLineSize = MmDataCacheLineSize;
    if (DataLineSize == 0) {
        DataLineSize = ArGetDataCacheLineSize();
        MmDataCacheLineSize = DataLineSize;
    }

    CurrentAddress = ALIGN_POINTER_DOWN(Address, DataLineSize);
    Size += REMAINDER((UINTN)Address, DataLineSize);
    Size = ALIGN_RANGE_UP(Size, DataLineSize);
    PageSize = MmPageSize();
    ArSerializeExecution();
    while (Size != 0) {
        ThisRegionSize = PageSize;
        ThisRegionSize -= REMAINDER((UINTN)CurrentAddress, PageSize);
        if (ThisRegionSize > Size) {
            ThisRegionSize = Size;
        }

        if ((ValidateAddress == FALSE) ||
            ((MmpVirtualToPhysical(CurrentAddress, &Attributes) !=
             INVALID_PHYSICAL_ADDRESS) &&
             ((Attributes & MAP_FLAG_PRESENT) != 0))) {

            ArCleanCacheRegion(CurrentAddress, ThisRegionSize);
        }

        CurrentAddress += ThisRegionSize;
        Size -= ThisRegionSize;
    }

    ArSerializeExecution();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

