#include <include.h>

BOOL HsLaunchKMON( VOID )
{
  g::status.kmon.launch_request_received = true;

  kmon::status_code kmon_status = g::kmon->initialize();

  driver::kmon_status_code = static_cast<int>(kmon_status);
  g::status.kmon.status_code = static_cast<int>(kmon_status);

  if (kmon_status != kmon::status_code::success)
  {
    LOG_WARN("enable_kmon -> failed with status code %i", kmon_status);
    return FALSE;
  }

  LOG_WARN("enable_kmon -> succeeded with ok status code.");

  LOG_INFO_SAFE("nt_base= %p", g::ntoskrnl.Base);
  LOG_INFO_SAFE("nt_length= L%llx", g::ntoskrnl.Length);

  return TRUE;
}

NTSTATUS MmCreateDriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
  //
  // Gets invoked when driver object had to be created manually by us.
  //

  //
  // It's only duty is to assign it globally, since there seems to be no way to get it otherwise.
  //

  if (!DriverObject)
    return STATUS_FAILED_DRIVER_ENTRY;

  driver::driver_object = DriverObject;

  return STATUS_SUCCESS;
}

NTSTATUS c_core::on_regular_entry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
  HANDLE init_thread{ };

  if (auto err = mm::initialize())
    return STATUS_FAILED_DRIVER_ENTRY;

  if (auto err = driver::common::system_allocator_default_initialize())
    return STATUS_FAILED_DRIVER_ENTRY;

  mm::allocator_guard _(mm::system_allocator());

  driver::get_address_ranges();

  if (!driver::get_self_module_info() || !driver::get_kernel_main_info())
    return STATUS_FAILED_DRIVER_ENTRY;

  if (!imports::initialize())
    return STATUS_FAILED_DRIVER_ENTRY;

  if (!(g::kmon = stl::mem::allocate_type<kmon::c_kmon>()))
    return STATUS_FAILED_DRIVER_ENTRY;

  DriverObject->DriverUnload = DriverUnload;

  DriverObject->MajorFunction[IRP_MJ_CREATE] = &DriverDispatch;
  DriverObject->MajorFunction[IRP_MJ_CLOSE] = &DriverDispatch;
  DriverObject->MajorFunction[IRP_MJ_READ] = &DriverDispatch;
  DriverObject->MajorFunction[IRP_MJ_WRITE] = &DriverDispatch;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = &DriverDispatch;

  driver::driver_object = DriverObject;

  if ((init_thread = util::CreateThreadEx(&ThreadEntry)) == nullptr)
    return STATUS_FAILED_DRIVER_ENTRY;

  if (!util::WaitForThread(init_thread))
    return STATUS_FAILED_DRIVER_ENTRY;

  return STATUS_SUCCESS;
}

void c_core::on_thread_entry(void* param)
{
  auto err = driver::common::initialize(&driver::initialize, &driver::destroy);

  if (err)
  {
    _dbg(
      "fatal initialization failure was encountered (CODE=%i, KMON_STATUS=%i)",
      err.value(), driver::kmon_status_code
    );

    driver::destroy();

    BUG_CHECK_EX(HYPERVISOR_ERROR, err.value(), driver::kmon_status_code, 0, 0);
    /* END */
  }

  if (!HsLaunchKMON())
  {
    BUG_CHECK(hvpp_ecode::ECODE_KMON_NOT_LAUNCHED);
  }
}

void c_core::on_unload(PDRIVER_OBJECT DriverObject)
{
  if (g::kmon && g::kmon->is_running())
    g::kmon->shutdown();

  driver::common::destroy();
}
