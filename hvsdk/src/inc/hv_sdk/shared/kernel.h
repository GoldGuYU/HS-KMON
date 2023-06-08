#pragma once

namespace k
{
  namespace ex
  {
    struct notify_callback_t
    {
      uint64_t context;
      uintptr_t base;
      uintptr_t routine;
    };
  }

  //0x4 bytes (sizeof)
  struct _MMVAD_FLAGS
  {
    ULONG Lock : 1;                                                           //0x0
    ULONG LockContended : 1;                                                  //0x0
    ULONG DeleteInProgress : 1;                                               //0x0
    ULONG NoChange : 1;                                                       //0x0
    ULONG VadType : 3;                                                        //0x0
    ULONG Protection : 5;                                                     //0x0
    ULONG PreferredNode : 6;                                                  //0x0
    ULONG PageSize : 2;                                                       //0x0
    ULONG PrivateMemory : 1;                                                  //0x0
  };

  enum MM_PROTECTIONS
  {
    MM_ZERO_ACCESS = 0,
    MM_READONLY,
    MM_EXECUTE,
    MM_EXECUTE_READ,
    MM_READWRITE,
    MM_WRITECOPY,
    MM_EXECUTE_READWRITE,
    MM_EXECUTE_WRITECOPY,
  };

  typedef struct _SYSTEM_MODULE_ENTRY
  {
    HANDLE Section;
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR FullPathName[256];
  } SYSTEM_MODULE_ENTRY, * PSYSTEM_MODULE_ENTRY;
}
