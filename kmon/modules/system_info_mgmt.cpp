#include <include.h>

using namespace kmon;

void c_kmon::on_query_system_info(k::SYSTEM_INFORMATION_CLASS sInfoClass,
  PVOID pBuffer, ULONG uLength, PULONG puReturnLength)
{
  if (!pBuffer || !uLength)
    return;

  switch (sInfoClass)
  {
  case k::SystemCodeIntegrityInformation:
  {
    auto* const pInfo = mem::addr(pBuffer).As<k::PSYSTEM_CODEINTEGRITY_INFORMATION>();
    auto& uCiFlags = pInfo->Options;

    if( !(uCiFlags & k::CODEINTEGRITY_OPTION_ENABLED) )
      uCiFlags |= k::CODEINTEGRITY_OPTION_ENABLED;

    if (uCiFlags & k::CODEINTEGRITY_OPTION_DEBUGMODE_ENABLED)
      uCiFlags &= ~k::CODEINTEGRITY_OPTION_DEBUGMODE_ENABLED;

    if (uCiFlags & k::CODEINTEGRITY_OPTION_TESTSIGN)
      uCiFlags &= ~k::CODEINTEGRITY_OPTION_TESTSIGN;

    break;
  }
  case k::SystemKernelDebuggerInformation:
  {
    typedef struct _SYSTEM_KERNEL_DEBUGGER_INFORMATION {
      BOOLEAN DebuggerEnabled;
      BOOLEAN DebuggerNotPresent;
    } SYSTEM_KERNEL_DEBUGGER_INFORMATION, * PSYSTEM_KERNEL_DEBUGGER_INFORMATION;

    auto* const pInfo = mem::addr(pBuffer).As<PSYSTEM_KERNEL_DEBUGGER_INFORMATION>();

    pInfo->DebuggerEnabled = FALSE;
    pInfo->DebuggerNotPresent = TRUE;

    break;
  }
  default:
    break;
  }
}
