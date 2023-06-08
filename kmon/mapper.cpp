#include <include.h>

void apc_kernel_routine(_In_ PKAPC Apc,
  _Inout_ k::PKNORMAL_ROUTINE* NormalRoutine,
  _Inout_ PVOID* NormalContext,
  _Inout_ PVOID* SystemArgument1,
  _Inout_ PVOID* SystemArgument2)
{
  LOG_DEBUG("kernel routine was called!");

  __noop();
}

BOOL FixThreadInitPointer(HANDLE procId)
{
  BOOL result = FALSE;

  k::module_info remote_kernel32{ }, remote_ntdll{ };

  if (util::GetProcessModuleInfo(procId, _XSW(L"kernel32.dll"), remote_kernel32, TRUE) &&
    util::GetProcessModuleInfo(procId, _XSW(L"ntdll.dll"), remote_ntdll, TRUE))
  {
    if (true)
    {
      k::c_process_attach _(procId);

      auto thread_init_thunk = util::FindSystemModuleExport<void*>(PTR_OF(remote_kernel32.base), _XS("BaseThreadInitThunk"));
      auto thread_user_start = util::FindSystemModuleExport<void*>(PTR_OF(remote_ntdll.base), _XS("RtlUserThreadStart"));

      if (thread_init_thunk && thread_user_start)
      {
        auto thread_init_ptr = reinterpret_cast<void**>(RESOLVE_ADDRESS_TO_RELATIVE(BASE_OF(thread_user_start) + 0x7, 0x3, 0x7));

        if (thread_init_ptr)
        {
          if (*thread_init_ptr != thread_init_thunk)
          {
            LOG_WARN("!! modified thread init detected.");
            *thread_init_ptr = thread_init_thunk;
          }

          result = TRUE;
        }
      }
    }
  }

  return result;
}

namespace util
{
  auto find_alertable_thread_id(_handle process_id)
  {
    auto result = _handle{ };
    auto thread_list = util::GetProcessThreads(process_id);

    for (const auto& entry : thread_list)
    {
      auto id = entry.ClientId.UniqueThread;

      if (!id)
        continue;

      k::_KTHREAD* object = nullptr;

      if (util::LookupThread(id, (PETHREAD*)&object))
      {
        k::c_object_guard _(object, FALSE);

        if (!iw::is_thread_terminating((PETHREAD)object) &&
          object->SuspendCount == 0 &&
          object->Alertable)
        {
          result = id;
          break;
        }
      }
    }

    return result;
  }

  auto queue_user_apc(
    PETHREAD thread,
    void* kernel_routine,
    void* normal_routine,
    KPROCESSOR_MODE mode,
    void* normal_context,
    void* system_argument1,
    void* system_argument2
  )
  {
    _IMP_CACHED_NT(KeInitializeApc);
    _IMP_CACHED_NT(KeInsertQueueApc);

    auto apc = (PKAPC)util::MemAlloc(sizeof(KAPC));

    if (apc == nullptr)
      return false;

    _IMP_C(KeInitializeApc)(
      apc, thread,
      k::OriginalApcEnvironment,
      (k::PKKERNEL_ROUTINE)kernel_routine,
      nullptr,
      (k::PKNORMAL_ROUTINE)normal_routine,
      mode,
      normal_context
    );

    auto result = _IMP_C(KeInsertQueueApc)(
      apc, system_argument1,
      system_argument2, 0
    );

    if (!result)
    {
      util::MemFree(apc);
      return false;
    }

    return true;
  }

  bool execute_shell_code_um(_handle process_id, PETHREAD thread_obj, uint8_t* sc_buffer, size_t sc_length)
  {
    if (!process_id || !thread_obj)
      return false;

    if (!sc_buffer || !sc_length)
      return false;

    void* stub_memory = nullptr;

    if (!util::AllocVirtualMemory(process_id, &stub_memory, sc_length, PAGE_EXECUTE_READWRITE))
      return false;

    //
    // Zero the allocated buffer.
    //

    if (util::FillVirtualMemoryEx(process_id, BASE_OF(stub_memory), 0x00, sc_length))
    {
      //
      // Copy over the shell code.
      //

      if (util::WriteVirtualMemoryEx(process_id, BASE_OF(stub_memory), sc_buffer, sc_length))
      {
        //
        // Execute it.
        //

        if (util::queue_user_apc(
          thread_obj, &apc_kernel_routine,
          stub_memory, UserMode,
          nullptr, nullptr, nullptr
        ))
          return true;
        else
          NT_VERIFY(util::FreeVirtualMemory(process_id, &stub_memory, sc_length));
      } else
        NT_VERIFY(util::FreeVirtualMemory(process_id, &stub_memory, sc_length));
    }
    else
      NT_VERIFY(util::FreeVirtualMemory(process_id, &stub_memory, sc_length));

    return false;
  }
}

//
// INITIATES THE MAPPER CONTEXT AND COORDINATES THE OTHER FUNCTIONS.
// ALSO RETURNS THE RESULT OF THE WHOLE MAPPING PROCEDURE, OF COURSE.
//
bool mapper::map_image(const params_t& params)
{
  if (!params.process_id || !params.image_data || !params.image_length)
    return false;

  if (!util::IsProcessValid((size_t)params.process_id) || !util::IsMemoryValid(params.image_data))
    return false;

  //
  //  ************************************************************************
  // 
  //    local_base        => BASE OF LOCAL IMAGE COPY.
  //    local_buffer      => BASE OF LOCAL IMAGE BUFFER FOR RECONSTRUCTION.
  //    remote_base       => BASE OF LOCAL IMAGE DESTINATION (REMOTE).
  // 
  // *************************************************************************
  //
  auto ctx = context_t{};

  if (!pe::GetHeaders(
    params.image_data,
    &ctx.m_headers.dh,
    &ctx.m_headers.nth
  ))
    return false;

  ctx.params = params;
  ctx.m_target.process_id = params.process_id;

  ctx.exec_type = params.exec_type;

  ctx.local_image = params.image_data;
  ctx.local_length = params.image_length;

  ctx.image_length = ctx.m_headers.nth->OptionalHeader.SizeOfImage;
  ctx.header_length = ctx.m_headers.nth->OptionalHeader.SizeOfHeaders;

  if (*params.remote_base != 0)
  {
    LOG_INFO("using caller specified remote base at %llx", *params.remote_base);

    ctx.remote_base = *params.remote_base;
  }
  else
  {
    LOG_INFO("wish remote base has not been specified, allocating remote memory myself.");

    if (!util::AllocVirtualMemory(
      params.process_id,
      (PVOID*)&ctx.remote_base,
      ctx.image_length,
      PAGE_EXECUTE_READWRITE
    ))
    {
      LOG_WARN("failed to allocate remote memory for the image.");
      return false;
    }

    *params.remote_base = ctx.remote_base;

    LOG_INFO("remote memory has been allocated for the image at %llx", ctx.remote_base);
  }

  LOG_INFO("mapping an image now! (%p, %llx)", ctx.local_image, ctx.image_length);

  if ((ctx.local_buffer = util::MemAlloc(ctx.image_length)) == nullptr)
  {
    LOG_WARN("failed to allocate local buffer.");
    return false;
  }

  RtlZeroMemory(ctx.local_buffer, ctx.image_length);

  if (!prepare_image_local(ctx))
  {
    LOG_WARN("failed to prepare image locally.");
    return false;
  }

  if (!prepare_image_remote(ctx))
  {
    LOG_WARN("failed to prepare image remotely.");
    return false;
  }

  const auto entry_point_rva = ctx.m_headers.nth->OptionalHeader.AddressOfEntryPoint;

  if (!entry_point_rva)
  {
    LOG_WARN("image has no entry point.");
    return false;
  }

  LOG_INFO("image's entry_point RVA = %p", entry_point_rva);

  const auto entry_point_remote_address = mem::addr(ctx.remote_base).Add(entry_point_rva).Base();

  *params.remote_entry_point = entry_point_remote_address;
  ctx.remote_entry_point = entry_point_remote_address;

  LOG_INFO("image has been mapped! (remote_ep=%p)", (void*)entry_point_remote_address);

  if (!execute_image(ctx))
  {
    LOG_WARN("failed to execute image remotely.");
    return false;
  }

  map_operation_t mo{ };

  mo.base = ctx.remote_base;
  mo.length = ctx.image_length;

  _log.insert(mo);

  return true;
}

//
// RECONSTRUCTS THE IMAGE IN A WAY IT CAN BE RAN REMOTELY.
// THEN IT SAVES THAT INTO A LOCAL BUFFER.
//
bool mapper::prepare_image_local(const context_t& ctx)
{
  const auto dos_header = ctx.m_headers.dh;
  const auto nt_header = ctx.m_headers.nth;
  const auto local_buffer = ctx.local_buffer;

  const auto local_image = ctx.local_image;
  const auto remote_base = ctx.remote_base;
  const auto section_hdr = IMAGE_FIRST_SECTION(nt_header);

  memcpy(local_buffer, local_image, ctx.header_length);

  for (size_t i = 0; i < nt_header->FileHeader.NumberOfSections; i++)
  {
    const auto& section = section_hdr[i];

    auto* const source = mem::addr(local_image)
      .Add(section.PointerToRawData)
      .Ptr();

    auto* const destination = mem::addr(local_buffer)
      .Add(section.VirtualAddress)
      .Ptr();
    
    memcpy(destination, source, section.SizeOfRawData);
  }

  if (!resolve_relocations(ctx))
    return false;

  if (!resolve_imports(ctx))
    return false;

  return true;
}

//
// MAPS THE RECONSTRUCTED IMAGE INTO THE REMOTE PROCESS
// AND HANDLES A BUNCH OF OTHER THINGS RELATED TO IT.
//
bool mapper::prepare_image_remote(const context_t& ctx)
{
  const auto local_buffer = ctx.local_buffer;
  const auto remote_base = ctx.remote_base;
  const auto image_length = ctx.image_length;

  //
  // encrypt the PE header of the futurely mapped PE image.
  // do this right BEFORE mapping it into the target, so we have time to process it.
  //

  uint64_t header_encryption_key = 0xDE9644521;

  for (size_t i = 0; i < ctx.header_length; i++)
    reinterpret_cast<uint8_t*>(local_buffer)[i] ^= header_encryption_key;

  if (!util::WriteVirtualMemoryEx(
    ctx.m_target.process_id,
    remote_base,
    local_buffer,
    image_length
  ))
    return false;

  return true;
}

//
// EXECUTES THE REMOTELY MAPPED IMAGE.
// NOTHING MORE, NOTHING LESS.
//
bool mapper::execute_image(const context_t& ctx)
{
  using et = execution_type;

  if (ctx.exec_type == et::none)
  {
    //
    // no execution method was specified, do nothing.
    //
    return true;
  }

  switch (ctx.exec_type)
  {
  case et::iat_swap:
  {
    const auto process_base = util::GetProcessBase(ctx.m_target.process_id);

    if (!process_base)
    {
      LOG_WARN("iat_swap ->  failed to get process base.");
      return false;
    }

    _pimage_dos_header process_dh = nullptr;
    _pimage_nt_header process_nth = nullptr;

    bool iat_entry_found = false;

    const auto iat_swap_data = ctx.params.ts.et.iat_swap;

    uintptr_t original_address_reference = 0;
    uintptr_t* address_reference_pointer = nullptr;

    {
      k::c_process_attach _(ctx.m_target.process_id);

      if (pe::GetHeaders((PVOID)process_base, &process_dh, &process_nth))
      {
        const auto process_imports = pe::get_import_list(process_base, process_nth, TRUE);

        if (process_imports.empty())
        {
          LOG_WARN("iat_swap ->  failed to get process imports.");
          return false;
        }

        for (auto mod_i = 0ull; mod_i < process_imports.size(); mod_i++)
        {
          const auto& mod = process_imports[mod_i];

          if (!strcmp(mod.m_module_name, iat_swap_data.target_module))
          {
            for (auto imp_i = 0ull; imp_i < mod.m_data.size(); imp_i++)
            {
              const auto& imp = mod.m_data[imp_i];

              if (!strcmp(imp.m_name, iat_swap_data.target_name))
              {
                iat_entry_found = true;

                original_address_reference = *imp.m_address_reference;
                address_reference_pointer = imp.m_address_reference;

                break;
              }
            }
          }
        }
      }
      else
      {
        LOG_WARN("iat_swap ->  failed to get process PE headers.");
        return false;
      }
    }

    if (iat_entry_found == false)
    {
      LOG_WARN(
        "iat_swap ->  failed to find the specified entry '%s':'%s'.",
        iat_swap_data.target_module, iat_swap_data.target_name
      );

      return false;
    }

    if (!util::WriteVirtualValueEx(
      ctx.m_target.process_id,
      mem::addr(address_reference_pointer).Base(),
      ctx.remote_entry_point, TRUE
    ))
    {
      LOG_WARN("iat_swap ->  failed to overwrite the address reference.");
      return false;
    }

    util::DelayExecutionProcessor(2);

    if (!util::WriteVirtualValueEx(
      ctx.m_target.process_id,
      mem::addr(address_reference_pointer).Base(),
      original_address_reference, TRUE
    ))
    {
      LOG_WARN("iat_swap ->  failed to restore the address reference.");
      return false;
    }

    LOG_INFO("iat_swap ->  found and swapped the entry.");

    break;
  }
  case et::vmt_swap:
    //
    // TODO: implement.
    //
    return false;
  case et::apc:
  {
    auto thread_id = util::find_alertable_thread_id(ctx.m_target.process_id);
    auto thread_obj = PETHREAD{ };

    if (!thread_id)
    {
      LOG_INFO("failed to find a viable thread.");
      return false;
    }

    LOG_INFO("found a viable thread => %i.", thread_id);

    if (util::LookupThread(thread_id, &thread_obj))
    {
      k::c_object_guard _(thread_obj, FALSE);

      struct sc_t
      {
        uint8_t instr_move[2];
        uint8_t dest_move[8];
        uint8_t instr_call[3];
        uint8_t instr_nop[6];
        uint8_t instr_ret;
      };

      sc_t shell_code{ };
      memset(&shell_code, 0, sizeof(shell_code));

      //
      // MOV R15, <ENTRY_POINT>
      //
      shell_code.instr_move[0] = 0x49;
      shell_code.instr_move[1] = 0xBF;

      memcpy(shell_code.dest_move, &ctx.remote_entry_point, sizeof(uintptr_t));

      //
      // CALL R15
      //
      shell_code.instr_call[0] = 0x41;
      shell_code.instr_call[1] = 0xFF;
      shell_code.instr_call[2] = 0xD7;

      //
      // NO-OP ALIGN SPACE
      //
      memset(shell_code.instr_nop, 0x90, sizeof(shell_code.instr_nop));

      //
      // RETURN
      //
      shell_code.instr_ret = 0xC3;

      if (!util::execute_shell_code_um(
        ctx.m_target.process_id, thread_obj,
        (uint8_t*)&shell_code, sizeof(shell_code))
        )
      {
        LOG_WARN("failed to execute UM shellcode!");
        return false;
      }
    }

    break;
  }
  case et::user_thread:
  {
    HANDLE thread_handle{ };
    CLIENT_ID thread_cid{ };

    if (!FixThreadInitPointer(ctx.m_target.process_id))
    {
      LOG_WARN("failed to check thread init pointer.");
      return false;
    }

    if (!util::ExecuteCodeInProcess(ctx.m_target.process_id, PTR_OF(ctx.remote_entry_point), nullptr, &thread_handle, &thread_cid))
    {
      return false;
    }

    break;
  }
  default:
    break;
  }

  LOG_INFO("successfully injected this image.");

  return true;
}

//
// RESOLVES RELOCATIONS WITH THE REMOTE BASE ADDRESS IN MIND.
//
bool mapper::resolve_relocations(const context_t& ctx)
{
  const auto nth = ctx.m_headers.nth;
  const auto reloc_list = pe::get_relocation_list(
    mem::addr(ctx.local_buffer).Base(), nth
  );

  if (reloc_list.empty() == false)
  {
    const auto image_delta = mem::addr(ctx.remote_base)
      .Sub(static_cast<uintptr_t>(nth->OptionalHeader.ImageBase))
      .Base();

    LOG_INFO("remote base for relocation is %llx.", ctx.remote_base);

    for (auto i = 0ull; i < reloc_list.size(); i++)
    {
      const auto& reloc = reloc_list[i];

      *reinterpret_cast<uintptr_t*>(reloc.m_address) += image_delta;
    }
  }
  else
    LOG_WARN("image has no relocations.");

  return true;
}

//
// i know that we currently lack the handling of forward imports, but THIS has nothing to do with that.
// the checksums listed here are names of functions that produce a #DEP violation when called since they're
// actually located in NTDLL.dll and i haven't made enough research yet to automatically resolve them.
// i am fully aware that this is pretty ghetto, but CRT can be a fucking bitch and honestly i don't give a damn fuck.
//
bool try_resolve_unfound_ntapi(const char* name, uintptr_t nt, uintptr_t mod, uintptr_t& result)
{
  u_long hash = util::CalculateHash( (void*)name, strlen(name) );

  auto resolved = true;
  char actual_import_name[128];

  RtlZeroMemory(actual_import_name, sizeof(actual_import_name));

  const char* actual_import_cst = nullptr;

  switch (hash)
  {
  case 0x5edb1d72:
    actual_import_cst = _XS("RtlAllocateHeap");
    break;
  case 0x5889a8fe:
    actual_import_cst = _XS("RtlReAllocateHeap");
    break;
  case 0x2f01ae99:
    actual_import_cst = _XS("RtlEnterCriticalSection");
    break;
  case 0xa3589103:
    actual_import_cst = _XS("RtlDeleteCriticalSection");
    break;
  case 0xe806e1ac:
    actual_import_cst = _XS("RtlLeaveCriticalSection");
    break;
  case 0x0d6c9e5b:
    actual_import_cst = _XS("RtlInitializeSListHead");
    break;
  case 0x94e8eba0:
    actual_import_cst = _XS("RtlInterlockedFlushSList");
    break;
  case 0x7e3e84d5:
    actual_import_cst = _XS("RtlEncodePointer");
    break;
  case 0x0a180d0b:
    actual_import_cst = _XS("RtlSizeHeap");
    break;
  default:
    resolved = false;
    break;
  }

  if (resolved == true && actual_import_cst != nullptr)
  {
    sprintf_s(actual_import_name, actual_import_cst);

    result = pe::FindExport( (void*)nt, actual_import_name );

    if (result == NULL)
    {
      LOG_WARN("unfound ntapi import was resolved, but could not be grabbed => '%s'.", actual_import_name);
      return false;
    }
  }
  else
    LOG_WARN("unfound ntapi import could not be resolved.");

  return resolved;
}

//
// RESOLVES IMPORTS WITH THE REMOTE MODULES IN MIND.
//
bool mapper::resolve_imports(const context_t& ctx)
{
  const auto nth = ctx.m_headers.nth;
  const auto import_list = pe::get_import_list(
    mem::addr(ctx.local_buffer).Base(), nth
  );

  auto remote_kernel = k::module_info{};
  auto remote_ntdll = k::module_info{};

  if (!util::GetProcessModuleInfoWait(ctx.m_target.process_id, _XSW(L"kernel32.dll"), remote_ntdll, TRUE))
  {
    LOG_WARN("failed to get KERNEL32 module info.");
    return false;
  }

  if (!util::GetProcessModuleInfoWait(ctx.m_target.process_id, _XSW(L"ntdll.dll"), remote_ntdll, TRUE))
  {
    LOG_WARN("failed to get NTDLL module info.");
    return false;
  }

  if (import_list.empty() == false)
  {
    k::c_process_attach _(ctx.m_target.process_id);

    for (auto i = 0ull; i < import_list.size(); i++)
    {
      auto mod = import_list[i];

      wchar_t     mod_name_wide[MAX_PATH];
      auto        mod_info = k::module_info{};

      RtlZeroMemory(mod_name_wide, sizeof(mod_name_wide));

      if (!util::MultibyteToUnicodeStringS(mod_name_wide, mod.m_module_name, sizeof(mod_name_wide)))
      {
        LOG_WARN("failed to convert module name '%s' to wide.", mod.m_module_name);
        continue;
      }

      str::UnicodeLower(mod_name_wide, sizeof(mod_name_wide));

      if (!util::GetProcessModuleInfo(ctx.m_target.process_id, mod_name_wide, mod_info, TRUE))
      {
        LOG_WARN("failed to get module info for '%ls'.", mod_name_wide);
        continue;
      }

      for (auto imp_i = 0; imp_i < mod.m_data.size(); imp_i++)
      {
        const auto& imp = mod.m_data[imp_i];

        if (imp.m_type == 2)
        {
          auto imp_addr = util::FindSystemModuleExport<uintptr_t>((PVOID)mod_info.base, imp.m_name);

          if (!imp_addr && !try_resolve_unfound_ntapi(imp.m_name, remote_ntdll.base, mod_info.base, imp_addr))
            LOG_WARN("failed to resolve import '%s'", imp.m_name);

          *imp.m_address_reference = imp_addr;
        }
      }
    }
  }
  else
    LOG_WARN("image has no imports.");

  return true;
}
