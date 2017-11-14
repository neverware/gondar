// Copyright 2017 Neverware
//
// The source code in this file was derived from:
//
//     Rufus: The Reliable USB Formatting Utility
//     Copyright © 2011-2016 Pete Batard <pete@akeo.ie>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Set minimum SDK version to Win8, needed for
// USB_GET_NODE_CONNECTION_INFORMATION_EX_V2
#define _WIN32_WINNT _WIN32_WINNT_WIN8

#include <windows.h>

#include <setupapi.h>
#include <usbioctl.h>
#include <versionhelpers.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "gondar.h"

#include "msapi_utf8.h"

#include "hdd_vs_ufd.h"

// gondar-level includes
#include "device.h"
#include "gpt_pal.h"
#include "log.h"
#include "shared.h"

static ssize_t size_t_to_signed(const size_t value) {
  if (value <= SSIZE_MAX) {
    return (ssize_t)value;
  } else {
    fprintf(stderr, "warning: overflow in size_t_to_signed\n");
    return (ssize_t)SSIZE_MAX;
  }
}

static int dword_to_int(const DWORD value) {
  if (value <= INT_MAX) {
    return (int)value;
  } else {
    fprintf(stderr, "warning: overflow in dword_to_int\n");
    return (int)INT_MAX;
  }
}

#define ARRAYSIZE_SIGNED(array_) size_t_to_signed(ARRAYSIZE(array_))

/* Convenient to have around */
#define MB 1048576LL
#define GB 1073741824LL

#define USB_GET_NODE_CONNECTION_INFORMATION_EX 274
#define USB_GET_NODE_CONNECTION_INFORMATION_EX_V2 279

#ifndef FILE_DEVICE_UNKNOWN
#define FILE_DEVICE_UNKNOWN 0x00000022
#endif
#ifndef FILE_DEVICE_USB
#define FILE_DEVICE_USB FILE_DEVICE_UNKNOWN
#endif

#define STR_NO_LABEL "NO_LABEL"

#define DRIVE_ACCESS_RETRIES 150  // How many times we should retry
#define DRIVE_ACCESS_TIMEOUT \
  15000  // How long we should retry drive access (in ms)

#define safe_closehandle(h)                           \
  do {                                                \
    if ((h != INVALID_HANDLE_VALUE) && (h != NULL)) { \
      CloseHandle(h);                                 \
      h = INVALID_HANDLE_VALUE;                       \
    }                                                 \
  } while (0)

#define safe_free(p) \
  do {               \
    free((void*)p);  \
    p = NULL;        \
  } while (0)
#define safe_mm_free(p) \
  do {                  \
    _mm_free((void*)p); \
    p = NULL;           \
  } while (0)

#define DD_BUFFER_SIZE \
  65536  // Minimum size of the buffer we use for DD operations

#define WRITE_RETRIES 3

#define FAC(f) (f << 16)

#define safe_strnicmp(str1, str2, count)        \
  _strnicmp(((str1 == NULL) ? "<NULL>" : str1), \
            ((str2 == NULL) ? "<NULL>" : str2), count)

#define DRIVE_INDEX_MIN 0x00000080
#define DRIVE_INDEX_MAX 0x000000C0
#define MAX_DRIVES (DRIVE_INDEX_MAX - DRIVE_INDEX_MIN)

enum {
  USB_SPEED_UNKNOWN = 0,
  USB_SPEED_LOW = 1,
  USB_SPEED_FULL = 2,
  USB_SPEED_HIGH = 3,
  USB_SPEED_SUPER_OR_LATER = 4,
  USB_SPEED_MAX = 5,
};

#define HTAB_EMPTY \
  { NULL, 0, 0 }
#define DEVID_HTAB_SIZE 257

const GUID _GUID_DEVINTERFACE_DISK = {
    0x53f56307L,
    0xb6bf,
    0x11d0,
    {0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b}};
const GUID _GUID_DEVINTERFACE_CDROM = {
    0x53f56308L,
    0xb6bf,
    0x11d0,
    {0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b}};
const GUID _GUID_DEVINTERFACE_USB_HUB = {
    0xf18a0e88L,
    0xc30c,
    0x11d0,
    {0x88, 0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8}};

bool enable_vmdk = false;

/*
 * Windows DDK API definitions. Most of it copied from MinGW's includes
 */
typedef DWORD DEVNODE, DEVINST;
typedef DEVNODE *PDEVNODE, *PDEVINST;
typedef DWORD RETURN_TYPE;
typedef RETURN_TYPE CONFIGRET;
typedef CHAR* DEVINSTID_A;

#define CR_SUCCESS 0x00000000
#define CM_GETIDLIST_FILTER_SERVICE 0x00000002
#define CM_REMOVAL_POLICY_EXPECT_ORDERLY_REMOVAL 0x00000002
#define CM_REMOVAL_POLICY_EXPECT_SURPRISE_REMOVAL 0x00000003
// /!\ The following flag is only available on Windows 7 or later!
#define CM_GETIDLIST_FILTER_PRESENT 0x00000100
#define CM_DRP_ADDRESS 0x0000001D

/*
 * Working with drive indexes quite risky (left unchecked,inadvertently passing
 * 0 as
 * index would return a handle to C:, which we might then proceed to unknowingly
 * clear the MBR of!), so we mitigate the risk by forcing our indexes to belong
 * to
 * the specific range [DRIVE_INDEX_MIN; DRIVE_INDEX_MAX].
 */
#define CheckDriveIndex(DriveIndex)                                         \
  do {                                                                      \
    if ((DriveIndex < DRIVE_INDEX_MIN) || (DriveIndex > DRIVE_INDEX_MAX)) { \
      printf("ERROR: Bad index value %lu. Please check the code!",          \
             DriveIndex);                                                   \
      goto out;                                                             \
    }                                                                       \
    DriveIndex -= DRIVE_INDEX_MIN;                                          \
  } while (0)

#define MAX_LIBRARY_HANDLES 32
HMODULE OpenedLibrariesHandle[MAX_LIBRARY_HANDLES];
uint16_t OpenedLibrariesHandleSize;

static __inline HMODULE GetLibraryHandle(const char* szLibraryName) {
  HMODULE h = NULL;
  if ((h = GetModuleHandleA(szLibraryName)) == NULL) {
    if (OpenedLibrariesHandleSize >= MAX_LIBRARY_HANDLES) {
      printf("Error: MAX_LIBRARY_HANDLES is too small\n");
    } else {
      h = LoadLibraryA(szLibraryName);
      if (h != NULL)
        OpenedLibrariesHandle[OpenedLibrariesHandleSize++] = h;
    }
  }
  return h;
}

#define PF_TYPE(api, ret, proc, args) typedef ret(api* proc##_t) args
#define PF_DECL(proc) static proc##_t pf##proc = NULL
#define PF_TYPE_DECL(api, ret, proc, args) \
  PF_TYPE(api, ret, proc, args);           \
  PF_DECL(proc)
#define PF_INIT(proc, name) \
  if (pf##proc == NULL)     \
  pf##proc = (proc##_t)GetProcAddress(GetLibraryHandle(#name), #proc)

extern "C" {
/* Cfgmgr32.dll interface */
DECLSPEC_IMPORT CONFIGRET WINAPI CM_Get_Device_IDA(DEVINST dnDevInst,
                                                   PCSTR Buffer,
                                                   ULONG BufferLen,
                                                   ULONG ulFlags);
DECLSPEC_IMPORT CONFIGRET WINAPI CM_Get_Device_ID_List_SizeA(PULONG pulLen,
                                                             PCSTR pszFilter,
                                                             ULONG ulFlags);
DECLSPEC_IMPORT CONFIGRET WINAPI CM_Get_Device_ID_ListA(PCSTR pszFilter,
                                                        PCHAR Buffer,
                                                        ULONG BufferLen,
                                                        ULONG ulFlags);
DECLSPEC_IMPORT CONFIGRET WINAPI CM_Locate_DevNodeA(PDEVINST pdnDevInst,
                                                    DEVINSTID_A pDeviceID,
                                                    ULONG ulFlags);
DECLSPEC_IMPORT CONFIGRET WINAPI CM_Get_Child(PDEVINST pdnDevInst,
                                              DEVINST dnDevInst,
                                              ULONG ulFlags);
DECLSPEC_IMPORT CONFIGRET WINAPI CM_Get_Parent(PDEVINST pdnDevInst,
                                               DEVINST dnDevInst,
                                               ULONG ulFlags);
DECLSPEC_IMPORT CONFIGRET WINAPI CM_Get_Sibling(PDEVINST pdnDevInst,
                                                DEVINST dnDevInst,
                                                ULONG ulFlags);
// This last one is unknown from MinGW32 and needs to be fetched from the DLL
PF_TYPE_DECL(WINAPI,
             CONFIGRET,
             CM_Get_DevNode_Registry_PropertyA,
             (DEVINST, ULONG, PULONG, PVOID, PULONG, ULONG));
}

// from registry.c
/* Read a generic registry key value. If a short key_name is used, assume that
   it belongs to
   the application and create the app subkey if required */
static __inline bool _GetRegistryKey(HKEY key_root,
                                     const char* key_name,
                                     DWORD reg_type,
                                     LPBYTE dest,
                                     DWORD dest_size) {
  const char software_prefix[] = "SOFTWARE\\";
  char long_key_name[MAX_PATH] = {0};
  bool r = false;
  size_t i;
  LONG s;
  HKEY hSoftware = NULL, hApp = NULL;
  DWORD dwDisp, dwType = -1, dwSize = dest_size;

  memset(dest, 0, dest_size);

  if (key_name == NULL)
    return false;

  for (i = safe_strlen(key_name); i > 0; i--) {
    if (key_name[i] == '\\')
      break;
  }

  if (i != 0) {
    // Prefix with "SOFTWARE" if needed
    if (_strnicmp(key_name, software_prefix, sizeof(software_prefix) - 1) !=
        0) {
      strcpy(long_key_name, software_prefix);
      safe_strcat(long_key_name, sizeof(long_key_name), key_name);
      long_key_name[sizeof(software_prefix) + i - 1] = 0;
    } else {
      safe_strcpy(long_key_name, sizeof(long_key_name), key_name);
      long_key_name[i] = 0;
    }
    i++;
    if (RegOpenKeyExA(key_root, long_key_name, 0, KEY_READ, &hApp) !=
        ERROR_SUCCESS) {
      hApp = NULL;
      goto out;
    }
  } else {
    if (RegOpenKeyExA(key_root, "SOFTWARE", 0, KEY_READ | KEY_CREATE_SUB_KEY,
                      &hSoftware) != ERROR_SUCCESS) {
      hSoftware = NULL;
      goto out;
    }
    // TODO(kendall): what should the official name of this tool be?
    if (RegCreateKeyExA(hSoftware, "Neverware\\Gondar", 0, NULL, 0,
                        KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_CREATE_SUB_KEY,
                        NULL, &hApp, &dwDisp) != ERROR_SUCCESS) {
      hApp = NULL;
      goto out;
    }
  }

  s = RegQueryValueExA(hApp, &key_name[i], NULL, &dwType, (LPBYTE)dest,
                       &dwSize);
  // No key means default value of 0 or empty string
  if ((s == ERROR_FILE_NOT_FOUND) ||
      ((s == ERROR_SUCCESS) && (dwType == reg_type) && (dwSize > 0))) {
    r = true;
  }
out:
  if (hSoftware != NULL)
    RegCloseKey(hSoftware);
  if (hApp != NULL)
    RegCloseKey(hApp);
  return r;
}

#define GetRegistryKey32(root, key, pval) \
  _GetRegistryKey(root, key, REG_DWORD, (LPBYTE)pval, sizeof(DWORD))

static __inline int32_t ReadRegistryKey32(HKEY root, const char* key) {
  DWORD val;
  GetRegistryKey32(root, key, &val);
  return (int32_t)val;
}
// from stdfn.c
/*
 * Returns true if:
 * 1. The OS supports UAC, UAC is on, and the current process runs elevated, or
 * 2. The OS doesn't support UAC or UAC is off, and the process is being run by
 * a member of the admin group
 */
// TODO(kendall): include whatever this includes
bool IsCurrentProcessElevated() {
  BOOL r = false;
  DWORD size;
  HANDLE token = INVALID_HANDLE_VALUE;
  TOKEN_ELEVATION te;
  SID_IDENTIFIER_AUTHORITY auth = {SECURITY_NT_AUTHORITY};
  PSID psid;

  if (ReadRegistryKey32(HKEY_LOCAL_MACHINE,
                        "Software\\Microsoft\\Windows\\CurrentVersion\\Policies"
                        "\\System\\EnableLUA") == 1) {
    printf("Note: UAC is active");
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
      printf("Could not get current process token:");
      goto out;
    }
    if (!GetTokenInformation(token, TokenElevation, &te, sizeof(te), &size)) {
      printf("Could not get token information:");
      goto out;
    }
    r = (te.TokenIsElevated != 0);
  } else {
    printf("Note: UAC is either disabled or not available");
    if (!AllocateAndInitializeSid(&auth, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &psid))
      goto out;
    if (!CheckTokenMembership(NULL, psid, &r))
      r = false;
    FreeSid(psid);
  }

out:
  safe_closehandle(token);
  return r;
}

// from drive.c
/*
 * Open a drive or volume with optional write and lock access
 * Return INVALID_HANDLE_VALUE (/!\ which is DIFFERENT from NULL /!\) on
 * failure.
 */
static HANDLE GetHandle(char* Path,
                        bool bLockDrive,
                        bool bWriteAccess,
                        bool bWriteShare) {
  int i;
  DWORD size;
  HANDLE hDrive = INVALID_HANDLE_VALUE;
  char DevPath[MAX_PATH];

  if ((safe_strlen(Path) < 5) || (Path[0] != '\\') || (Path[1] != '\\') ||
      (Path[3] != '\\'))
    goto out;

  // Resolve a device path, so that users can seek for it in Process Explorer
  // in case of access issues.
  if (QueryDosDeviceA(&Path[4], DevPath, sizeof(DevPath)) == 0)
    strcpy(DevPath, "???");

  for (i = 0; i < DRIVE_ACCESS_RETRIES; i++) {
    // Try without FILE_SHARE_WRITE (unless specifically requested) so that
    // we won't be bothered by the OS or other apps when we set up our data.
    // However this means we might have to wait for an access gap...
    // We keep FILE_SHARE_READ though, as this shouldn't hurt us any, and is
    // required for enumeration.
    hDrive =
        CreateFileA(Path, GENERIC_READ | (bWriteAccess ? GENERIC_WRITE : 0),
                    FILE_SHARE_READ |
                        ((bWriteAccess && bWriteShare) ? FILE_SHARE_WRITE : 0),
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDrive != INVALID_HANDLE_VALUE)
      break;
    if ((GetLastError() != ERROR_SHARING_VIOLATION) &&
        (GetLastError() != ERROR_ACCESS_DENIED))
      break;
    if (i == 0) {
      printf("Waiting for access...");
    } else if (bWriteAccess && !bWriteShare && (i > DRIVE_ACCESS_RETRIES / 3)) {
      // If we can't seem to get a hold of the drive for some time,
      // try to enable FILE_SHARE_WRITE...
      printf(
          "Warning: Could not obtain exclusive rights. Retrying with write "
          "sharing enabled...");
      bWriteShare = true;
    }
    Sleep(DRIVE_ACCESS_TIMEOUT / DRIVE_ACCESS_RETRIES);
  }
  if (hDrive == INVALID_HANDLE_VALUE) {
    printf("Could not open %s [%s]:\n", Path, DevPath);
    goto out;
  }

  if (bWriteAccess) {
    printf("Opened %s [%s] for write access\n", Path, DevPath);
  }

  if (bLockDrive) {
    if (DeviceIoControl(hDrive, FSCTL_ALLOW_EXTENDED_DASD_IO, NULL, 0, NULL, 0,
                        &size, NULL)) {
      printf("I/O boundary checks disabled\n");
    }

    for (i = 0; i < DRIVE_ACCESS_RETRIES; i++) {
      if (DeviceIoControl(hDrive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &size,
                          NULL))
        goto out;
      Sleep(DRIVE_ACCESS_TIMEOUT / DRIVE_ACCESS_RETRIES);
    }
    // If we reached this section, either we didn't manage to get a lock or the
    // user cancelled
    printf("Could not get exclusive access to %s [%s]:\n", Path, DevPath);
    safe_closehandle(hDrive);
  }

out:
  return hDrive;
}

/*
 * Return the path to access the physical drive, or NULL on error.
 * The string is allocated and must be freed (to ensure concurrent access)
 */
static char* GetPhysicalName(DWORD DriveIndex) {
  bool success = false;
  char physical_name[24];

  CheckDriveIndex(DriveIndex);
  safe_sprintf(physical_name, sizeof(physical_name), "\\\\.\\PHYSICALDRIVE%lu",
               DriveIndex);
  success = true;
out:
  return (success) ? safe_strdup(physical_name) : NULL;
}
// also from drive.c
/*
 * Return a handle to the physical drive identified by DriveIndex
 */
static HANDLE GetPhysicalHandle(DWORD DriveIndex,
                                bool bLockDrive,
                                bool bWriteAccess) {
  HANDLE hPhysical = INVALID_HANDLE_VALUE;
  char* PhysicalPath = GetPhysicalName(DriveIndex);
  hPhysical = GetHandle(PhysicalPath, bLockDrive, bWriteAccess, false);
  safe_free(PhysicalPath);
  return hPhysical;
}
// from drive.c
/*
 * Return the drive size
 */
static uint64_t GetDriveSize(DWORD DriveIndex) {
  bool r;
  HANDLE hPhysical;
  DWORD size;
  BYTE geometry[256];
  PDISK_GEOMETRY_EX DiskGeometry = (PDISK_GEOMETRY_EX)(void*)geometry;

  hPhysical = GetPhysicalHandle(DriveIndex, false, false);
  if (hPhysical == INVALID_HANDLE_VALUE)
    return false;

  r = DeviceIoControl(hPhysical, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0,
                      geometry, sizeof(geometry), &size, NULL);
  safe_closehandle(hPhysical);
  if (!r || size <= 0)
    return 0;
  printf("DISK SIZE=%llu\n", DiskGeometry->DiskSize.QuadPart);
  return DiskGeometry->DiskSize.QuadPart;
}

/* We need a redef of these MS structure */
typedef struct {
  DWORD DeviceType;
  ULONG DeviceNumber;
  ULONG PartitionNumber;
} STORAGE_DEVICE_NUMBER_REDEF;

typedef struct {
  DWORD NumberOfDiskExtents;
  // The one from MS uses ANYSIZE_ARRAY, which can lead to all kind of problems
  DISK_EXTENT Extents[8];
} VOLUME_DISK_EXTENTS_REDEF;

/*
 * Who would have thought that Microsoft would make it so unbelievably hard to
 * get the frickin' device number for a drive? You have to use TWO different
 * methods to have a chance to get it!
 */
// FIXME(kendall): is path really just for debugging?
static int GetDriveNumber(HANDLE hDrive) {
  STORAGE_DEVICE_NUMBER_REDEF DeviceNumber;
  VOLUME_DISK_EXTENTS_REDEF DiskExtents;
  DWORD size;
  int r = -1;

  if (!DeviceIoControl(hDrive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0,
                       &DiskExtents, sizeof(DiskExtents), &size, NULL) ||
      (size <= 0) || (DiskExtents.NumberOfDiskExtents < 1)) {
    // DiskExtents are NO_GO (which is the case for external USB HDDs...)
    if (!DeviceIoControl(hDrive, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0,
                         &DeviceNumber, sizeof(DeviceNumber), &size, NULL) ||
        (size <= 0)) {
      printf("Could not get device number for device: ");
      return -1;
    }
    r = (int)DeviceNumber.DeviceNumber;
  } else if (DiskExtents.NumberOfDiskExtents >= 2) {
    printf("Ignoring drive as it spans multiple disks (RAID?)");
    return -1;
  } else {
    r = (int)DiskExtents.Extents[0].DiskNumber;
  }
  if (r >= MAX_DRIVES) {
    printf("Device Number for device is too big (%d) - ignoring device", r);
    return -1;
  }
  return r;
}
// some kewl heuristics from smart.c

/*
 * Returns the drive letters for all volumes located on the drive identified by
 * DriveIndex,
 * as well as the drive type. This is used as base for the 2 function calls that
 * follow.
 */
static bool _GetDriveLettersAndType(DWORD DriveIndex,
                                    char* drive_letters,
                                    UINT* drive_type) {
  DWORD size;
  bool r = false;
  HANDLE hDrive = INVALID_HANDLE_VALUE;
  UINT _drive_type;
  int i = 0, drive_number;
  char *drive, drives[26 * 4 + 1]; /* "D:\", "E:\", etc., plus one NUL */
  char logical_drive[] = "\\\\.\\#:";

  if (drive_letters != NULL)
    drive_letters[0] = 0;
  if (drive_type != NULL)
    *drive_type = DRIVE_UNKNOWN;
  CheckDriveIndex(DriveIndex);

  // This call is weird... The buffer needs to have an extra NUL, but you're
  // supposed to provide the size without the extra NUL. And the returned size
  // does not include the NUL either *EXCEPT* if your buffer is too small...
  // But then again, this doesn't hold true if you have a 105 byte buffer and
  // pass a 4*26=104 size, as the the call will return 105 (i.e. *FAILURE*)
  // instead of 104 as it should => screw Microsoft: We'll include the NUL
  // always, as each drive string is at least 4 chars long anyway.
  size = GetLogicalDriveStringsA(sizeof(drives), drives);
  if (size == 0) {
    printf("GetLogicalDriveStrings failed: \n");
    goto out;
  }
  if (size > sizeof(drives)) {
    printf("GetLogicalDriveStrings: Buffer too small (required %lu vs. %d)\n",
           size, sizeof(drives));
    goto out;
  }

  r = true;  // Required to detect drives that don't have volumes assigned
  for (drive = drives; *drive; drive += safe_strlen(drive) + 1) {
    if (!isalpha(*drive))
      continue;
    *drive = (char)toupper((int)*drive);
    if (*drive < 'C') {
      continue;
    }

    // IOCTL_STORAGE_GET_DEVICE_NUMBER's STORAGE_DEVICE_NUMBER.DeviceNumber is
    // not unique! An HDD, a DVD and probably other drives can have the same
    // value there => Use GetDriveType() to filter out unwanted devices.
    // See https://github.com/pbatard/rufus/issues/32#issuecomment-3785956
    _drive_type = GetDriveTypeA(drive);

    if ((_drive_type != DRIVE_REMOVABLE) && (_drive_type != DRIVE_FIXED))
      continue;

    safe_sprintf(logical_drive, sizeof(logical_drive), "\\\\.\\%c:", drive[0]);
    hDrive = CreateFileA(logical_drive, GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
      continue;
    }

    drive_number = GetDriveNumber(hDrive);
    safe_closehandle(hDrive);
    if (drive_number == dword_to_int(DriveIndex)) {
      r = true;
      if (drive_letters != NULL)
        drive_letters[i++] = *drive;
      // The drive type should be the same for all volumes, so we can overwrite
      if (drive_type != NULL)
        *drive_type = _drive_type;
    }
  }

out:
  if (drive_letters != NULL)
    drive_letters[i] = 0;
  return r;
}
// There's already a GetDriveType in the Windows API
static UINT GetDriveTypeFromIndex(DWORD DriveIndex) {
  UINT drive_type;
  _GetDriveLettersAndType(DriveIndex, NULL, &drive_type);
  return drive_type;
}
/*
 * This attempts to detect whether a drive is an USB HDD or an USB Flash Drive
 * (UFD).
 * A positive score means that we think it's an USB HDD, zero or negative means
 * that
 * we think it's an UFD.
 *
 * This is done so that, if someone already has an USB HDD plugged in (say as a
 * backup drive) and plugs an UFD we *try* to do what we can to avoid them
 * formatting
 * that drive by mistake.
 * However, because there is no foolproof (let alone easy) way to differentiate
 * UFDs
 * from HDDs, thanks to every manufacturer, Microsoft, and their mothers, making
 * it
 * exceedingly troublesome to find what type of hardware we are actually
 * accessing,
 * you are expected to pay heed to the following:
 *
 * WARNING: NO PROMISE IS MADE ABOUT THIS ALGORITHM BEING ABLE TO CORRECTLY
 * DIFFERENTIATE AN USB HDD FROM AN USB FLASH DRIVE. MOREOVER, YOU ARE REMINDED
 * THAT
 * THE LICENSE OF THIS APPLICATION MAKES NO PROMISE ABOUT AVOIDING DATA LOSS
 * EITHER
 * (PROVIDED "AS IS").
 * THUS, IF DATA LOSS IS INCURRED DUE TO THIS, OR ANY OTHER PART OF THIS
 * APPLICATION,
 * NOT BEHAVING IN THE MANNER YOU EXPECTED, THE RESPONSIBILITY IS ENTIRELY ON
 * YOU!
 *
 * What you have below, then, is our *current best guess* at differentiating
 * UFDs
 * from HDDs. But short of a crystal ball, this remains just a guess, which may
 * be
 * way off mark. Still, you are also reminded that Rufus does produce PROMINENT
 * warnings before you format a drive, and also provides extensive info about
 * the
 * drive (from the tooltips and the log) => PAY ATTENTION TO THESE OR PAY THE
 * PRICE!
 *
 * But let me just elaborate further on why differentiating UFDs from HDDs is
 * not as
 * 'simple' as it seems:
 * - many USB flash drives manufacturer will present UFDs as non-removable,
 * which used
 *   to be reserved for HDDs => we can't use that as differentiator.
 * - some UFDs (SanDisk Extreme) have added S.M.A.R.T. support, which also used
 * to be
 *   reserved for HDDs => can't use that either
 * - even if S.M.A.R.T. was enough, not all USB->IDE or USB->SATA bridges
 * support ATA
 *   passthrough, which is required S.M.A.R.T. data, and each manufacturer of an
 *   USB<->(S)ATA bridge seem to have their own method of implementing
 * passthrough.
 * - SSDs have also changed the deal completely, as you can get something that
 * looks
 *   like Flash but that is really an HDD.
 * - Some manufacturers (eg. verbatim) provide both USB Flash Drives and USB
 * HDDs, so
 *   we can't exactly use the VID to say for sure what we're looking at.
 * - Finally, Microsoft is absolutely no help either (which is kind of
 * understandable
 *   from the above) => there is no magic API we can query that will tell us
 * what we're
 *   really looking at.
 */
static int IsHDD(DWORD DriveIndex,
                 uint16_t vid,
                 uint16_t pid,
                 const char* strid) {
  int score = 0;
  size_t i, mlen, ilen;
  bool wc;
  uint64_t drive_size;

  // Boost the score if fixed, as these are *generally* HDDs
  // NB: Due to a Windows API limitation, drives with no mounted partition will
  // never have DRIVE_FIXED
  if (GetDriveTypeFromIndex(DriveIndex) == DRIVE_FIXED)
    score += 3;

  // Adjust the score depending on the size
  drive_size = GetDriveSize(DriveIndex);
  if (drive_size > 512 * GB)
    score += 10;
  else if (drive_size < 8 * GB)
    score -= 10;

  // Check the string against well known HDD identifiers
  if (strid != NULL) {
    ilen = strlen(strid);
    for (i = 0; i < ARRAYSIZE(str_score); i++) {
      mlen = strlen(str_score[i].name);
      if (mlen > ilen)
        break;
      wc = (str_score[i].name[mlen - 1] == '#');
      if ((_strnicmp(strid, str_score[i].name, mlen - ((wc) ? 1 : 0)) == 0) &&
          ((!wc) || ((strid[mlen] >= '0') && (strid[mlen] <= '9')))) {
        score += str_score[i].score;
        break;
      }
    }
  }

  // Adjust for oddball devices
  if (strid != NULL) {
    for (i = 0; i < ARRAYSIZE(str_adjust); i++)
      if (strstr(strid, str_adjust[i].name) != NULL)
        score += str_adjust[i].score;
  }

  // Check against known VIDs
  for (i = 0; i < ARRAYSIZE(vid_score); i++) {
    if (vid == vid_score[i].vid) {
      score += vid_score[i].score;
      break;
    }
  }

  // Check against known VID:PIDs
  for (i = 0; i < ARRAYSIZE(vidpid_score); i++) {
    if ((vid == vidpid_score[i].vid) && (pid == vidpid_score[i].pid)) {
      score += vidpid_score[i].score;
      break;
    }
  }
  return score;
}
// end kewl heuristics
static const wchar_t wspace[] = L" \t";

static __inline bool IsVHD(const char* buffer) {
  int i;
  // List of the Hardware IDs of the VHD devices we know
  const char* vhd_name[] = {
      "Arsenal_________Virtual_", "KernSafeVirtual_________",
      "Msft____Virtual_Disk____",
      "VMware__VMware_Virtual_S"  // Enabled through a cheat mode, as this lists
                                  // primary disks on VMWare instances
  };

  for (i = 0; i < (int)(ARRAYSIZE(vhd_name) - (enable_vmdk ? 0 : 1)); i++)
    if (safe_strstr(buffer, vhd_name[i]) != NULL)
      return true;
  return false;
}

/* List of the properties we are interested in */
typedef struct usb_device_props {
  uint32_t vid;
  uint32_t pid;
  uint32_t speed;
  uint32_t port;
  BOOLEAN is_USB;
  BOOLEAN is_SCSI;
  BOOLEAN is_CARD;
  BOOLEAN is_UASP;
  BOOLEAN is_VHD;
  BOOLEAN is_Removable;
  BOOLEAN is_LowerSpeed;
} usb_device_props;

/*
 * Get the VID, PID and current device speed
 */
static bool GetUSBProperties(char* parent_path,
                             char* device_id,
                             usb_device_props* props) {
  bool r = false;
  CONFIGRET cr;
  HANDLE handle = INVALID_HANDLE_VALUE;
  DWORD size;
  DEVINST device_inst;
  USB_NODE_CONNECTION_INFORMATION_EX conn_info;
  USB_NODE_CONNECTION_INFORMATION_EX_V2 conn_info_v2;
  PF_INIT(CM_Get_DevNode_Registry_PropertyA, Cfgmgr32);

  if ((parent_path == NULL) || (device_id == NULL) || (props == NULL) ||
      (pfCM_Get_DevNode_Registry_PropertyA == NULL)) {
    goto out;
  }

  cr = CM_Locate_DevNodeA(&device_inst, device_id, 0);
  if (cr != CR_SUCCESS) {
    printf("Could not get device instance handle for '%s': CR error %lu",
           device_id, cr);
    goto out;
  }

  props->port = 0;
  size = sizeof(props->port);
  cr = pfCM_Get_DevNode_Registry_PropertyA(device_inst, CM_DRP_ADDRESS, NULL,
                                           (PVOID)&props->port, &size, 0);
  if (cr != CR_SUCCESS) {
    printf("Could not get port for '%s': CR error %lu", device_id, cr);
    goto out;
  }

  handle = CreateFileA(parent_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                       OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    printf("Could not open hub %s:", parent_path);
    goto out;
  }
  memset(&conn_info, 0, sizeof(conn_info));
  size = sizeof(conn_info);
  conn_info.ConnectionIndex = (ULONG)props->port;
  // coverity[tainted_data_argument]
  if (!DeviceIoControl(handle, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                       &conn_info, size, &conn_info, size, &size, NULL)) {
    printf("Could not get node connection information for '%s':", device_id);
    goto out;
  }

  // Some poorly written proprietary Windows 7 USB 3.0 controller drivers
  // (<cough>ASMedia<cough>)
  // have a screwed up implementation of
  // IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX that succeeds
  // but returns zeroed data => Add a workaround so that we don't lose our
  // VID:PID...
  if ((conn_info.DeviceDescriptor.idVendor != 0) ||
      (conn_info.DeviceDescriptor.idProduct != 0)) {
    props->vid = conn_info.DeviceDescriptor.idVendor;
    props->pid = conn_info.DeviceDescriptor.idProduct;
    props->speed = conn_info.Speed + 1;
    r = true;
  }

  // In their great wisdom, Microsoft decided to BREAK the USB speed report
  // between Windows 7 and Windows 8
  if (IsWindows8OrGreater()) {
    memset(&conn_info_v2, 0, sizeof(conn_info_v2));
    size = sizeof(conn_info_v2);
    conn_info_v2.ConnectionIndex = (ULONG)props->port;
    conn_info_v2.Length = size;
    conn_info_v2.SupportedUsbProtocols.Usb300 = 1;
    if (!DeviceIoControl(
            handle, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX_V2,
            &conn_info_v2, size, &conn_info_v2, size, &size, NULL)) {
      printf("Could not get node connection information (V2) for device '%s':",
             device_id);
    } else if (conn_info_v2.Flags.DeviceIsOperatingAtSuperSpeedOrHigher) {
      props->speed = USB_SPEED_SUPER_OR_LATER;
    } else if (conn_info_v2.Flags.DeviceIsSuperSpeedCapableOrHigher) {
      props->is_LowerSpeed = true;
    }
  }

out:
  safe_closehandle(handle);
  return r;
}
/*
 * Parse a line of UTF-16 text and return the data if it matches the 'token'
 * The parsed line is of the form: [ ][<][ ]token[ ][=|>][ ]["]data["][ ][<] and
 * is
 * modified by the parser
 */
static wchar_t* get_token_data_line(const wchar_t* wtoken, wchar_t* wline) {
  size_t i, r;
  BOOLEAN quoteth = false;
  BOOLEAN xml = false;

  if ((wtoken == NULL) || (wline == NULL) || (wline[0] == 0))
    return NULL;

  i = 0;

  // Skip leading spaces and opening '<'
  i += wcsspn(&wline[i], wspace);
  if (wline[i] == L'<')
    i++;
  i += wcsspn(&wline[i], wspace);

  // Our token should begin a line
  if (_wcsnicmp(&wline[i], wtoken, wcslen(wtoken)) != 0)
    return NULL;

  // Token was found, move past token
  i += wcslen(wtoken);

  // Skip spaces
  i += wcsspn(&wline[i], wspace);

  // Check for '=' or '>' sign
  if (wline[i] == L'>')
    xml = true;
  else if (wline[i] != L'=')
    return NULL;
  i++;

  // Skip spaces
  i += wcsspn(&wline[i], wspace);

  // eliminate leading quote, if it exists
  if (wline[i] == L'"') {
    quoteth = true;
    i++;
  }

  // Keep the starting pos of our data
  r = i;

  // locate end of string or quote
  while ((wline[i] != 0) &&
         (((wline[i] != L'"') && (wline[i] != L'<')) ||
          ((wline[i] == L'"') && (!quoteth)) || ((wline[i] == L'<') && (!xml))))
    i++;
  wline[i--] = 0;

  // Eliminate trailing EOL characters
  while ((i >= r) && ((wline[i] == L'\r') || (wline[i] == L'\n')))
    wline[i--] = 0;

  return (wline[r] == 0) ? NULL : &wline[r];
}
/*
 * Parse a file (ANSI or UTF-8 or UTF-16) and return the data for the 'index'th
 * occurrence of 'token'
 * The returned string is UTF-8 and MUST be freed by the caller
 */
static char* get_token_data_file_indexed(const char* token,
                                         const char* filename,
                                         int index) {
  int i = 0;
  wchar_t *wtoken = NULL, *wdata = NULL, *wfilename = NULL;
  wchar_t buf[1024];
  FILE* fd = NULL;
  char* ret = NULL;

  if ((filename == NULL) || (token == NULL))
    return NULL;
  if ((filename[0] == 0) || (token[0] == 0))
    return NULL;

  wfilename = utf8_to_wchar(filename);
  if (wfilename == NULL) {
    printf("conversion error!\n");
    goto out;
  }
  wtoken = utf8_to_wchar(token);
  if (wfilename == NULL) {
    printf("conversion error!\n");
    goto out;
  }
  fd = _wfopen(wfilename, L"r, ccs=UNICODE");
  if (fd == NULL)
    goto out;

  // Process individual lines. NUL is always appended.
  // Ideally, we'd check that our buffer fits the line
  while (fgetws(buf, ARRAYSIZE(buf), fd) != NULL) {
    wdata = get_token_data_line(wtoken, buf);
    if ((wdata != NULL) && (++i == index)) {
      ret = wchar_to_utf8(wdata);
      break;
    }
  }

out:
  if (fd != NULL)
    fclose(fd);
  safe_free(wfilename);
  safe_free(wtoken);
  return ret;
}
#define get_token_data_file(token, filename) \
  get_token_data_file_indexed(token, filename, 1)

// Could have used a #define, but this is clearer
static bool GetDriveLetters(DWORD DriveIndex, char* drive_letters) {
  return _GetDriveLettersAndType(DriveIndex, drive_letters, NULL);
}

/*
 * Return the drive letter and volume label
 * If the drive doesn't have a volume assigned, space is returned for the letter
 */
static bool GetDriveLabel(DWORD DriveIndex, char* letters, const char** label) {
  HANDLE hPhysical;
  DWORD size;
  static char VolumeLabel[MAX_PATH + 1];
  char DrivePath[] = "#:\\", AutorunPath[] = "#:\\autorun.inf",
       *AutorunLabel = NULL;

  *label = STR_NO_LABEL;

  if (!GetDriveLetters(DriveIndex, letters))
    return false;
  if (letters[0] == 0) {
    // Drive without volume assigned - always enabled
    return true;
  }
  // We only care about an autorun.inf if we have a single volume
  AutorunPath[0] = letters[0];
  DrivePath[0] = letters[0];

  // Try to read an extended label from autorun first. Fallback to regular label
  // if not found.
  // In the case of card readers with no card, users can get an annoying popup
  // asking them
  // to insert media. Use IOCTL_STORAGE_CHECK_VERIFY to prevent this
  hPhysical = GetPhysicalHandle(DriveIndex, false, false);
  if (DeviceIoControl(hPhysical, IOCTL_STORAGE_CHECK_VERIFY, NULL, 0, NULL, 0,
                      &size, NULL))
    AutorunLabel = get_token_data_file("label", AutorunPath);
  else if (GetLastError() == ERROR_NOT_READY)
    printf("Ignoring autorun.inf label for drive %c: %s\n", letters[0],
           (HRESULT_CODE(GetLastError()) == ERROR_NOT_READY) ? "No media"
                                                             : "Bad media");
  safe_closehandle(hPhysical);
  if (AutorunLabel != NULL) {
    printf("Using autorun.inf label for drive %c: '%s'\n", letters[0],
           AutorunLabel);
    safe_strcpy(VolumeLabel, sizeof(VolumeLabel), AutorunLabel);
    safe_free(AutorunLabel);
    *label = VolumeLabel;
  } else if (GetVolumeInformationU(DrivePath, VolumeLabel,
                                   ARRAYSIZE(VolumeLabel), NULL, NULL, NULL,
                                   NULL, 0) &&
             (VolumeLabel[0] != 0)) {
    *label = VolumeLabel;
  } else {
    printf("Failed to read label:");
  }

  return true;
}
/*
 * GET_DRIVE_GEOMETRY is used to tell if there is an actual media
 */
static bool IsMediaPresent(DWORD DriveIndex) {
  bool r;
  HANDLE hPhysical;
  DWORD size;
  BYTE geometry[128];

  hPhysical = GetPhysicalHandle(DriveIndex, false, false);
  r = DeviceIoControl(hPhysical, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0,
                      geometry, sizeof(geometry), &size, NULL) &&
      (size > 0);
  safe_closehandle(hPhysical);
  return r;
}

static __inline bool IsRemovable(const char* buffer) {
  switch (*((DWORD*)buffer)) {
    case CM_REMOVAL_POLICY_EXPECT_SURPRISE_REMOVAL:
    case CM_REMOVAL_POLICY_EXPECT_ORDERLY_REMOVAL:
      return true;
    default:
      return false;
  }
}

/* Basic String Array */
typedef struct {
  char** String;
  uint32_t Index;  // Current array size
  uint32_t Max;    // Maximum array size
} StrArray;

/* Hash tables */
typedef struct htab_entry {
  uint32_t used;
  char* str;
  void* data;
} htab_entry;
typedef struct htab_table {
  htab_entry* table;
  uint32_t size;
  uint32_t filled;
} htab_table;

typedef DWORD DEVNODE, DEVINST;

/*
 * String array manipulation
 */
static void StrArrayCreate(StrArray* arr, uint32_t initial_size) {
  if (arr == NULL)
    return;
  arr->Max = initial_size;
  arr->Index = 0;
  arr->String = (char**)calloc(arr->Max, sizeof(char*));
  if (arr->String == NULL)
    printf("Could not allocate string array\n");
}

static int32_t StrArrayAdd(StrArray* arr, const char* str, bool duplicate) {
  char** old_table;
  if ((arr == NULL) || (arr->String == NULL) || (str == NULL))
    return -1;
  if (arr->Index == arr->Max) {
    arr->Max *= 2;
    old_table = arr->String;
    arr->String = (char**)realloc(arr->String, arr->Max * sizeof(char*));
    if (arr->String == NULL) {
      free(old_table);
      printf("Could not reallocate string array\n");
      return -1;
    }
  }
  arr->String[arr->Index] = (duplicate) ? safe_strdup(str) : (char*)str;
  if (arr->String[arr->Index] == NULL) {
    printf("Could not store string in array\n");
    return -1;
  }
  return arr->Index++;
}

static void StrArrayClear(StrArray* arr) {
  size_t i;
  if ((arr == NULL) || (arr->String == NULL))
    return;
  for (i = 0; i < arr->Index; i++) {
    safe_free(arr->String[i]);
  }
  arr->Index = 0;
}

static void StrArrayDestroy(StrArray* arr) {
  StrArrayClear(arr);
  if (arr != NULL)
    safe_free(arr->String);
}

/*
 * For the used double hash method the table size has to be a prime. To
 * correct the user given table size we need a prime test.  This trivial
 * algorithm is adequate because the code is called only during init and
 * the number is likely to be small
 */
static uint32_t isprime(uint32_t number) {
  // no even number will be passed
  uint32_t divider = 3;

  while ((divider * divider < number) && (number % divider != 0))
    divider += 2;

  return (number % divider != 0);
}

/*
 * Before using the hash table we must allocate memory for it.
 * We allocate one element more as the found prime number says.
 * This is done for more effective indexing as explained in the
 * comment for the hash function.
 */
static bool htab_create(uint32_t nel, htab_table* htab) {
  if (htab == NULL) {
    return false;
  }
  if (htab->table != NULL) {
    printf("warning: htab_create() was called with a non empty table");
    return false;
  }

  // Change nel to the first prime number not smaller as nel.
  nel |= 1;
  while (!isprime(nel))
    nel += 2;

  htab->size = nel;
  htab->filled = 0;

  // allocate memory and zero out.
  htab->table = (htab_entry*)calloc(htab->size + 1, sizeof(htab_entry));
  if (htab->table == NULL) {
    printf("could not allocate space for hash table\n");
    return false;
  }

  return true;
}

/* After using the hash table it has to be destroyed.  */
static void htab_destroy(htab_table* htab) {
  size_t i;

  if ((htab == NULL) || (htab->table == NULL)) {
    return;
  }

  for (i = 0; i < htab->size + 1; i++) {
    if (htab->table[i].used) {
      safe_free(htab->table[i].str);
    }
  }
  htab->filled = 0;
  htab->size = 0;
  safe_free(htab->table);
  htab->table = NULL;
}

static __inline void ToUpper(char* str) {
  size_t i;
  for (i = 0; i < safe_strlen(str); i++)
    str[i] = toupper(str[i]);
}

/*
 * This is the search function. It uses double hashing with open addressing.
 * We use a trick to speed up the lookup. The table is created with one
 * more element available. This enables us to use the index zero special.
 * This index will never be used because we store the first hash index in
 * the field used where zero means not used. Every other value means used.
 * The used field can be used as a first fast comparison for equality of
 * the stored and the parameter value. This helps to prevent unnecessary
 * expensive calls of strcmp.
 */
static uint32_t htab_hash(char* str, htab_table* htab) {
  uint32_t hval, hval2;
  uint32_t idx;
  uint32_t r = 0;
  int c;
  char* sz = str;

  if ((htab == NULL) || (htab->table == NULL) || (str == NULL)) {
    return 0;
  }

  // Compute main hash value using sdbm's algorithm (empirically
  // shown to produce half the collisions as djb2's).
  // See http://www.cse.yorku.ca/~oz/hash.html
  while ((c = *sz++) != 0)
    r = c + (r << 6) + (r << 16) - r;
  if (r == 0)
    ++r;

  // compute table hash: simply take the modulus
  hval = r % htab->size;
  if (hval == 0)
    ++hval;

  // Try the first index
  idx = hval;

  if (htab->table[idx].used) {
    if ((htab->table[idx].used == hval) &&
        (safe_strcmp(str, htab->table[idx].str) == 0)) {
      // existing hash
      return idx;
    }
    // uprintf("hash collision ('%s' vs '%s')\n", str, htab->table[idx].str);

    // Second hash function, as suggested in [Knuth]
    hval2 = 1 + hval % (htab->size - 2);

    do {
      // Because size is prime this guarantees to step through all available
      // indexes
      if (idx <= hval2) {
        idx = ((uint32_t)htab->size) + idx - hval2;
      } else {
        idx -= hval2;
      }

      // If we visited all entries leave the loop unsuccessfully
      if (idx == hval) {
        break;
      }

      // If entry is found use it.
      if ((htab->table[idx].used == hval) &&
          (safe_strcmp(str, htab->table[idx].str) == 0)) {
        return idx;
      }
    } while (htab->table[idx].used);
  }

  // Not found => New entry

  // If the table is full return an error
  if (htab->filled >= htab->size) {
    printf("hash table is full (%d entries)", htab->size);
    return 0;
  }

  safe_free(htab->table[idx].str);
  htab->table[idx].used = hval;
  htab->table[idx].str = (char*)malloc(safe_strlen(str) + 1);
  if (htab->table[idx].str == NULL) {
    printf("could not duplicate string for hash table\n");
    return 0;
  }
  memcpy(htab->table[idx].str, str, safe_strlen(str) + 1);
  ++htab->filled;

  return idx;
}

bool list_non_usb_removable_drives = false;
#define MIN_DRIVE_SIZE 8

bool enable_HDDs = false;
// look i don't know
bool right_to_left_mode = false;

char app_dir[512];
char system_dir[512];
// ^ new stuff for detecting devices

DWORD FormatStatus;

// this is a big one.  in its modified form, it just returns to me the device
// number i want
/*
 * Refresh the list of USB devices
 */
static void GetDevices(DeviceGuyList* device_list) {
  DWORD drive_index = -1;  // this value is set for each device as we iterate.
                           // the value initialized here should never be used

  // List of USB storage drivers we know - list may be incomplete!
  const char* usbstor_name[] = {
      // Standard MS USB storage driver
      "USBSTOR",
      // USB card readers, with proprietary drivers (Realtek,etc...)
      // Mostly "guessed" from http://www.carrona.org/dvrref.php
      "RTSUER", "CMIUCR", "EUCR",
      // UASP Drivers *MUST* be listed after this, starting with "UASPSTOR"
      // (which is Microsoft's native UASP driver for Windows 8 and later)
      // as we use "UASPSTOR" as a delimiter
      "UASPSTOR", "VUSBSTOR", "ETRONSTOR", "ASUSSTPT"};
  // These are the generic (non USB) storage enumerators we also test
  const char* genstor_name[] = {
      // Generic storage drivers (Careful now!)
      "SCSI",  // "STORAGE", // "STORAGE" is used by 'Storage Spaces" and stuff
               // => DANGEROUS!
      // Non-USB card reader drivers - This list *MUST* start with "SD"
      // (delimiter)
      // See http://itdoc.hitachi.co.jp/manuals/3021/30213B5200e/DMDS0094.HTM
      // Also  http://www.carrona.org/dvrref.php. NB: These should be reported
      // as enumerators by Rufus when Enum Debug is enabled
      "SD", "PCISTOR", "RTSOR", "JMCR", "JMCF", "RIMMPTSK", "RIMSPTSK",
      "RIXDPTSK", "TI21SONY", "ESD7SK", "ESM7SK", "O2MD", "O2SD", "VIACR"};
  // Oh, and we also have card devices (e.g. 'SCSI\DiskO2Micro_SD_...') under
  // the SCSI enumerator...
  const char* scsi_disk_prefix = "SCSI\\Disk";
  const char* scsi_card_name[] = {"_SD_",    "_MMC_",       "_MS_",
                                  "_MSPro_", "_xDPicture_", "_O2Media_"};
  const char* usb_speed_name[USB_SPEED_MAX] = {"USB", "USB 1.0", "USB 1.1",
                                               "USB 2.0", "USB 3.0"};
  // Hash table and String Array used to match a Device ID with the parent hub's
  // Device Interface Path
  htab_table htab_devid = HTAB_EMPTY;
  StrArray dev_if_path;
  char drive_name[] = "?:\\";
  bool post_backslash;
  HDEVINFO dev_info = NULL;
  SP_DEVINFO_DATA dev_info_data;
  SP_DEVICE_INTERFACE_DATA devint_data;
  PSP_DEVICE_INTERFACE_DETAIL_DATA_A devint_detail_data;
  DEVINST parent_inst, grandparent_inst, device_inst;
  DWORD size, i, j, k, l, datatype;
  DWORD uasp_start = ARRAYSIZE(usbstor_name),
        card_start = ARRAYSIZE(genstor_name);
  ULONG list_size[ARRAYSIZE(usbstor_name)] = {0},
        list_start[ARRAYSIZE(usbstor_name)] = {0}, full_list_size, ulFlags;
  HANDLE hDrive;
  int s, score, drive_number;
  char drive_letters[27], *device_id, *devid_list = NULL;
  char *p, buffer[MAX_PATH], str[MAX_PATH];
  const char* label;
  const char* method_str;
  usb_device_props props;

  StrArrayCreate(&dev_if_path, 128);
  // Add a dummy for string index zero, as this is what non matching hashes will
  // point to
  StrArrayAdd(&dev_if_path, "", true);

  device_id = (char*)malloc(MAX_PATH);
  if (device_id == NULL)
    goto out;

  // Build a hash table associating a CM Device ID of an USB device with the
  // SetupDI Device Interface Path
  // of its parent hub - this is needed to retrieve the device speed

  dev_info = SetupDiGetClassDevsA(&_GUID_DEVINTERFACE_USB_HUB, NULL, NULL,
                                  DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (dev_info != INVALID_HANDLE_VALUE) {
    if (htab_create(DEVID_HTAB_SIZE, &htab_devid)) {
      dev_info_data.cbSize = sizeof(dev_info_data);
      for (i = 0; SetupDiEnumDeviceInfo(dev_info, i, &dev_info_data); i++) {
        printf("Processing Hub %lu:", i + 1);
        devint_detail_data = NULL;
        devint_data.cbSize = sizeof(devint_data);
        // Only care about the first interface (MemberIndex 0)
        if ((SetupDiEnumDeviceInterfaces(dev_info, &dev_info_data,
                                         &_GUID_DEVINTERFACE_USB_HUB, 0,
                                         &devint_data)) &&
            (!SetupDiGetDeviceInterfaceDetailA(dev_info, &devint_data, NULL, 0,
                                               &size, NULL)) &&
            (GetLastError() == ERROR_INSUFFICIENT_BUFFER) &&
            ((devint_detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)calloc(
                  1, size)) != NULL)) {
          devint_detail_data->cbSize =
              sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
          if (SetupDiGetDeviceInterfaceDetailA(dev_info, &devint_data,
                                               devint_detail_data, size, &size,
                                               NULL)) {
            // Find the Device IDs for all the children of this hub
            if (CM_Get_Child(&device_inst, dev_info_data.DevInst, 0) ==
                CR_SUCCESS) {
              device_id[0] = 0;
              s = StrArrayAdd(&dev_if_path, devint_detail_data->DevicePath,
                              true);
              printf("  Hub[%d] = '%s'", s, devint_detail_data->DevicePath);
              if ((s >= 0) && (CM_Get_Device_IDA(device_inst, device_id,
                                                 MAX_PATH, 0) == CR_SUCCESS)) {
                ToUpper(device_id);
                if ((k = htab_hash(device_id, &htab_devid)) != 0) {
                  htab_devid.table[k].data = (void*)(uintptr_t)s;
                }
                printf("  Found ID[%03lu]: %s", k, device_id);
                while (CM_Get_Sibling(&device_inst, device_inst, 0) ==
                       CR_SUCCESS) {
                  device_id[0] = 0;
                  if (CM_Get_Device_IDA(device_inst, device_id, MAX_PATH, 0) ==
                      CR_SUCCESS) {
                    ToUpper(device_id);
                    if ((k = htab_hash(device_id, &htab_devid)) != 0) {
                      htab_devid.table[k].data = (void*)(uintptr_t)s;
                    }
                    printf("  Found ID[%03lu]: %s", k, device_id);
                  }
                }
              }
            }
          }
          free(devint_detail_data);
        }
      }
    }
    SetupDiDestroyDeviceInfoList(dev_info);
  }
  free(device_id);

  // Build a single list of Device IDs from all the storage enumerators we know
  // of
  full_list_size = 0;
  ulFlags = CM_GETIDLIST_FILTER_SERVICE;
  if (IsWindows7OrGreater())
    ulFlags |= CM_GETIDLIST_FILTER_PRESENT;
  for (s = 0; s < ARRAYSIZE_SIGNED(usbstor_name); s++) {
    // Get a list of device IDs for all USB storage devices
    // This will be used to find if a device is UASP
    // Also compute the uasp_start index
    if (strcmp(usbstor_name[s], "UASPSTOR") == 0)
      uasp_start = s;
    if (CM_Get_Device_ID_List_SizeA(&list_size[s], usbstor_name[s], ulFlags) !=
        CR_SUCCESS)
      list_size[s] = 0;
    if (list_size[s] != 0)
      full_list_size += list_size[s] - 1;  // remove extra NUL terminator
  }
  // Compute the card_start index
  for (s = 0; s < ARRAYSIZE_SIGNED(genstor_name); s++) {
    if (strcmp(genstor_name[s], "SD") == 0)
      card_start = s;
  }
  // Overkill, but better safe than sorry. And yeah, we could have used
  // arrays of arrays to avoid this, but it's more readable this way.
  if ((uasp_start <= 0) || (uasp_start >= ARRAYSIZE(usbstor_name))) {
    printf("Spock gone crazy error in %s:%d", __FILE__, __LINE__);
    goto out;
  }
  if ((card_start <= 0) || (card_start >= ARRAYSIZE(genstor_name))) {
    printf("Spock gone crazy error in %s:%d", __FILE__, __LINE__);
    goto out;
  }
  devid_list = NULL;
  if (full_list_size != 0) {
    full_list_size += 1;  // add extra NUL terminator
    devid_list = (char*)malloc(full_list_size);
    if (devid_list == NULL) {
      printf("Could not allocate Device ID list\n");
      goto out;
    }
    for (s = 0, i = 0; s < ARRAYSIZE_SIGNED(usbstor_name); s++) {
      list_start[s] = i;
      if (list_size[s] > 1) {
        if (CM_Get_Device_ID_ListA(usbstor_name[s], &devid_list[i],
                                   list_size[s], ulFlags) != CR_SUCCESS)
          continue;
        // The list_size is sometimes larger than required thus we need to find
        // the real end
        for (i += list_size[s]; i > 2; i--) {
          if ((devid_list[i - 2] != '\0') && (devid_list[i - 1] == '\0') &&
              (devid_list[i] == '\0'))
            break;
        }
      }
    }
  }
  // Now use SetupDi to enumerate all our disk storage devices
  dev_info = SetupDiGetClassDevsA(&_GUID_DEVINTERFACE_DISK, NULL, NULL,
                                  DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (dev_info == INVALID_HANDLE_VALUE) {
    printf("SetupDiGetClassDevs (Interface) failed:\n");
    goto out;
  }
  dev_info_data.cbSize = sizeof(dev_info_data);
  for (i = 0; SetupDiEnumDeviceInfo(dev_info, i, &dev_info_data); i++) {
    memset(buffer, 0, sizeof(buffer));
    memset(&props, 0, sizeof(props));
    method_str = "";
    if (!SetupDiGetDeviceRegistryPropertyA(
            dev_info, &dev_info_data, SPDRP_ENUMERATOR_NAME, &datatype,
            (LPBYTE)buffer, sizeof(buffer), &size)) {
      printf("SetupDiGetDeviceRegistryProperty (Enumerator Name) failed:\n");
      continue;
    }

    for (j = 0; j < ARRAYSIZE(usbstor_name); j++) {
      if (safe_stricmp(buffer, usbstor_name[0]) == 0) {
        props.is_USB = true;
        if ((j != 0) && (j < uasp_start))
          props.is_CARD = true;
        break;
      }
    }

    // UASP drives are listed under SCSI, and we also have non USB card readers
    // to populate
    for (j = 0; j < ARRAYSIZE(genstor_name); j++) {
      if (safe_stricmp(buffer, genstor_name[j]) == 0) {
        props.is_SCSI = true;
        if (j >= card_start)
          props.is_CARD = true;
        break;
      }
    }

    printf("Processing '%s' device:", buffer);
    if ((!props.is_USB) && (!props.is_SCSI)) {
      printf("  Disabled by policy");
      continue;
    }

    // We can't use the friendly name to find if a drive is a VHD, as friendly
    // name string gets translated
    // according to your locale, so we poke the Hardware ID
    memset(buffer, 0, sizeof(buffer));
    props.is_VHD = SetupDiGetDeviceRegistryPropertyA(
                       dev_info, &dev_info_data, SPDRP_HARDWAREID, &datatype,
                       (LPBYTE)buffer, sizeof(buffer), &size) &&
                   IsVHD(buffer);
    // Additional detection for SCSI card readers
    if ((!props.is_CARD) &&
        (safe_strnicmp(buffer, scsi_disk_prefix,
                       sizeof(scsi_disk_prefix) - 1) == 0)) {
      for (j = 0; j < ARRAYSIZE(scsi_card_name); j++) {
        if (safe_strstr(buffer, scsi_card_name[j]) != NULL) {
          props.is_CARD = true;
          break;
        }
      }
    }
    printf("  Hardware ID: '%s'", buffer);

    memset(buffer, 0, sizeof(buffer));
    props.is_Removable =
        SetupDiGetDeviceRegistryPropertyA(
            dev_info, &dev_info_data, SPDRP_REMOVAL_POLICY, &datatype,
            (LPBYTE)buffer, sizeof(buffer), &size) &&
        IsRemovable(buffer);

    memset(buffer, 0, sizeof(buffer));
    if (!SetupDiGetDeviceRegistryPropertyU(
            dev_info, &dev_info_data, SPDRP_FRIENDLYNAME, &datatype,
            (LPBYTE)buffer, sizeof(buffer), &size)) {
      printf("SetupDiGetDeviceRegistryProperty (Friendly Name) failed: \n");
      // We can afford a failure on this call - just replace the name with "USB
      // Storage Device (Generic)"
      safe_strcpy(buffer, sizeof(buffer), "USB Storage Device (Generic)");
    } else if ((!props.is_VHD) && (devid_list != NULL)) {
      // Get the properties of the device. We could avoid doing this lookup
      // every time by keeping
      // a lookup table, but there shouldn't be that many USB storage devices
      // connected...
      // NB: Each of these Device IDs should have a child, from which we get the
      // Device Instance match.
      for (device_id = devid_list; *device_id != 0;
           device_id += strlen(device_id) + 1) {
        if (CM_Locate_DevNodeA(&parent_inst, device_id, 0) != CR_SUCCESS) {
          printf("Could not locate device node for '%s'", device_id);
          continue;
        }
        if (CM_Get_Child(&device_inst, parent_inst, 0) != CR_SUCCESS) {
          printf("Could not get children of '%s'", device_id);
          continue;
        }
        if (device_inst != dev_info_data.DevInst) {
          // Try the siblings
          while (CM_Get_Sibling(&device_inst, device_inst, 0) == CR_SUCCESS) {
            if (device_inst == dev_info_data.DevInst) {
              printf("NOTE: Matched instance from sibling for '%s'", device_id);
              break;
            }
          }
          if (device_inst != dev_info_data.DevInst)
            continue;
        }
        post_backslash = false;
        method_str = "";

        // If we're not dealing with the USBSTOR part of our list, then this is
        // an UASP device
        props.is_UASP = ((((uintptr_t)device_id) + 2) >=
                         ((uintptr_t)devid_list) + list_start[uasp_start]);
        // Now get the properties of the device, and its Device ID, which we
        // need to populate the properties
        ToUpper(device_id);
        j = htab_hash(device_id, &htab_devid);
        printf("  Matched with ID[%03lu]: %s", j, device_id);

        // Try to parse the current device_id string for VID:PID
        // We'll use that if we can't get anything better
        for (k = 0, l = 0; (k < strlen(device_id)) && (l < 2); k++) {
          // The ID is in the form USB_VENDOR_BUSID\VID_xxxx&PID_xxxx\...
          if (device_id[k] == '\\')
            post_backslash = true;
          if (!post_backslash)
            continue;
          if (device_id[k] == '_') {
            props.pid = (uint16_t)strtoul(&device_id[k + 1], NULL, 16);
            if (l++ == 0)
              props.vid = props.pid;
          }
        }
        if (props.vid != 0)
          method_str = "[ID]";

        // If the hash didn't match a populated string in dev_if_path[]
        // (htab_devid.table[j].data > 0),
        // we might have an extra vendor driver in between (e.g. "ASUS USB 3.0
        // Boost Storage Driver"
        // for UASP devices in ASUS "Turbo Mode" or "Apple Mobile Device USB
        // Driver" for iPods)
        // so try to see if we can match the grandparent.
        if (((uintptr_t)htab_devid.table[j].data == 0) &&
            (CM_Get_Parent(&grandparent_inst, parent_inst, 0) == CR_SUCCESS) &&
            (CM_Get_Device_IDA(grandparent_inst, str, MAX_PATH, 0) ==
             CR_SUCCESS)) {
          device_id = str;
          method_str = "[GP]";
          ToUpper(device_id);
          j = htab_hash(device_id, &htab_devid);
          printf("  Matched with (GP) ID[%03lu]: %s", j, device_id);
        }
        if ((uintptr_t)htab_devid.table[j].data > 0) {
          printf("  Matched with Hub[%d]: '%s'",
                 (uintptr_t)htab_devid.table[j].data,
                 dev_if_path.String[(uintptr_t)htab_devid.table[j].data]);
          if (GetUSBProperties(
                  dev_if_path.String[(uintptr_t)htab_devid.table[j].data],
                  device_id, &props))
            method_str = "";
#ifdef FORCED_DEVICE
          props.vid = FORCED_VID;
          props.pid = FORCED_PID;
          safe_strcpy(buffer, sizeof(buffer), FORCED_NAME);
#endif
        }
        break;
      }
    }

    if (props.is_VHD) {
      printf("Found VHD device '%s'", buffer);
    } else if ((props.is_CARD) &&
               ((!props.is_USB) || ((props.vid == 0) && (props.pid == 0)))) {
      printf("Found card reader device '%s'", buffer);
    } else if ((!props.is_USB) && (!props.is_UASP) && (props.is_Removable)) {
      if (!list_non_usb_removable_drives) {
        printf("Found non-USB removable device '%s' => Eliminated", buffer);
        printf(
            "If you *REALLY* need, you can enable listing of this device with "
            "<Ctrl><Alt><F>");
        continue;
      }
      printf("Found non-USB removable device '%s'", buffer);
    } else {
      if ((props.vid == 0) && (props.pid == 0)) {
        if (!props.is_USB) {
          // If we have a non removable SCSI drive and couldn't get a VID:PID,
          // we are most likely dealing with a system drive => eliminate it!
          printf("Found non-USB non-removable device '%s' => Eliminated",
                 buffer);
          continue;
        }
        safe_strcpy(str, sizeof(str), "????:????");  // Couldn't figure VID:PID
      } else {
        static_sprintf(str, "%04X:%04X", props.vid, props.pid);
      }
      if (props.speed >= USB_SPEED_MAX)
        props.speed = 0;
      printf("Found %s%s%s device '%s' (%s) %s\n", props.is_UASP ? "UAS (" : "",
             usb_speed_name[props.speed], props.is_UASP ? ")" : "", buffer, str,
             method_str);
      if (props.is_LowerSpeed)
        printf(
            "NOTE: This device is an USB 3.0 device operating at lower "
            "speed...");
    }
    devint_data.cbSize = sizeof(devint_data);
    hDrive = INVALID_HANDLE_VALUE;
    devint_detail_data = NULL;
    for (j = 0;; j++) {
      safe_closehandle(hDrive);
      safe_free(devint_detail_data);

      if (!SetupDiEnumDeviceInterfaces(dev_info, &dev_info_data,
                                       &_GUID_DEVINTERFACE_DISK, j,
                                       &devint_data)) {
        if (GetLastError() != ERROR_NO_MORE_ITEMS) {
          printf("SetupDiEnumDeviceInterfaces failed: \n");
        } else {
          printf(
              "A device was eliminated because it didn't report itself as a "
              "disk\n");
        }
        break;
      }

      if (!SetupDiGetDeviceInterfaceDetailA(dev_info, &devint_data, NULL, 0,
                                            &size, NULL)) {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
          devint_detail_data =
              (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)calloc(1, size);
          if (devint_detail_data == NULL) {
            printf(
                "Unable to allocate data for "
                "SP_DEVICE_INTERFACE_DETAIL_DATA\n");
            continue;
          }
          devint_detail_data->cbSize =
              sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        } else {
          printf("SetupDiGetDeviceInterfaceDetail (dummy) failed: \n");
          continue;
        }
      }
      if (devint_detail_data == NULL) {
        printf(
            "SetupDiGetDeviceInterfaceDetail (dummy) - no data was "
            "allocated\n");
        continue;
      }
      if (!SetupDiGetDeviceInterfaceDetailA(
              dev_info, &devint_data, devint_detail_data, size, &size, NULL)) {
        printf("SetupDiGetDeviceInterfaceDetail (actual) failed:\n");
        continue;
      }

      hDrive = CreateFileA(devint_detail_data->DevicePath,
                           GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hDrive == INVALID_HANDLE_VALUE) {
        printf("Could not open '%s':\n", devint_detail_data->DevicePath);
        continue;
      }

      drive_number = GetDriveNumber(hDrive);

      if (drive_number < 0)
        continue;

      // kendall: ok
      drive_index = drive_number + DRIVE_INDEX_MIN;
      // everything that comes after this is just a filter for drive_index
      // TODO(kendall): we can remove some of these filters or write our own
      if (!IsMediaPresent(drive_index)) {
        printf("Device eliminated because it appears to contain no media\n");
        safe_closehandle(hDrive);
        safe_free(devint_detail_data);
        break;
      }
      if (GetDriveSize(drive_index) < (MIN_DRIVE_SIZE * MB)) {
        printf("Device eliminated because it is smaller than %d MB\n",
               MIN_DRIVE_SIZE);
        safe_closehandle(hDrive);
        safe_free(devint_detail_data);
        break;
      }

      if (GetDriveLabel(drive_index, drive_letters, &label)) {
        if ((props.is_SCSI) && (!props.is_UASP) && (!props.is_VHD)) {
          if (!props.is_Removable) {
            // Non removables should have been eliminated above, but since we
            // are potentially dealing with system drives, better safe than
            // sorry
            safe_closehandle(hDrive);
            safe_free(devint_detail_data);
            break;
          }
          if (!list_non_usb_removable_drives) {
            // Go over the mounted partitions and find if GetDriveType() says
            // they are
            // removable. If they are not removable, don't allow the drive to be
            // listed
            for (p = drive_letters; *p; p++) {
              drive_name[0] = *p;
              if (GetDriveTypeA(drive_name) != DRIVE_REMOVABLE)
                break;
            }
            if (*p) {
              printf(
                  "Device eliminated because it contains a mounted partition "
                  "that is set as non-removable");
              safe_closehandle(hDrive);
              safe_free(devint_detail_data);
              break;
            }
          }
        }
        if ((!enable_HDDs) && (!props.is_VHD) && (!props.is_CARD) &&
            ((score = IsHDD(drive_index, (uint16_t)props.vid,
                            (uint16_t)props.pid, buffer)) > 0)) {
          printf(
              "Device eliminated because it was detected as a Hard Drive "
              "(score %d > 0)",
              score);
          if (!list_non_usb_removable_drives)
            printf(
                "If this device is not a Hard Drive, please e-mail the author "
                "of this application");
          printf(
              "NOTE: You can enable the listing of Hard Drives in 'Advanced "
              "Options' (after clicking the white triangle)");
          safe_closehandle(hDrive);
          safe_free(devint_detail_data);
          break;
        }

        safe_closehandle(hDrive);
        safe_free(devint_detail_data);
      }
      // here we are.  the device has not been eliminated!  that is great!
      // if we don't already have a ret, make this our ret
      printf("kendall: %lu qualified\n", drive_index);
      uint64_t num_bytes = GetDriveSize(drive_index);
      device_list->emplace_back(DeviceGuy(drive_index, buffer, num_bytes));
    }
  }

  SetupDiDestroyDeviceInfoList(dev_info);

out:
  // Set 'Start' as the selected button, so that tab selection works
  safe_free(devid_list);
  StrArrayDestroy(&dev_if_path);
  htab_destroy(&htab_devid);
}
// end GetDevices()
/*
 * Unmount of volume using the DISMOUNT_VOLUME ioctl
 */
static bool UnmountVolume(HANDLE hDrive) {
  DWORD size;

  if (!DeviceIoControl(hDrive, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &size,
                       NULL)) {
    printf("Could not unmount drive: \n");
    return false;
  }
  return true;
}

/*
 * Return the first GUID volume name for the associated drive or NULL if not
 * found
 * See http://msdn.microsoft.com/en-us/library/cc542456.aspx
 * The returned string is allocated and must be freed
 */
static char* GetLogicalName(DWORD DriveIndex, bool bKeepTrailingBackslash) {
  bool success = false;
  char volume_name[MAX_PATH];
  HANDLE hDrive = INVALID_HANDLE_VALUE, hVolume = INVALID_HANDLE_VALUE;
  size_t len;
  char path[MAX_PATH];
  VOLUME_DISK_EXTENTS_REDEF DiskExtents;
  DWORD size;
  UINT drive_type;
  int i, j;
  static const char* ignore_device[] = {"\\Device\\CdRom", "\\Device\\Floppy"};
  static const char* volume_start = "\\\\?\\";

  CheckDriveIndex(DriveIndex);

  for (i = 0; hDrive == INVALID_HANDLE_VALUE; i++) {
    if (i == 0) {
      hVolume = FindFirstVolumeA(volume_name, sizeof(volume_name));
      if (hVolume == INVALID_HANDLE_VALUE) {
        printf("Could not access first GUID volume\n");
        goto out;
      }
    } else {
      if (!FindNextVolumeA(hVolume, volume_name, sizeof(volume_name))) {
        if (GetLastError() != ERROR_NO_MORE_FILES) {
          printf("Could not access next GUID volume\n");
        }
        goto out;
      }
    }

    // Sanity checks
    len = safe_strlen(volume_name);
    if ((len <= 1) || (safe_strnicmp(volume_name, volume_start, 4) != 0) ||
        (volume_name[len - 1] != '\\')) {
      printf("'%s' is not a GUID volume name\n", volume_name);
      continue;
    }

    drive_type = GetDriveTypeA(volume_name);
    if ((drive_type != DRIVE_REMOVABLE) && (drive_type != DRIVE_FIXED))
      continue;

    volume_name[len - 1] = 0;

    if (QueryDosDeviceA(&volume_name[4], path, sizeof(path)) == 0) {
      printf("Failed to get device path for GUID volume '%s':\n", volume_name);
      continue;
    }

    for (j = 0; (j < ARRAYSIZE_SIGNED(ignore_device)) &&
                (_strnicmp(path, ignore_device[j],
                           safe_strlen(ignore_device[j])) != 0);
         j++)
      ;
    if (j < ARRAYSIZE_SIGNED(ignore_device)) {
      printf("Skipping GUID volume for '%s'\n", path);
      continue;
    }

    hDrive = CreateFileA(volume_name, GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
      printf("Could not open GUID volume '%s': \n", volume_name);
      continue;
    }

    if ((!DeviceIoControl(hDrive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0,
                          &DiskExtents, sizeof(DiskExtents), &size, NULL)) ||
        (size <= 0)) {
      printf("Could not get Disk Extents:\n");
      safe_closehandle(hDrive);
      continue;
    }
    safe_closehandle(hDrive);
    if ((DiskExtents.NumberOfDiskExtents >= 1) &&
        (DiskExtents.Extents[0].DiskNumber == DriveIndex)) {
      if (bKeepTrailingBackslash)
        volume_name[len - 1] = '\\';
      success = true;
      break;
    }
  }

out:
  if (hVolume != INVALID_HANDLE_VALUE)
    FindVolumeClose(hVolume);
  return (success) ? safe_strdup(volume_name) : NULL;
}

/*
 * Open a drive or volume with optional write and lock access
 * Return INVALID_HANDLE_VALUE (/!\ which is DIFFERENT from NULL /!\) on
 * failure.
 */
static HANDLE OldGetHandle(char* Path,
                           bool bLockDrive,
                           bool bWriteAccess,
                           bool bWriteShare) {
  int i;
  DWORD size;
  HANDLE hDrive = INVALID_HANDLE_VALUE;
  char DevPath[MAX_PATH];

  if ((safe_strlen(Path) < 5) || (Path[0] != '\\') || (Path[1] != '\\') ||
      (Path[3] != '\\'))
    goto out;

  // Resolve a device path, so that users can seek for it in Process Explorer
  // in case of access issues.
  if (QueryDosDeviceA(&Path[4], DevPath, sizeof(DevPath)) == 0)
    strcpy(DevPath, "???");

  for (i = 0; i < DRIVE_ACCESS_RETRIES; i++) {
    // Try without FILE_SHARE_WRITE (unless specifically requested) so that
    // we won't be bothered by the OS or other apps when we set up our data.
    // However this means we might have to wait for an access gap...
    // We keep FILE_SHARE_READ though, as this shouldn't hurt us any, and is
    // required for enumeration.
    hDrive =
        CreateFileA(Path, GENERIC_READ | (bWriteAccess ? GENERIC_WRITE : 0),
                    FILE_SHARE_READ |
                        ((bWriteAccess && bWriteShare) ? FILE_SHARE_WRITE : 0),
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDrive != INVALID_HANDLE_VALUE)
      break;
    if ((GetLastError() != ERROR_SHARING_VIOLATION) &&
        (GetLastError() != ERROR_ACCESS_DENIED))
      break;
    if (i == 0) {
      printf("Waiting for access...");
    } else if (bWriteAccess && !bWriteShare && (i > DRIVE_ACCESS_RETRIES / 3)) {
      // If we can't seem to get a hold of the drive for some time,
      // try to enable FILE_SHARE_WRITE...
      printf(
          "Warning: Could not obtain exclusive rights. Retrying with write "
          "sharing enabled...");
      bWriteShare = true;
    }
    Sleep(DRIVE_ACCESS_TIMEOUT / DRIVE_ACCESS_RETRIES);
  }
  if (hDrive == INVALID_HANDLE_VALUE) {
    printf("Could not open %s [%s]\n", Path, DevPath);
    goto out;
  }

  if (bWriteAccess) {
    printf("Opened %s [%s] for write access\n", Path, DevPath);
  }

  if (bLockDrive) {
    if (DeviceIoControl(hDrive, FSCTL_ALLOW_EXTENDED_DASD_IO, NULL, 0, NULL, 0,
                        &size, NULL)) {
      printf("I/O boundary checks disabled\n");
    }

    for (i = 0; i < DRIVE_ACCESS_RETRIES; i++) {
      if (DeviceIoControl(hDrive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &size,
                          NULL))
        goto out;
      if (IS_ERROR(FormatStatus))  // User cancel
        break;
      Sleep(DRIVE_ACCESS_TIMEOUT / DRIVE_ACCESS_RETRIES);
    }
    // If we reached this section, either we didn't manage to get a lock or the
    // user cancelled
    printf("Could not get exclusive access to %s [%s]\n", Path, DevPath);
    safe_closehandle(hDrive);
  }

out:
  return hDrive;
}

/*
 * Obtain a handle to the first logical volume on the disk identified by
 * DriveIndex
 * Returns INVALID_HANDLE_VALUE on error or NULL if no logical path exists
 * (typical
 * of unpartitioned drives)
 */
static HANDLE GetLogicalHandle(DWORD DriveIndex,
                               bool bLockDrive,
                               bool bWriteAccess,
                               bool bWriteShare) {
  HANDLE hLogical = INVALID_HANDLE_VALUE;
  char* LogicalPath = GetLogicalName(DriveIndex, false);

  if (LogicalPath == NULL) {
    printf("No logical drive found (unpartitioned?)\n");
    return NULL;
  }

  hLogical = OldGetHandle(LogicalPath, bLockDrive, bWriteAccess, bWriteShare);
  free(LogicalPath);
  return hLogical;
}

// from drive.c
static bool RefreshDriveLayout(HANDLE hDrive) {
  bool r;
  DWORD size;

  // Diskpart does call the following IOCTL this after updating the partition
  // table, so we do too
  r = DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0,
                      &size, NULL);
  if (!r)
    printf("Could not refresh drive layout\n");
  return r;
}

// not really from drive.c, but more or less.
/*
 * Return the drive size
 */

// ok, this could be useful in general.  i can return DiskGeometry->whatever

static uint64_t GetSectorSize(DWORD DriveIndex) {
  bool r;
  HANDLE hPhysical;
  DWORD size;
  BYTE geometry[256];
  PDISK_GEOMETRY_EX DiskGeometry = (PDISK_GEOMETRY_EX)(void*)geometry;

  hPhysical = GetPhysicalHandle(DriveIndex, false, false);
  if (hPhysical == INVALID_HANDLE_VALUE)
    return false;

  r = DeviceIoControl(hPhysical, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0,
                      geometry, sizeof(geometry), &size, NULL);
  safe_closehandle(hPhysical);
  if (!r || size <= 0)
    return 0;
  return DiskGeometry->Geometry.BytesPerSector;
}

// from format.c
static bool WriteDrive(HANDLE hPhysicalDrive,
                       HANDLE hSourceImage,
                       uint64_t sector_size,
                       uint64_t drive_size,
                       int64_t image_size) {
  bool s, ret = false;
  LARGE_INTEGER li;
  DWORD rSize, wSize, BufSize;
  // ok; i found the logic for this in vhd.c.  we have the handles

  // previous logic (rufus) also casted a signed int into an unsigned int here,
  // just using LARGE_INTEGER union as a middleman
  uint64_t projected_size = (uint64_t)image_size;

  uint64_t wb, target_size = projected_size;
  uint8_t* buffer = NULL;
  int i;

  // We poked the MBR and other stuff, so we need to rewind
  li.QuadPart = 0;
  printf("kendall: before if check\n");
  // works when i return here
  // return true;
  if (!SetFilePointerEx(hPhysicalDrive, li, NULL, FILE_BEGIN))
    printf(
        "Warning: Unable to rewind image position - wrong data might be "
        "copied!");

  // the image won't be compressed
  printf(hSourceImage ? "Writing Image..." : "Zeroing drive...");
  printf("kendall: we are writing the image now!\n");
  // Our buffer size must be a multiple of the sector size and *ALIGNED* to the
  // sector size
  printf("kendall: sector size: %llu\n", sector_size);
  if (sector_size < 512) {
    sector_size = 512;
  }
  // return true;
  BufSize = ((DD_BUFFER_SIZE + sector_size - 1) / sector_size) * sector_size;
  buffer = (uint8_t*)_mm_malloc(BufSize, sector_size);
  if (buffer == NULL) {
    FormatStatus =
        ERROR_SEVERITY_ERROR | FAC(FACILITY_STORAGE) | ERROR_NOT_ENOUGH_MEMORY;
    printf("Could not allocate disk write buffer");
    goto out;
  }
  // Sanity check
  if ((uintptr_t)buffer % sector_size != 0) {
    FormatStatus =
        ERROR_SEVERITY_ERROR | FAC(FACILITY_STORAGE) | ERROR_READ_FAULT;
    printf("Write buffer is not aligned");
    goto out;
  }
  // Don't bother trying for something clever, using double buffering overlapped
  // and whatnot:
  // With Windows' default optimizations, sync read + sync write for sequential
  // operations
  // will be as fast, if not faster, than whatever async scheme you can come up
  // with.
  rSize = BufSize;
  // i made this
  for (wb = 0, wSize = 0; wb < drive_size; wb += wSize) {
    if (hSourceImage != NULL) {
      s = ReadFile(hSourceImage, buffer, BufSize, &rSize, NULL);
      if (!s) {
        FormatStatus =
            ERROR_SEVERITY_ERROR | FAC(FACILITY_STORAGE) | ERROR_READ_FAULT;
        printf("read error:\n");
        goto out;
      }
      if (rSize == 0)
        break;
    }
    // Don't overflow our projected size (mostly for VHDs)
    if (wb + rSize > target_size) {
      rSize = (DWORD)(target_size - wb);
    }

    // WriteFile fails unless the size is a multiple of sector size
    if (rSize % sector_size != 0)
      rSize = ((rSize + sector_size - 1) / sector_size) * sector_size;
    for (i = 0; i < WRITE_RETRIES; i++) {
      s = WriteFile(hPhysicalDrive, buffer, rSize, &wSize, NULL);
      if ((s) && (wSize == rSize))
        break;
      if (s)
        printf("write error: Wrote %lu bytes, expected %lu bytes", wSize,
               rSize);
      else
        printf("write error at sector %llu:\n", wb / sector_size);
      if (i < WRITE_RETRIES - 1) {
        li.QuadPart = wb;
        printf("  RETRYING...\n");
        if (!SetFilePointerEx(hPhysicalDrive, li, NULL, FILE_BEGIN)) {
          printf("write error: could not reset position -");
          goto out;
        }
      } else {
        FormatStatus =
            ERROR_SEVERITY_ERROR | FAC(FACILITY_STORAGE) | ERROR_WRITE_FAULT;
        goto out;
      }
      Sleep(200);
    }
    if (i >= WRITE_RETRIES)
      goto out;
  }
  RefreshDriveLayout(hPhysicalDrive);
  ret = true;
out:
  safe_mm_free(buffer);
  return ret;
}

DeviceGuyList GetDeviceList() {
  DeviceGuyList device_list;
  GetDevices(&device_list);
  return device_list;
}

static bool formatShared(char* physical_path) {
  LOG_INFO << "using physical_path=" << physical_path;
  bool success = clearMbrGpt(physical_path);
  if (!success) {
    LOG_WARNING << "error clearing mbr/gpt";
    // The operation is unlikely to succeed if there was an error cleaning gpt
    return false;
  }
  LOG_INFO << "success clearing mbr/gpt";
  return true;
}

bool Install(DeviceGuy* target_device,
             const char* image_path,
             int64_t image_size) {
  uint64_t device_num = target_device->device_num;
  uint64_t sector_size = GetSectorSize(device_num);
  uint64_t drive_size = GetDriveSize(device_num);
  char* physical_path = GetPhysicalName(device_num);
  if (!formatShared(physical_path)) {
    // pass up the failure
    return false;
  }
  HANDLE phys_handle = GetHandle(physical_path, true, true, false);
  // HANDLE phys_handle = GetHandle(physical_path, true, true, true);
  // ^ i have not noticed any difference in behavior whether we share or not
  HANDLE source_img =
      CreateFileU(image_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                  OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  bool ret = false;
  // TODO(kendall): make sure the handlers don't equal INVALID_HANDLE_VALUE
  safe_free(physical_path);
  if (phys_handle != INVALID_HANDLE_VALUE &&
      source_img != INVALID_HANDLE_VALUE) {
    printf("handles are valid!\n");
  }
  HANDLE hLogicalVolume = GetLogicalHandle(device_num, true, false, false);
  if (hLogicalVolume == INVALID_HANDLE_VALUE) {
    printf("kendall: Could not lock volume\n");
  }

  UnmountVolume(hLogicalVolume);
  if (phys_handle == INVALID_HANDLE_VALUE) {
    printf("kendall: physical handle invalid!\n");
  }

  ret =
      WriteDrive(phys_handle, source_img, sector_size, drive_size, image_size);

  // close the handles we created so that Install() may be called again
  // within this same run
  safe_closehandle(phys_handle);
  safe_closehandle(hLogicalVolume);
  safe_closehandle(source_img);

  return ret;
}

bool Format(DeviceGuy* target_device) {
  uint64_t device_num = target_device->device_num;
  char* physical_path = GetPhysicalName(device_num);
  bool ret = formatShared(physical_path);
  if (!ret) {
    // logging handled by formatShared already
    return ret;
  }
  ret = makeEmptyPartition(physical_path);
  if (!ret) {
    LOG_WARNING << "Error creating empty fat32 partition";
  }
  safe_free(physical_path);
  return ret;
}
