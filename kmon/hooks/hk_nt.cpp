#include <include.h>

NTSTATUS HKCALL hooks::ShutdownSystem(k::SHUTDOWN_ACTION Action, PVOID Extra1)
{
  DECLARE_HOOK_DATA(ShutdownSystem);

  if (mem::addr(Action).Base() == sdk::comms::Magic)
    return Comms::Intercepts::ShutdownSystem(Action, Extra1);

  INVOKE_HOOK_ORIGINAL(Action, 0);

  return originalResult;
}
