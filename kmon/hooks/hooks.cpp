#include <include.h>

hooks::Entry_t::Entry_t(LPCSTR _Name, BOOLEAN _Used,
  Data_t* _Data, PVOID _Addr, PVOID _Handler)
{
  strcpy_s(Name, _Name);

  Used = _Used;
  Data = _Data;
  Handler = _Handler;
  Data->pTarget = _Addr;
}

BOOLEAN hooks::Initialize()
{
  hook_list =
  {
    HOOK_ENTRY(TRUE, &data::ShutdownSystem, SYSCALL_PTR(SYSCALL_ID("ZwShutdownSystem")), &ShutdownSystem),
  };

  LOG_WARN("About to install hooks!");

  if (!PlaceHooks())
  {
    LOG_WARN("failed to install hooks.");
    return FALSE;
  }

  return TRUE;
}

NTSTATUS hooks::PlaceHook(const Entry_t& _entry, Data_t& _data)
{
  if (!util::IsMemoryValid(_entry.Data->pTarget))
  {
    LOG_WARN("the hook with name '%s' has an invalid target (%p).", _entry.Name, _entry.Data->pTarget);
    return STATUS_INVALID_PARAMETER;
  }

  auto result = STATUS_UNSUCCESSFUL;

  if (enable_shadowing)
  {
    if (sh::mgr->place_hook_krn(_data.pTarget, _entry.Handler, &_data.pOriginal, &_data.uBackupLength))
      result = STATUS_SUCCESS;
  }
  else
  {
    if (hook::detour::Place(_data.pTarget,
      _entry.Handler,
      &_data.pOriginal,
      &_data.uBackupLength,
      nullptr, nullptr,
      FALSE, TRUE
    ))
      result = STATUS_SUCCESS;
  }

  return result;
}

BOOLEAN hooks::PlaceHooks()
{
  SIZE_T nEnabledEntries = 0;

  SIZE_T nEntryFailedIndex = 0;
  PVOID pEntryFailedTarget = 0;

  BOOLEAN bEntryFailed = FALSE;
  DWORD dwEntryFailedCode = 0;

  for (const auto& sHookEntry : hook_list)
  {
    if (!sHookEntry.Used)
      continue;

    auto& rsData = *sHookEntry.Data;

    if (sHookEntry.Data->pTarget == nullptr)
    {
      LOG_WARN("hook entry %i '%s' target is null!",
        nEnabledEntries, sHookEntry.Name);
      continue;
    }

    NT_ASSERT(sHookEntry.Data->pTarget);

    const auto dwStatus = PlaceHook(sHookEntry, *sHookEntry.Data);

    if (dwStatus == 0x00)
    {
      ++nEnabledEntries;
      LOG_INFO_SAFE("register_hooks:  entry %i '%s' has been enabled.",
        nEnabledEntries, sHookEntry.Name);
    }
    else
    {
      bEntryFailed = TRUE;
      dwEntryFailedCode = dwStatus;

      nEntryFailedIndex = (nEnabledEntries + 1);
      pEntryFailedTarget = sHookEntry.Data->pTarget;
      break;
    }
  }

  // if one entry could not be enabled just undo everything.
  // if (bEntryFailed && nEnabledEntries > 0)
  //  hv::inst->shm()->shutdown_managers();

  if (bEntryFailed)
  {
    LOG_WARN("register_hooks:  entry %i with target %p "
      "could not be enabled, so the other ones have been shut down (error code= %x).",
      nEntryFailedIndex, pEntryFailedTarget, dwEntryFailedCode);
    return FALSE;
  }

  enabled_entries = nEnabledEntries;

  if (!PlacePatches())
  {
    LOG_WARN("failed to install patches.");
    return FALSE;
  }

  sh::mgr->split();

  memset(hook_list.data(), 0, hook_list.size() * sizeof(hook_list[0]));

  return TRUE;
}

BOOLEAN hooks::PlacePatches()
{
  return TRUE;
}
