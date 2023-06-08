#include <include.h>

NTSTATUS hv::sdk::invoke(uint64_t rcx, uint64_t rdx, void* r8, uint64_t r9, uint64_t a5)
{
  auto* const nt = GetModuleHandle("ntdll.dll");

  if (nt == nullptr)
    return 0xC0000001;

  using fn_type = NTSTATUS(*)(__int64 a1, void* a2);

  auto* const fn =
    reinterpret_cast<fn_type>(GetProcAddress(nt, "ZwShutdownSystem"));

  if (fn == nullptr)
    return 0xC0000001;

  return fn(rdx, reinterpret_cast<void*>(rcx));
}

bool hv::sdk::invoke_ex(hyper_comms::request_t& request, bool strict) noexcept
{
  __try
  {
    ia32_asm_vmx_vmcall(0xC9, mem::addr(&request).Base(), 0, 0);
  }
  __except (EXCEPTION_EXECUTE_HANDLER) { __noop(); }

  return strict ? (request.m_processed && request.m_completed) : request.m_processed;
}

bool hv::sdk::send_symbol_data(c_device& device, const char* image_name, ds::symbol_entry* se, size_t len)
{
  if (!se || len <= 0)
    return false;

  if (!device.valid())
    return false;

  ds::symbol_data_t data{ };

  memset(&data, 0, sizeof(data));
  strcpy_s(data.image_name, image_name);

  data.list = se;
  data.count = (len / sizeof(*se));
  data.length = len;

  u_long bytes_returned{ };

  auto result = DeviceIoControl(
    device.handle(), ioctl_send_symbol_data_t::code,
    &data, sizeof(data), 0, 0,
    &bytes_returned, 0
  );

  printf("send_symbol_data():  result => %x.\n", result);

  return NT_SUCCESS(result);
}

void hv::sdk::add_symbol_data(ds::symbol_entry* se, size_t* cnt, const char* name, uintptr_t rva)
{
  auto& entry = se[*cnt];

  memset(&entry, 0, sizeof(entry));

  entry.function_rva = rva;
  strcpy(entry.function_name, name);

  ++(*cnt);
}

size_t hv::sdk::query_info_length_ex(uint64_t id)
{
  hyper_comms::request_t request{ };

  request.m_id = 0x1;
  request.m_vregs.vcx = id;
  request.m_vregs.vdx = 0;
  request.m_vregs.v8 = 0;

  invoke_ex(request);

  return request.m_vregs.v8;
}

void* hv::sdk::query_info_ex(uint64_t id, size_t* length)
{
  void* buffer{ };

  if (!length)
    return nullptr;

  hyper_comms::request_t request{ };

  request.m_id = 0x1;
  request.m_vregs.vcx = id;
  request.m_vregs.vdx = 0;
  request.m_vregs.v8 = 0;

  invoke_ex(request);

  // must be processed, but can't be completed, since we're only querying the length using invalid fields.
  if (!request.m_processed || request.m_completed)
    return nullptr;

  *length = request.m_vregs.v8;

  if (*length <= 0)
    return nullptr;

  buffer = malloc(*length);

  if (buffer == nullptr)
    return nullptr;

  memset(buffer, 0, *length);

  // this time we enter real fields with the acquired information.
  request.m_vregs.vdx = mem::addr(buffer).Base();
  request.m_vregs.v8 = *length;

  invoke_ex(request);

  return buffer;
}

bool hv::sdk::trigger_vmxr_exception(bool safe)
{
  hyper_comms::request_t request{ };

  request.m_id = 0x2;
  request.m_vregs.vcx = safe;

  return invoke_ex(request);
}

bool hv::sdk::trigger_vmxr_hang()
{
  hyper_comms::request_t request{ };

  request.m_id = 0x3;

  return invoke_ex(request);
}

bool hv::sdk::trigger_debug_break()
{
  Context_t context{};

  context.Id = 0xD00;
  context.Data = nullptr;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::trigger_bug_check()
{
  Context_t context{};

  context.Id = 0xD02;
  context.Data = nullptr;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::trigger_pg_checks()
{
  Context_t context{};

  context.Id = 0xD68;
  context.Data = nullptr;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

uintptr_t hv::sdk::get_system_module_export(const std::string& module_name, const std::string& export_name)
{
  if (module_name.empty() || export_name.empty())
    return 0;

  Context_t context{};
  SystemModuleExportInfo_t info{};

  memset(&info, 0, sizeof(info));

  strcpy_s(info.ModuleName, module_name.c_str());
  strcpy_s(info.RoutineName, export_name.c_str());

  context.Id = 0xD66;
  context.Data = &info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  if (!NT_SUCCESS(result))
    return 0;

  return info.Result;
}

bool hv::sdk::info_table_read(tables table_id, size_t entry_id, uint64_t* out_result)
{
  Context_t context{};

  context.Id = 0xD58;
  context.Data = nullptr;

  context.VRegs.vcx = table_id;
  context.VRegs.vdx = entry_id;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  *out_result = context.VRegs.v8;

  return NT_SUCCESS(result);
}

bool hv::sdk::info_table_write(tables table_id, size_t entry_id, uint64_t value)
{
  Context_t context{};

  context.Id = 0xD60;
  context.Data = nullptr;

  context.VRegs.vcx = table_id;
  context.VRegs.vdx = entry_id;
  context.VRegs.v8 = value;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

size_t hv::sdk::get_back_log_length()
{
  Context_t context{};
  InfoQuery_t query{};

  SIZE_T return_length = 1;

  query.Id = HVI_BACK_LOG;
  query.Buffer = (void*)0x1;
  query.Length = &return_length;

  context.Id = 0xD08;
  context.Data = &query;

  //
  // perform an arbitrary call first in order
  // to determine the actual size of the data we want.
  //
  const auto arb_result = invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return return_length;
}

bool hv::sdk::query_info(uint64_t id, void** buffer, size_t* length)
{
  if (!id || !buffer)
    return false;

  Context_t context{};
  InfoQuery_t query{};

  SIZE_T return_length = 1;

  query.Id = id;
  query.Buffer = buffer;
  query.Length = &return_length;

  context.Id = 0xD08;
  context.Data = &query;

  //
  // perform an arbitrary call first in order
  // to determine the actual size of the data we want.
  //
  const auto arb_result = invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  if (!arb_result || query.Result != HVI_QUERY_RESULT_BUFFER_TOO_SMALL)
    return false;

  //
  // check if the data is empty.
  //
  if (return_length <= NULL)
    return true;

  return_length *= 4;

  auto* const actual_buffer = malloc(return_length);

  if (actual_buffer == nullptr)
    return false;

  memset(actual_buffer, 0, return_length);

  query.Length = &return_length;
  query.Buffer = actual_buffer;

  //
  // if that's not the case then request it normally with a large enough buffer now.
  //
  const auto result = invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  *buffer = actual_buffer;
  *length = return_length;

  return (NT_SUCCESS(result) && query.Result == HVI_QUERY_RESULT_SUCCESS);
}

bool hv::sdk::map_image(mmp::map_params_t& info)
{
  Context_t context{};

  context.Id = 0xD62;
  context.Data = nullptr;

  context.VRegs.vcx = (uint64_t)&info;

  const auto result = invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::clear_back_log()
{
  Context_t context{};

  context.Id = 0xD64;
  context.Data = nullptr;

  const auto result = invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::translate_virt_to_phys(uint64_t address, uint64_t* out)
{
  if (!address || !out)
    return false;

  Context_t context{};

  context.Id = 0xD18;
  context.Data = nullptr;

  context.VRegs.vcx = address;
  context.VRegs.vdx = (uint64_t)out;

  const auto result = invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::get_main_module_base(uint32_t pid, uint64_t* out)
{
  if (!pid || !out)
    return false;

  Context_t context{};

  context.Id = 0xD28;
  context.Data = nullptr;

  context.VRegs.vcx = pid;
  context.VRegs.vdx = (uint64_t)out;

  const auto result = invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::get_module_list(uint32_t pid, std::vector<module_info_ex>& buffer)
{
  if (!pid)
    return false;

  buffer.resize(1024);

  Context_t context{};

  context.Id = 0xD48;
  context.Data = nullptr;

  context.VRegs.vcx = pid;
  context.VRegs.vdx = buffer.size();
  context.VRegs.v8 = (uintptr_t)buffer.data();

  const auto result = invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  if (context.VRegs.v9 <= 0)
    return false;

  buffer.resize(context.VRegs.v9);

  return NT_SUCCESS(result);
}

bool hv::sdk::get_module_info(uint32_t pid, const std::wstring& name, mmp::module_info_t* out)
{
  if (!pid || !out)
    return false;

  char name_buffer[MAX_PATH];
  memset(name_buffer, 0, sizeof(name_buffer));
  memcpy(name_buffer, name.c_str(), name.size() * sizeof(wchar_t));

  Context_t context{};

  context.Id = 0xD40;
  context.Data = nullptr;

  context.VRegs.vcx = pid;
  context.VRegs.vdx = (uint64_t)name_buffer;
  context.VRegs.v8 = sizeof(name_buffer);
  context.VRegs.v9 = (uint64_t)out;

  const auto result = invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::query_virt_memory(uint32_t pid, uintptr_t address, size_t length, mmp::virtual_memory_info_t* buffer)
{
  if (!pid || !address || !length || !buffer)
    return false;

  Context_t context{};
  mmp::query_virtual_memory_t info{};

  info.process_id = pid;
  info.address = address;
  info.length = length;

  context.Id = 0xD50;
  context.Data = nullptr;

  context.VRegs.vcx = (uint64_t)&info;
  context.VRegs.vdx = (uint64_t)buffer;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::read_virt_memory(uint32_t pid, uint64_t src, void* dst, uint64_t length)
{
  if (!length || !src || !dst)
    return false;

  Context_t context{};
  mmp::read_virtual_memory_t info{};

  info.process_id = pid;
  info.source_address = src;
  info.destination_buffer = dst;
  info.length = length;

  context.Id = 0xD20;
  context.Data = nullptr;

  context.VRegs.vcx = (uint64_t)&info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return (NT_SUCCESS(result) || (u_long)context.ReturnStatus == 0x8000000d);
}

bool hv::sdk::write_virt_memory(uint32_t pid, uint64_t dst, void* src, uint64_t length, bool stealth)
{
  if (!length || !dst || !src)
    return false;

  Context_t context{};
  mmp::write_virtual_memory_t info{};

  info.process_id = pid;
  info.source_buffer = src;
  info.destination_address = dst;
  info.length = length;
  info.stealth = stealth;

  context.Id = 0xD22;
  context.Data = nullptr;

  context.VRegs.vcx = (uint64_t)&info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::alloc_virt_memory(uint32_t pid, size_t length, uintptr_t& address, u_long protect)
{
  if (!length)
    return false;

  Context_t context{};
  mmp::allocate_virtual_memory_t info{};

  info.process_id = pid;
  info.address = 0;
  info.length = length;
  info.free = false;
  info.protect = protect;

  context.Id = 0xD42;
  context.Data = nullptr;

  context.VRegs.vcx = (uint64_t)&info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  if (!NT_SUCCESS(result) || !info.address)
    return false;

  address = info.address;

  return true;
}

bool hv::sdk::free_virt_memory(uint32_t pid, uintptr_t address, size_t length)
{
  if (!length)
    return false;

  Context_t context{};
  mmp::allocate_virtual_memory_t info{};

  info.process_id = pid;
  info.address = address;
  info.length = length;
  info.free = true;

  context.Id = 0xD42;
  context.Data = nullptr;

  context.VRegs.vcx = (uint64_t)&info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  if (!NT_SUCCESS(result))
    return false;

  return true;
}

bool hv::sdk::protect_virt_memory(uint32_t pid,
  uintptr_t address, size_t length,
  u_long protect_new, u_long* protect_old)
{
  if (!length)
    return false;

  Context_t context{};
  mmp::protect_virtual_memory_t info{};

  info.process_id = pid;
  info.address = address;
  info.length = length;
  info.protect_new = protect_new;

  context.Id = 0xD44;
  context.Data = nullptr;

  context.VRegs.vcx = (uint64_t)&info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  if (!NT_SUCCESS(result))
    return false;

  if (protect_old != nullptr)
    *protect_old = info.protect_old;

  return true;
}

bool hv::sdk::read_phys_memory(uint32_t pid, uint64_t src, void* dst, uint64_t length)
{
  if (!pid || !length || !src || !dst)
    return false;

  Context_t context{};
  mmp::read_physical_memory_t info{};

  info.process_id = pid;
  info.length = length;
  info.source_address = src;
  info.destination_buffer = dst;

  context.Id = 0xD24;
  context.Data = nullptr;

  context.VRegs.vcx = (uint64_t)&info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::write_phys_memory(uint32_t pid, uint64_t dst, void* src, uint64_t length)
{
  if (!pid || !length || !dst || !src)
    return false;

  Context_t context{};
  mmp::write_physical_memory_t info{};

  info.process_id = pid;
  info.length = length;
  info.source_buffer = src;
  info.destination_address = dst;

  context.Id = 0xD26;
  context.Data = nullptr;

  context.VRegs.vcx = (uint64_t)&info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::read_virt_memory_ex(uint32_t pid, uint64_t src, void* dst, uint64_t length)
{
  if (!length || !src || !dst)
    return false;

  if (!read_phys_memory(pid, src, dst, length))
    return false;

  return true;
}

bool hv::sdk::write_virt_memory_ex(uint32_t pid, uint64_t dst, void* src, uint64_t length)
{
  if (!length || !src || !dst)
    return false;

  uint64_t phys_source{}, phys_dst{};

  auto* const interm_buffer =
    malloc(length);

  if (interm_buffer == nullptr)
    return false;

  memset(interm_buffer, 0, length);

  if (!write_phys_memory(pid, dst, src, length))
    return false;

  return true;
}

bool hv::sdk::get_system_module(const std::string& name, k::SYSTEM_MODULE_ENTRY* out_info)
{
  Context_t context{};

  context.Id = 0xD36;
  context.Data = nullptr;

  char buffer[256];

  if (name.size() >= sizeof(buffer))
    return false;

  memset(buffer, 0, sizeof(buffer));
  memcpy(buffer, name.data(), name.size());

  context.VRegs.vcx = (uint64_t)buffer;
  context.VRegs.vdx = (uint64_t)out_info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::read_msr(uint64_t address, uint64_t* buffer)
{
  Context_t context{};
  MsrAccess_t info{};

  info.Type = MsrAccess_t::MSR_ACCESS_READ;
  info.Address = address;
  info.Buffer = buffer;

  context.Id = 0xD38;
  context.Data = &info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::write_msr(uint64_t address, uint64_t value)
{
  Context_t context{};
  MsrAccess_t info{};

  info.Type = MsrAccess_t::MSR_ACCESS_WRITE;
  info.Address = address;
  info.Buffer = &value;

  context.Id = 0xD38;
  context.Data = &info;

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}

bool hv::sdk::add_data_shadow_watcher(uint64_t address, uint64_t length, bool remove)
{
  Context_t context{};

  context.Id = 0xF01;
  context.Data = nullptr;

  context.VRegs.vcx = address;
  context.VRegs.vdx = length;
  context.VRegs.v8 = (remove == true);

  const auto result =
    invoke((uint64_t)&context, Magic, nullptr, NULL, NULL);

  return NT_SUCCESS(result);
}
