#include <include.h>

NTSTATUS HKCALL hooks::CallProcessNotifyRoutines(PEPROCESS pProcess, int64_t dwUnk1, bool bCreate)
{
  DECLARE_HOOK_DATA(CallProcessNotifyRoutines);

  INVOKE_HOOK_ORIGINAL(pProcess, dwUnk1, bCreate);

  if (bCreate)
    g::kmon->on_process_launch(pProcess);
  else
    g::kmon->on_process_exit(pProcess);

  return originalResult;
}

NTSTATUS HKCALL hooks::QuerySystemInfo(
  k::SYSTEM_INFORMATION_CLASS nInfoClass,
  void* a2,
  unsigned int a3,
  void* pInfo,
  ULONG uInfoLen,
  PULONG uInfoLenRet
)
{
  DECLARE_HOOK_DATA(QuerySystemInfo);

  INVOKE_HOOK_ORIGINAL(
    nInfoClass,
    a2,
    a3,
    pInfo,
    uInfoLen,
    uInfoLenRet
  );

  if (NT_SUCCESS(originalResult))
  {
    if (nInfoClass == k::SystemCodeIntegrityInformation)
    {
      auto pBuf = reinterpret_cast<k::PSYSTEM_CODEINTEGRITY_INFORMATION>(pInfo);

      if ( BITFLAG_IS_SET(pBuf->Options, k::CODEINTEGRITY_OPTION_TESTSIGN) )
        pBuf->Options &= ~k::CODEINTEGRITY_OPTION_TESTSIGN;
    }
  }

  return originalResult;
}
