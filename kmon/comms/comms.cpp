#include <include.h>

using namespace hs::sdk::comms;

//
// This thread will trigger a bug check when launched.
// It is essential for testing if our bug checking code works.
// If it does, this thread will be stalled.
// Make sure to restart your machine after executing this test:
// You will have an unsigned system thread running if you don't.
//
VOID BCTestThread()
{
  LOG_INFO("BCTestThread:  triggering test bug check.");

  KeBugCheck(0x666);

  LOG_INFO("BCTestThread:  continued execution after BC, this should ideally NOT happen.");
}

NTSTATUS Comms::Intercepts::ShutdownSystem(k::SHUTDOWN_ACTION Action, PVOID Extra1)
{
  auto* const pContext = reinterpret_cast<Context_t*>(Extra1);

  if (pContext == nullptr)
    return STATUS_UNSUCCESSFUL;

  return DispatchRequest(pContext);
}

NTSTATUS Comms::DispatchRequest(Context_t* pContext)
{
  BOOLEAN bCompleted = TRUE;

  switch (pContext->Id)
  {
  case 0xD00:
    DEBUG_BREAK();
    break;
  case 0xD02:
    util::CreateThread(&BCTestThread);
    bCompleted = TRUE;
    break;
  case 0xD08:
    bCompleted = HandleQueryInfo(pContext->Data);
    break;
  case 0xD18:
    bCompleted = Mmp::HandleTranslateVirtToPhys(pContext);
    break;
  case 0xD20:
    bCompleted = Mmp::HandleReadVirtualMemory(pContext);
    break;
  case 0xD22:
    bCompleted = Mmp::HandleWriteVirtualMemory(pContext);
    break;
  case 0xD24:
    bCompleted = Mmp::HandleReadPhysicalMemory(pContext);
    break;
  case 0xD26:
    bCompleted = Mmp::HandleWritePhysicalMemory(pContext);
    break;
  case 0xD28:
    bCompleted = Mmp::HandleGetMainModuleBase(pContext);
    break;
  case 0xD48:
    bCompleted = Mmp::HandleGetModuleList(pContext);
    break;
  case 0xD40:
    bCompleted = Mmp::HandleGetModuleBase(pContext);
    break;
  case 0xD42:
    bCompleted = Mmp::HandleAllocateVirtualMemory(pContext);
    break;
  case 0xD44:
    bCompleted = Mmp::HandleProtectVirtualMemory(pContext);
    break;
  case 0xD36:
    bCompleted = HandleGetSystemModule(pContext);
    break;
  case 0xD38:
    bCompleted = HandleMsrAccess(pContext->Data);
    break;
  case 0xD58:
    bCompleted = HandleInfoTableRead(pContext);
    break;
  case 0xD60:
    bCompleted = HandleInfoTableWrite(pContext);
    break;
  case 0xD62:
    bCompleted = Mmp::HandleMapImage(pContext);
    break;
  case 0xD64:
    bCompleted = HandleClearBackLog();
    break;
  case 0xD66:
    bCompleted = HandleGetSystemModuleExport(pContext->Data);
    break;
  case 0xD68:
    bCompleted = HandleTriggerPgChecks();
    break;
  case 0xF01:
    bCompleted = Hv::HandleAddShadowDataWatcher(pContext);
    break;
  default:
    bCompleted = FALSE;
    break;
  }

  if (g::back_log.full())
  {
    hvpp_trace("resetting backlog because it's full.");

    g::back_log.reset();
  }

  return (bCompleted ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
}

BOOLEAN Comms::PostResult(sdk::comms::InfoQuery_t* info, PVOID wish_data, SIZE_T wish_data_length)
{
  if (!util::IsMemoryValid(info->Buffer))
  {
    *info->Length = wish_data_length;
    info->Result = HVI_QUERY_RESULT_INSUFFICIENT_MEMORY;

    return FALSE;
  }

  if (*info->Length < wish_data_length)
  {
    *info->Length = wish_data_length;
    info->Result = HVI_QUERY_RESULT_BUFFER_TOO_SMALL;

    return FALSE;
  }

  *info->Length = wish_data_length;

  RtlCopyMemory(info->Buffer, wish_data, wish_data_length);

  return TRUE;
}

BOOLEAN Comms::HandleQueryInfo(PVOID pData)
{
  auto* const info = mem::addr(pData)
    .As<InfoQuery_t*>();

  if (info == nullptr)
    return FALSE;

  if (!info->Id || !info->Buffer
    || !info->Length || !(*info->Length))
    return FALSE;

  BOOLEAN result = TRUE;

  switch(info->Id)
  {
  case HVI_STATUS:
  {
    result = PostResult(info, &g::status, sizeof(g::status));
    break;
  }
  case HVI_BACK_LOG:
  {
    result = PostResult(info, (void*)g::back_log.get(), g::back_log.length());
    break;
  }
  case HVI_DIAG_INFO:
  {
    result = PostResult(info, g::diagnose_info, g::diagnose_info_length);
    break;
  }
  case HVI_AC_CONTEXT_EAC:
  {
    /*if (modules::eac == nullptr)
    {
      info->Result = HVI_QUERY_RESULT_INSUFFICIENT_MEMORY;
      result = FALSE;
    }
    else
    {
      auto eac_context = modules::eac->get_context();
      result = PostResult(info, &eac_context, sizeof(eac_context));
    }*/

    break;
  }
  case HVI_OS_LOADED_DRIVERS:
  {
    using data_t = hs::sdk::loaded_driver_data_t;

    auto list = util::GetKernelModules();
    auto count = list.size();

    auto length = sizeof(hs::sdk::header_t) + (count * sizeof(data_t::entry_t));
    auto data = (data_t*)util::MemAlloc(length);

    if (data)
    {
      RtlZeroMemory(data, length);

      data->hdr.count = count;

      for (auto i = 0ull; i < count; i++)
      {
        auto& slot = data->m[i];
        auto& entry = list[i];

        slot = entry;
      }

      result = PostResult(info, data, length);
      util::MemFree(data);
    }

    break;
  }
  default:
    result = FALSE;
    break;
  }

  if(result)
    info->Result = HVI_QUERY_RESULT_SUCCESS;

  return result;
}

BOOLEAN Comms::HandleGetSystemModule(Context_t* pContext)
{
  auto sModuleEntry = k::SYSTEM_MODULE_ENTRY{};

  auto* const pszName =
    reinterpret_cast<char*>(pContext->VRegs.vcx);

  auto* const pOutInfo =
    reinterpret_cast<k::PSYSTEM_MODULE_ENTRY>(pContext->VRegs.vdx);

  if (!util::GetKernelModuleInfo(pszName, sModuleEntry))
    return FALSE;

  RtlCopyMemory(pOutInfo,
    &sModuleEntry, sizeof(sModuleEntry));

  return TRUE;
}

BOOLEAN Comms::HandleMsrAccess(PVOID pData)
{
  auto* const pInfo =
    mem::addr(pData)
    .As<MsrAccess_t*>();

  if (pInfo == nullptr || pInfo->Buffer == nullptr)
    return FALSE;

  switch (pInfo->Type)
  {
  case MsrAccess_t::MSR_ACCESS_READ:
  {
    *pInfo->Buffer = __readmsr(pInfo->Address);
    break;
  }
  case MsrAccess_t::MSR_ACCESS_WRITE:
  {
    __writemsr(pInfo->Address, *pInfo->Buffer);
    break;
  }
  default:
    return FALSE;
  }

  return TRUE;
}

BOOLEAN Comms::HandleInfoTableRead(Context_t* pContext)
{
  const auto table_id = pContext->VRegs.vcx;
  const auto entry_id = pContext->VRegs.vdx;

  if (!g::info_table_mgr.table_present(table_id))
    return FALSE;

  if (!g::info_table_mgr.table_read_value(table_id, entry_id, &pContext->VRegs.v8))
    return FALSE;

  return TRUE;
}

BOOLEAN Comms::HandleInfoTableWrite(Context_t* pContext)
{
  const auto table_id = pContext->VRegs.vcx;
  const auto entry_id = pContext->VRegs.vdx;
  const auto entry_value = pContext->VRegs.v8;

  if (!g::info_table_mgr.table_present(table_id))
    return FALSE;

  if (!g::info_table_mgr.table_write_value(table_id, entry_id, entry_value))
    return FALSE;

  //
  // on write, update the cached info table counterparts.
  //
  g::it::update();

  return TRUE;
}

BOOLEAN Comms::HandleClearBackLog()
{
  if (!g::back_log.valid())
    return FALSE;

  g::back_log.reset(true);

  return TRUE;
}

BOOLEAN Comms::HandleGetSystemModuleExport(PVOID pData)
{
  auto* const pInfo =
    mem::addr(pData)
    .As<SystemModuleExportInfo_t*>();

  if (pInfo == nullptr)
    return FALSE;

  const auto dwModuleBase = util::GetKernelModuleBase(pInfo->ModuleName);

  if (dwModuleBase == NULL)
    return FALSE;

  if ((pInfo->Result = util::FindSystemModuleExport<DWORD_PTR>((PVOID)dwModuleBase, pInfo->RoutineName)) == NULL)
    return FALSE;

  return !(pInfo->Result == NULL);
}

BOOLEAN Comms::HandleTriggerPgChecks()
{
  for(auto i = 0ull; i < 4096; i++)
    kmon_asm_trigger_sw_interrupt();

  return TRUE;
}
