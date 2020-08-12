/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    oswin32.h

Abstract:

    This header fills in definitions that Windows doesn't have.

Author:

    Evan Green 1-Feb-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stddef.h>
#include <sys/time.h>
#include <time.h>

//
// --------------------------------------------------------------------- Macros
//

#define mkdir(_Path, _Permissions) mkdir(_Path)
#define lstat stat
#define chroot(_Path) (errno = ENOSYS, (_Path), -1)
#define link(_Existing, _Target) (errno = ENOSYS, (_Existing), (_Target), -1)
#define symlink(_Target, _Symlink) (errno = ENOSYS, (_Target), (_Symlink), -1)
#define readlink(_Symlink, _Buffer, _Size) \
    (errno = ENOSYS, (_Symlink), -1)

#define chown(_Path, _Uid, _Gid) (errno = ENOSYS, (_Path), (_Uid), (_Gid), -1)
#define lchown chown
#define lutimes utimes
#define fork() (errno = ENOSYS, -1)
#define waitpid(_ProcessId, _Status, _Options) (errno = ENOSYS, -1)

#define WIFEXITED(_Status) 1
#define WIFSTOPPED(_Status) 0
#define WIFCONTINUED(_Status) 0
#define WIFSIGNALED(_Status) 0
#define WEXITSTATUS(_Status) (_Status)
#define WTERMSIG(_Status) (_Status)
#define WSTOPSIG(_Status) (_Status)

//
// Re-define unlink to a function that tries unlink a few times, since Windows
// often fails with EPERM when weird other things are using a file.
//

#define unlink CkpWin32Unlink
#define rmdir CkpWin32Rmdir

//
// The user ID functions all point to the same thing.
//

#define getuid geteuid
#define getgid geteuid
#define getegid geteuid
#define seteuid(_NewId) (errno = ENOSYS, -1)
#define setegid(_NewId) (errno = ENOSYS, -1)
#define setresuid(_RealId, _EffectiveId, _SavedId) (errno = ENOSYS, -1)
#define setresgid(_RealId, _EffectiveId, _SavedId) (errno = ENOSYS, -1)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum length of each of the strings in the utsname structure.
//

#define UTSNAME_STRING_SIZE 80

//
// Windows is not *nix-like.
//

#define CK_IS_UNIX 0

#define S_IFLNK 0
#define S_IFSOCK 0

#define _SC_NPROCESSORS_ONLN 1

#define WCONTINUED 0
#define WNOHANG 0
#define WUNTRACED 0
#define WEXITED 0
#define WNOWAIT 0

//
// ------------------------------------------------------ Data Type Definitions
//

typedef int uid_t, gid_t;

/*++

Structure Description:

    This structure defines the buffer used to name the machine.

Members:

    sysname - Stores a string describing the name of this implementation of
        the operating system.

    nodename - Stores a string describing the name of this node within the
        communications network to which this node is attached, if any.

    release - Stores a string containing the release level of this
        implementation.

    version - Stores a string containing the version level of this release.

    machine - Stores the name of the hardware type on which the system is
        running.

    domainname - Stores the name of the network domain this machine resides in,
        if any.

--*/

struct utsname {
    char sysname[UTSNAME_STRING_SIZE];
    char nodename[UTSNAME_STRING_SIZE];
    char release[UTSNAME_STRING_SIZE];
    char version[UTSNAME_STRING_SIZE];
    char machine[UTSNAME_STRING_SIZE];
    char domainname[UTSNAME_STRING_SIZE];
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

int
uname (
    struct utsname *Name
    );

/*++

Routine Description:

    This routine returns the system name and version.

Arguments:

    Name - Supplies a pointer to a name structure to fill out.

Return Value:

    Returns a non-negative value on success.

    -1 on error, and errno will be set to indicate the error.

--*/

int
getdomainname (
    char *Name,
    size_t NameLength
    );

/*++

Routine Description:

    This routine returns the network domain name for the current machine.

Arguments:

    Name - Supplies a pointer where the null-terminated name will be returned
        on success.

    NameLength - Supplies the length of the name buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

int
utimes (
    const char *Path,
    const struct timeval Times[2]
    );

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges.

Arguments:

    Path - Supplies a pointer to the path of the file to change times for.

    Times - Supplies an optional array of time value structures containing the
        access and modification times to set.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

long
sysconf (
    int Variable
    );

/*++

Routine Description:

    This routine gets the system value for the given variable index. These
    variables are not expected to change within a single invocation of a
    process, and therefore need only be queried once per process.

Arguments:

    Variable - Supplies the variable to get. See _SC_* definitions.

Return Value:

    Returns the value for that variable.

    -1 if the variable has no limit. The errno variable will be left unchanged.

    -1 if the variable was invalid, and errno will be set to EINVAL.

--*/

int
CkpWin32Unlink (
    const char *Path
    );

/*++

Routine Description:

    This routine attempts to unlink a path. This is the Windows version, so it
    will try a few times and only fail if it really cannot get access after
    some time.

Arguments:

    Path - Supplies a pointer to the path of the file to unlink.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

int
CkpWin32Rmdir (
    const char *Path
    );

/*++

Routine Description:

    This routine attempts to remove a directory. This is the Windows version,
    so it will try a few times and only fail if it really cannot get access
    after some time.

Arguments:

    Path - Supplies a pointer to the path of the file to unlink.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

int
geteuid (
    void
    );

/*++

Routine Description:

    This routine returns the effective user ID in Windows. If the process is
    privileged, this routine returns 0. Otherwise, this routine returns 1000.

Arguments:

    None.

Return Value:

    0 if the process is privileged.

    1000 if the process is a regular user.

--*/

