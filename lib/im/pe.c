/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    pe.c

Abstract:

    This module implements functionality for manipulating Portable Executable
    (PE) binaries.

Author:

    Evan Green 13-Oct-2012

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "imp.h"
#include "pe.h"

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

BOOL
ImpPeGetHeaders (
    PVOID File,
    UINTN FileSize,
    PIMAGE_NT_HEADERS *PeHeaders
    )

/*++

Routine Description:

    This routine returns a pointer to the PE image headers given a buffer
    containing the executable image mapped in memory.

Arguments:

    File - Supplies a pointer to the image file mapped into memory.

    FileSize - Supplies the size of the memory mapped file, in bytes.

    PeHeaders - Supplies a pointer where the location of the PE headers will
        be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    PIMAGE_DOS_HEADER DosHeader;

    //
    // Read the DOS header to find out where the PE headers are located.
    //

    if (FileSize < sizeof(IMAGE_DOS_HEADER)) {
        return FALSE;
    }

    DosHeader = (PIMAGE_DOS_HEADER)File;
    if (DosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return FALSE;
    }

    if (DosHeader->e_lfanew + sizeof(PIMAGE_NT_HEADERS) > FileSize) {
        return FALSE;
    }

    *PeHeaders = (PIMAGE_NT_HEADERS)((PUCHAR)File + DosHeader->e_lfanew);

    //
    // Perform a few basic checks on the headers to make sure they're valid.
    //

    if (((*PeHeaders)->FileHeader.Characteristics &
         IMAGE_FILE_EXECUTABLE_IMAGE) == 0) {

        return FALSE;
    }

    if ((*PeHeaders)->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return FALSE;
    }

    if ((*PeHeaders)->FileHeader.NumberOfSections == 0) {
        return FALSE;
    }

    return TRUE;
}

BOOL
ImpPeGetSection (
    PVOID File,
    UINTN FileSize,
    PSTR SectionName,
    PVOID *Section,
    PULONGLONG VirtualAddress,
    PULONG SectionSizeInFile,
    PULONG SectionSizeInMemory
    )

/*++

Routine Description:

    This routine gets a pointer to the given section in a PE image given a
    memory mapped file.

Arguments:

    File - Supplies a pointer to the image file mapped into memory.

    FileSize - Supplies the size of the memory mapped file, in bytes.

    SectionName - Supplies the name of the desired section.

    Section - Supplies a pointer where the pointer to the section will be
        returned.

    VirtualAddress - Supplies a pointer where the virtual address of the section
        will be returned, if applicable.

    SectionSizeInFile - Supplies a pointer where the size of the section as it
        appears in the file will be returned.

    SectionSizeInMemory - Supplies a pointer where the size of the section as it
        appears after being loaded in memory will be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    BOOL Match;
    PIMAGE_NT_HEADERS PeHeaders;
    BOOL Result;
    PVOID ReturnSection;
    ULONG ReturnSectionFileSize;
    ULONG ReturnSectionMemorySize;
    ULONG ReturnSectionVirtualAddress;
    PIMAGE_SECTION_HEADER SectionHeader;
    ULONG SectionIndex;

    ReturnSection = NULL;
    ReturnSectionFileSize = 0;
    ReturnSectionMemorySize = 0;
    ReturnSectionVirtualAddress = (UINTN)NULL;
    if (SectionName == NULL) {
        Result = FALSE;
        goto GetSectionEnd;
    }

    Result = ImpPeGetHeaders(File, FileSize, &PeHeaders);
    if (Result == FALSE) {
        goto GetSectionEnd;
    }

    //
    // Loop through all sections looking for the desired one.
    //

    SectionHeader = (PIMAGE_SECTION_HEADER)(PeHeaders + 1);
    for (SectionIndex = 0;
         SectionIndex < PeHeaders->FileHeader.NumberOfSections;
         SectionIndex += 1) {

        Match = RtlAreStringsEqual((PSTR)SectionHeader->Name,
                                   SectionName,
                                   IMAGE_SIZEOF_SHORT_NAME);

        //
        // If the name matches, return that section.
        //

        if (Match != FALSE) {
            ReturnSection = (PUCHAR)File + SectionHeader->PointerToRawData;
            ReturnSectionFileSize = SectionHeader->SizeOfRawData;
            ReturnSectionMemorySize = SectionHeader->Misc.VirtualSize;
            ReturnSectionVirtualAddress = SectionHeader->VirtualAddress;
            break;
        }

        SectionHeader += 1;
    }

GetSectionEnd:
    if (Section != NULL) {
        *Section = ReturnSection;
    }

    if (VirtualAddress != NULL) {
        *VirtualAddress = ReturnSectionVirtualAddress;
    }

    if (SectionSizeInFile != NULL) {
        *SectionSizeInFile = ReturnSectionFileSize;
    }

    if (SectionSizeInMemory != NULL) {
        *SectionSizeInMemory = ReturnSectionMemorySize;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

