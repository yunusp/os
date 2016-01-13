/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    osbasep.h

Abstract:

    This header contains internal definitions for the Operating System Base
    library.

Author:

    Evan Green 25-Feb-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#define OS_API DLLEXPORT_PROTECTED
#define RTL_API DLLEXPORT_PROTECTED

#include <osbase.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// On ARM and x64, there's only one system call mechanism, so it can be a direct
// function.
//

#if defined(__arm__) || defined(__amd64)

#define OsSystemCall OspSystemCallFull

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
VOID
(*POS_SYSTEM_CALL) (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine executes a system call.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    None.

--*/

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the environment.
//

extern PPROCESS_ENVIRONMENT OsEnvironment;

//
// On x86, the system call is a function pointer depending on which processor
// features are supported.
//

#if defined(__i386)

extern POS_SYSTEM_CALL OsSystemCall;

#endif

//
// Store a pointer to the list head of all loaded images.
//

extern LIST_ENTRY OsLoadedImagesHead;

//
// Store the module generation number, which increments whenever a module is
// loaded or unloaded. It is protected under the image list lock.
//

extern UINTN OsImModuleGeneration;

//
// Store the page shift and mask for easy use during image section mappings.
//

extern UINTN OsPageShift;
extern UINTN OsPageSize;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
OspSystemCallFull (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine executes a system call using the traditional method that looks
    a lot like an interrupt. On some architectures, this method is highly
    compatible, but slow. On other architectures, this is the only system call
    mechanism.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    None.

--*/

VOID
OspSetUpSystemCalls (
    VOID
    );

/*++

Routine Description:

    This routine sets up the system call handler.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
OspSignalHandler (
    UINTN SignalNumber,
    UINTN SignalParameter
    );

/*++

Routine Description:

    This routine is called directly by the kernel when a signal occurs. It
    marshals the parameters and calls the C routine for handling the signal.

Arguments:

    SignalNumber - Supplies the index of the signal that occurred.

    SignalParameter - Supplies the optional signal parameter.

Return Value:

    None.

--*/

VOID
OspInitializeMemory (
    VOID
    );

/*++

Routine Description:

    This routine initializes the memory heap portion of the OS base library.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
OspInitializeImageSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes the image library for use in the image creation
    tool.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
OspAcquireImageLock (
    VOID
    );

/*++

Routine Description:

    This routine acquires the global image lock.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
OspReleaseImageLock (
    VOID
    );

/*++

Routine Description:

    This routine releases the global image lock.

Arguments:

    None.

Return Value:

    None.

--*/

PUSER_SHARED_DATA
OspGetUserSharedData (
    VOID
    );

/*++

Routine Description:

    This routine returns a pointer to the user shared data.

Arguments:

    None.

Return Value:

    Returns a pointer to the user shared data area.

--*/

//
// Thread-Local storage functions
//

KSTATUS
OspTlsAllocate (
    PLIST_ENTRY ImageList,
    PVOID *ThreadData
    );

/*++

Routine Description:

    This routine creates the OS library data necessary to manage a new thread.
    This function is usually called by the C library.

Arguments:

    ImageList - Supplies a pointer to the head of the list of loaded images.
        Elements on this list have type LOADED_IMAGE.

    ThreadData - Supplies a pointer where a pointer to the thread data will be
        returned on success. It is the callers responsibility to destroy this
        thread data.

Return Value:

    Status code.

--*/

VOID
OspTlsDestroy (
    PVOID ThreadData
    );

/*++

Routine Description:

    This routine destroys a previously created thread data structure. Callers
    may not use OS library assisted TLS after this routine completes. Signals
    should also probably be masked.

Arguments:

    ThreadData - Supplies a pointer to the thread data to destroy.

Return Value:

    None.

--*/

VOID
OspTlsTearDownModule (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine is called when a module is unloaded. It goes through and
    frees all the TLS images for the module.

Arguments:

    Image - Supplies a pointer to the image being unloaded.

Return Value:

    None.

--*/

