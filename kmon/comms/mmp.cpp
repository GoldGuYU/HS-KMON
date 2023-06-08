#include <include.h>

BOOLEAN Comms::Mmp::HandleTranslateVirtToPhys(sdk::comms::Context_t* pContext)
{
  uint64_t virtual_address = pContext->VRegs.vcx;
  uint64_t target_cr3 = pContext->VRegs.v8;

  auto* const result = mem::addr(pContext->VRegs.vdx).As<uint64_t*>();

  if (!virtual_address || !result)
    return FALSE;

  pa_t physical_address{};

  if (target_cr3 == NULL)
    target_cr3 = phys_mem::GetProcessCr3(PsGetCurrentProcess());

  physical_address = phys_mem::TranslateLinearAddress(target_cr3, virtual_address);

  if (physical_address)
  {
    *result = physical_address.value();
    return TRUE;
  }

  return FALSE;
}

BOOLEAN Comms::Mmp::HandleGetMainModuleBase(sdk::comms::Context_t* pContext)
{
  const uint32_t process_id = pContext->VRegs.vcx;
  auto* const result = mem::addr(pContext->VRegs.vdx).As<uint64_t*>();

  if (!process_id || !result)
    return FALSE;

  if (!util::IsProcessValid(process_id))
    return FALSE;

  const auto main_module_base = util::GetProcessBase((HANDLE)process_id);

  if (main_module_base)
  {
    *result = mem::addr(main_module_base).Base();
    return TRUE;
  }

  return FALSE;
}

BOOLEAN Comms::Mmp::HandleGetModuleList(sdk::comms::Context_t* pContext)
{
  const uint32_t process_id = pContext->VRegs.vcx;
  const size_t max_count = pContext->VRegs.vdx;
  auto* const result = mem::addr(pContext->VRegs.v8).As<k::module_info_ex*>();

  if (!process_id || !result || max_count <= 0)
    return FALSE;

  auto buffer = util::GetProcessModules((HANDLE)process_id);

  if (buffer.size() < max_count)
  {
    for (size_t i = 0; i < buffer.size(); i++)
    {
      auto* const entry = &buffer[i];

      if (entry == nullptr)
        continue;

      RtlCopyMemory(&result[i], entry, sizeof(k::module_info_ex));
    }

    pContext->VRegs.v9 = buffer.size();

    return (buffer.size() > 0);
  }

  return FALSE;
}

BOOLEAN Comms::Mmp::HandleGetModuleBase(sdk::comms::Context_t* pContext)
{
  const uint32_t process_id = pContext->VRegs.vcx;
  const size_t name_length = pContext->VRegs.v8;

  auto* const name = mem::addr(pContext->VRegs.vdx).As<LPWSTR>();
  auto* const result = mem::addr(pContext->VRegs.v9).As<k::module_info*>();

  if (!process_id || !result || !name || !name_length)
    return FALSE;

  wchar_t name_buffer[MAX_PATH];
  RtlZeroMemory(name_buffer, sizeof(name_buffer));

  if (name_length > (sizeof(name_buffer) - 1))
    return FALSE;

  RtlCopyMemory(name_buffer, name, name_length);

  auto info = k::module_info{};

  if (util::GetProcessModuleInfo(mem::addr(process_id).As<_handle>(), name, info))
  {
    RtlCopyMemory(result, &info, sizeof(info));

    return TRUE;
  }

  return FALSE;
}

BOOLEAN Comms::Mmp::HandleReadVirtualMemory(sdk::comms::Context_t* pContext)
{
  _IMP_CACHED_NT(MmCopyVirtualMemory);

  auto* const info = mem::addr(pContext->VRegs.vcx)
    .As<read_virtual_memory_t*>();

  if (!info || !info->length ||
    !info->source_address || !info->destination_buffer)
    return FALSE;

  auto result = NTSTATUS{};

  if (info->process_id != 0)
  {
    //
    // Read from user-mode address.
    //

    if (!util::IsAddressWithinUserModeRange(
      info->source_address, info->length)
      )
      return FALSE;

    auto process = PEPROCESS{};
    auto bytes_read = SIZE_T{};

    if (!util::LookupProcess((HANDLE)info->process_id, &process))
      return FALSE;

    result = _IMP_C(MmCopyVirtualMemory)(process, mem::addr(info->source_address).Ptr(),
      PsGetCurrentProcess(), info->destination_buffer, info->length, KernelMode, &bytes_read);
  }
  else
  {
    //
    // Read from kernel-space address.
    //

    if (util::CopyVirtualMemory(info->destination_buffer,
      (void*)info->source_address, info->length))
      result = STATUS_SUCCESS;
  }

  pContext->ReturnStatus = result;

  return NT_SUCCESS(result);
}

BOOLEAN Comms::Mmp::HandleWriteVirtualMemory(sdk::comms::Context_t* pContext)
{
  _IMP_CACHED_NT(MmCopyVirtualMemory);

  auto* const info = mem::addr(pContext->VRegs.vcx)
    .As<write_virtual_memory_t*>();

  if (!info || !info->length ||
    !info->destination_address || !info->source_buffer)
    COMMS_RETURN_WITH_ERROR(STATUS_INVALID_PARAMETER_1);

  if (info->process_id == 0)
    COMMS_RETURN_WITH_ERROR(STATUS_INVALID_PARAMETER_2);

  if (!util::IsAddressWithinUserModeRange(
    info->destination_address, info->length)
    )
    COMMS_RETURN_WITH_ERROR(STATUS_INVALID_PARAMETER_3);

  auto result = NTSTATUS{};
  auto process = PEPROCESS{};
  auto bytes_read = SIZE_T{};

  if(!util::LookupProcess((HANDLE)info->process_id, &process))
    COMMS_RETURN_WITH_ERROR(STATUS_OBJECTID_NOT_FOUND);

  k::c_object_guard __process_guard(process, FALSE);

  if (info->stealth == false)
  {
    result = _IMP_C(MmCopyVirtualMemory)(PsGetCurrentProcess(), info->source_buffer,
      process, mem::addr(info->destination_address).Ptr(),
      info->length, KernelMode, &bytes_read);
  }
  else
  {
    auto dst_page = page_align(info->destination_address);
    auto sp = sh::mgr->find_ex(info->process_id, dst_page, true);

    if (sp)
    {
      auto current_pa = uintptr_t{ };

      if (!util::TranslateUserLinearVirtToPhys(info->process_id, dst_page, &current_pa))
      {
        LOG_WARN("write_stealth: failed va-pa translation.");
        return false;
      }

      if (sp->get_base_phys() != 0 && sp->get_base_phys() != current_pa)
      {
        LOG_WARN("write_stealth: PA of target has changed! (expected=%p, got=%p)", sp->get_base_phys(), current_pa);
      }

      if (sp->write_real(info->destination_address, info->source_buffer, info->length))
      {
        LOG_DEBUG("write_stealth:  write success.");
        result = STATUS_SUCCESS;

        auto pid = info->process_id;

        mp::ipi_call([&pid]()
          {
            ia32_asm_vmx_vmcall((int)hvpp_comms::hypercall_codes::START_SPLIT_ALL_SHADOW_PAGES_FOR_PROC, pid, 0, 0);
          });
      }
      else
      {
        LOG_INFO("write_stealth:  write failure.");
        result = STATUS_UNSUCCESSFUL;
      }
    }
    else
    {
      LOG_WARN("write_stealth:  failed to find/create shadow page for dst.");
      result = STATUS_UNSUCCESSFUL;
    }
  }

  pContext->ReturnStatus = result;

  return NT_SUCCESS(result);
}

BOOLEAN Comms::Mmp::HandleAllocateVirtualMemory(sdk::comms::Context_t* pContext)
{
  _IMP_CACHED_NT(ZwAllocateVirtualMemory);
  _IMP_CACHED_NT(ZwFreeVirtualMemory);

  auto* const info = mem::addr(pContext->VRegs.vcx)
    .As<allocate_virtual_memory_t*>();

  if (!info || !info->length)
    return FALSE;

  if (!util::IsProcessValid(info->process_id))
    return FALSE;

  auto result = NTSTATUS{};
  auto address = PVOID{};
  auto length = SIZE_T{};
  auto protect = ULONG{};
  auto process_id = HANDLE{};

  if (info->free == false)
  {
    address = nullptr;
    length = info->length;
    protect = info->protect;
    process_id = (HANDLE)info->process_id;

    {
      auto __guard = k::c_process_attach((_handle)info->process_id);

      result = _IMP_C(ZwAllocateVirtualMemory)(
        ZwCurrentProcess(), &address,
        NULL, &length, MEM_COMMIT | MEM_RESERVE,
        protect
      );
    }
  }
  else
  {
    if (!info->address)
      return FALSE;

    address = mem::addr(info->address).Ptr();
    length = info->length;

    auto null_length = size_t{};

    {
      auto __guard = k::c_process_attach((_handle)info->process_id);

      result = _IMP_C(ZwFreeVirtualMemory)(
        ZwCurrentProcess(), &address,
        &null_length, MEM_RELEASE
      );
    }
  }

  pContext->ReturnStatus = result;

  if (!NT_SUCCESS(result))
  {
    LOG_INFO("allocate_virtual_memory(%i) has failed with status code %x",
      info->free, result);

    return FALSE;
  }

  if (info->free == false)
    info->address = mem::addr(address).Base();

  return TRUE;
}

BOOLEAN Comms::Mmp::HandleProtectVirtualMemory(sdk::comms::Context_t* pContext)
{
  _IMP_CACHED_NT(ZwProtectVirtualMemory);

  auto* const info = mem::addr(pContext->VRegs.vcx)
    .As<protect_virtual_memory_t*>();

  if (!info || !info->address || !info->length || !info->protect_new)
    return FALSE;

  if (!util::IsProcessValid(info->process_id))
    return FALSE;

  auto result = NTSTATUS{};
  auto address = mem::addr(info->address).Ptr();
  auto length = info->length;
  auto protect_new = info->protect_new;
  auto protect_old = u_long{};

  __try
  {
    auto __guard = k::c_process_attach((_handle)info->process_id);

    result = _IMP_C(ZwProtectVirtualMemory)(
      ZwCurrentProcess(), &address, &length,
      protect_new, &protect_old
    );
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    return FALSE;
  }

  info->protect_old = protect_old;
  pContext->ReturnStatus = result;

  return NT_SUCCESS(result);
}

BOOLEAN Comms::Mmp::HandleReadPhysicalMemory(sdk::comms::Context_t* pContext)
{
  auto* const info = mem::addr(pContext->VRegs.vcx)
    .As<read_physical_memory_t*>();

  if (!info || !info->length ||
    !info->source_address || !info->destination_buffer)
    return FALSE;

  auto process = PEPROCESS{};

  if(!util::LookupProcess((HANDLE)info->process_id, &process))
    return FALSE;

  k::c_pool_buffer __interm_buffer(NonPagedPool, info->length);
  k::c_object_guard __process_guard(process, FALSE);

  if (!__interm_buffer.valid())
    return FALSE;

  size_t bytes_read = 0;
  const auto result =
    phys_mem::read_process_memory(info->process_id, (void*)info->source_address,
      __interm_buffer.ptr(), info->length, &bytes_read);

  if (NT_SUCCESS(result))
  {
    memcpy(info->destination_buffer,
      __interm_buffer.ptr(), bytes_read);

    return TRUE;
  }

  return FALSE;
}

BOOLEAN Comms::Mmp::HandleWritePhysicalMemory(sdk::comms::Context_t* pContext)
{
  auto* const info = mem::addr(pContext->VRegs.vcx)
    .As<write_physical_memory_t*>();

  if (!info || !info->length ||
    !info->destination_address || !info->source_buffer)
    return FALSE;

  auto process = PEPROCESS{};
  auto bytes_written = SIZE_T{};

  if (!util::LookupProcess((HANDLE)info->process_id, &process))
    return FALSE;

  k::c_pool_buffer __interm_buffer(NonPagedPool, info->length);
  k::c_object_guard __process_guard(process, FALSE);

  if (!__interm_buffer.valid())
    return FALSE;

  memcpy(__interm_buffer.ptr(), info->source_buffer, info->length);

  const auto result =
    phys_mem::write_process_memory(info->process_id, (void*)info->destination_address,
      __interm_buffer.ptr(), info->length, &bytes_written);

  return NT_SUCCESS(result);
}

BOOLEAN Comms::Mmp::HandleMapImage(sdk::comms::Context_t* pContext)
{
  auto* const info = mem::addr(pContext->VRegs.vcx)
    .As<mapper::params_t*>();

  if (!mapper::map_image(*info))
    return FALSE;

  return TRUE;
}
