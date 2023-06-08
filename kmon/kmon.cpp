#include <include.h>

using namespace kmon;

namespace kmon
{
  bool enable_hooks = true;
}

void nr_load_image(PUNICODE_STRING image_name, HANDLE proc_id, PIMAGE_INFO image_info)
{

}

status_code c_kmon::initialize()
{
  LOG_CALL();

  if (!NT_SUCCESS(PsSetLoadImageNotifyRoutine(nr_load_image)))
    return status_code::failed_notify_routines_init;

  if (_running)
    return status_code::success;

  //
  // Setup info table defaults.
  //
  INFO_TABLE_WRITE(hvpp::comms::table_generic, hvpp::comms::t::log_calls, TRUE);
  INFO_TABLE_WRITE(hvpp::comms::table_generic, hvpp::comms::t::log_calls_verbose, FALSE);
  INFO_TABLE_WRITE(hvpp::comms::table_generic, hvpp::comms::t::log_level_ept, hvpp::EPT_LOG_LEVEL_NONE);

  //
  // also initialize their cached counterparts.
  //
  g::it::initialize();
  g::system_modules.initialize();

  if (!setup_diag_info())
    return status_code::failed_diag_info_init;

  if (!setup_dyn_addrs())
    return status_code::failed_dyn_addr_init;

  if (!io_regs::call_driver.initialize())
    return status_code::failed_ioregs_init;

  if (!hooks::Initialize())
    return status_code::failed_hooks_init;

  if (!tests::run())
    return status_code::failed_tests;

  //
  // Enable our wanted modules here.
  //

  _running = true;

  return status_code::success;
}

void c_kmon::ci_spoof_registry()
{
  reg::set_sz(
    _XSW(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control"),
    _XSW(L"SystemStartOptions"),
    _XSW(L" NOEXECUTE=OPTIN  FVEBOOT=2670592  NOVGA")
  );

  auto bcd_list = reg::get_all_sub_keys(L"\\Registry\\Machine\\BCD00000000\\Objects");

  for (const auto& bcd_entry : bcd_list)
  {
    wchar_t key_fmt[256];
    RtlZeroMemory(key_fmt, sizeof(key_fmt));

    StringCchPrintfW(
      key_fmt,
      sizeof(key_fmt) / sizeof(wchar_t),
      _XSW(L"\\Registry\\Machine\\BCD00000000\\Objects\\%ls\\Elements\\16000049"),
      bcd_entry.data()
    );

    reg::set_dword(key_fmt, _XSW(L"Element"), 0);
  }
}

void c_kmon::shutdown()
{
  LOG_CALL();

  if (!NT_SUCCESS(PsRemoveLoadImageNotifyRoutine(nr_load_image)))
    return;

  if (!_running)
    return;

  if (g::diagnose_info)
  {
    memset(g::diagnose_info, 0, g::diagnose_info_length);
    util::MemFree(g::diagnose_info);

    g::diagnose_info = nullptr;
  }

  g::system_modules.shutdown();

  _running = false;
}

bool c_kmon::setup_dyn_addrs()
{
  if (!dyn_addr::initialize())
    return false;

  g::driver_phys_addr = pa_t::from_va(driver::image_base).value();

  if (!g::driver_phys_addr)
    return false;

  LOG_INFO("physical address of the HV is %p",
    g::driver_phys_addr);

  return true;
}

bool c_kmon::setup_diag_info()
{
  struct diagnose_info_t
  {
    uint32_t m_magic{}; // magic number to validate structure format.

    uintptr_t m_base{}; // the base address of the kernel-driver image.
    uintptr_t m_length{}; // the length of the kernel-driver image.
  };

  const auto length = sizeof(diagnose_info_t);
  auto* const buffer = util::MemAlloc(length);

  if (buffer == nullptr)
    return false;

  memset(buffer, 0, length);

  auto* const info =
    mem::addr(buffer)
    .As<diagnose_info_t*>();

  info->m_magic = 'HsKr';

  info->m_base = mem::addr(driver::image_base).Base();
  info->m_length = driver::image_length;

  g::diagnose_info = info;
  g::diagnose_info_length = sizeof(diagnose_info_t);

  return true;
}

void c_kmon::think()
{

}

uint64_t c_kmon::info_table_read(hvpp::comms::tables table, size_t entry)
{
  auto result = uint64_t{};

  if (!g::info_table_mgr.table_read_value(table, entry, &result))
    return 0;

  return result;
}

void c_kmon::info_table_write(hvpp::comms::tables table, size_t entry, uint64_t value)
{
  if (!g::info_table_mgr.table_write_value(table, entry, value))
  {
    LOG_WARN("failed to write to info table with id %llx and target entry %zu.", table, entry);
  }
}
