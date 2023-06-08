#include <include.h>

namespace driver
{
  constexpr bool enable_kmon = true;

  //
  // Create combined handler from these VM-exit handlers.
  //
  using vmexit_handler_t = vmexit_compositor_handler<
    vmexit_custom_handler
    >;

  static_assert(std::is_base_of_v<vmexit_handler, vmexit_handler_t>);

  vmexit_handler_t* vmexit_handler_  = nullptr;
  device_custom* device_ = nullptr;

  auto save_startup_info() noexcept
  {
    char diagnose_fmt[256];

    memset(diagnose_fmt, 0, sizeof(diagnose_fmt));

    sprintf_s(diagnose_fmt,
      _XS(
        "=============================\n"
        "driver_base = %p\n"
        "driver_length = %llx\n"
        "back_log = %p\n"
        "cpu_count = %i\n"
        "launch_time = %012llu\n"
        "============================="
      ),
      driver::image_base, driver::image_length, g::back_log.get(),
      mp::cpu_count(), g::back_log.get_delta_time());

    return util::save_data_to_disk(_XSW(LR"(\SystemRoot\hshv.dat)"), diagnose_fmt, strlen(diagnose_fmt));
  }

  auto get_system_module(LPCSTR name, DWORD_PTR& out_base, UINT64& out_length)
  {
    auto info = k::SYSTEM_MODULE_ENTRY{};

    if (!util::GetKernelModuleInfo(name, info))
      return false;

    out_base = mem::addr(info.ImageBase).Base();
    out_length = info.ImageSize;

    return true;
  }

  auto get_kernel_other_modules()
  {
    if (!get_system_module(_XS("win32kfull.sys"),
      g::win32kfull.Base, g::win32kfull.Length))
      return false;

    if (!get_system_module(_XS("win32kbase.sys"),
      g::win32kbase.Base, g::win32kbase.Length))
      return false;

    if (!get_system_module(_XS("hal.dll"),
      g::hal.Base, g::hal.Length))
      return false;

    return true;
  }

  auto initialize() noexcept -> error_code_t
  {
    //
    // Initialize our HV class.
    //
    hv::inst = new hv::c_hypervisor();

    if (!hv::inst)
    {
      destroy();
      return make_error_code_t(std::errc::not_enough_memory);
    }

    if (!hv::inst->init())
    {
      destroy();
      return make_error_code_t(std::errc::no_such_file_or_directory);
    }

    //
    // Create device instance.
    //
    device_ = new device_custom();
    sh::mgr = new sh::c_shadow_page_mgr();

    if (!device_ || !sh::mgr)
    {
      destroy();
      return make_error_code_t(std::errc::not_enough_memory);
    }

    //
    // Initialize device instance.
    //
    if (auto err = device_->create())
    {
      destroy();
      return err;
    }

    //
    // Set up back log and it's resources.
    //
    if (!g::back_log.initialize(0x10457400))
      return make_error_code_t(std::errc::not_enough_memory);

    //
    // Save startup information in case of trouble shooting need.
    //
    if (!save_startup_info())
      return make_error_code_t(std::errc::no_such_file_or_directory);

    LOG_INFO("my_base     =  %p", driver::image_base);
    LOG_INFO("my_length   =  %llx", driver::image_length);
    LOG_INFO("back_log    =  %p", g::back_log.get());

    //
    // Get windows kernel's other modules.
    //
    if (!get_kernel_other_modules())
      return make_error_code_t(std::errc::no_such_file_or_directory);

    //
    // Create info tables.
    //
    if (!g::info_table_mgr.table_create(comms::table_generic) ||
      !g::info_table_mgr.table_create(comms::table_debug))
      return make_error_code_t(std::errc::not_enough_memory);

    //
    // Create VM-exit handler instance.
    //
    vmexit_handler_ = new vmexit_handler_t();

    if (!vmexit_handler_)
      return make_error_code_t(std::errc::not_enough_memory);

    //
    // Right before virtualizing the system, give wrappers a chance to modify host configuration directly.
    //
    if (!hv::inst->configure_host())
      return make_error_code_t(std::errc::protocol_error);

    //
    // Start the hypervisor.
    //
    if (auto err = hvpp::hypervisor::start(*vmexit_handler_))
      return err;

    //
    // Tell debugger we're started.
    //

    driver::initialized = true;

    return {};
  }

  void destroy() noexcept
  {
    //
    // Stop the kernel monitor.
    //
    if(enable_kmon && g::kmon && g::kmon->is_running())
      g::kmon->shutdown();

    //
    // Stop the hypervisor.
    //
    hvpp::hypervisor::stop();

    //
    // Destroy VM-exit handler.
    //
    if (vmexit_handler_)
    {
      delete vmexit_handler_;
    }

    //
    // Destroy device.
    //
    if (device_)
    {
      delete device_;
    }

    if (sh::mgr)
    {
      delete sh::mgr;
    }

    //
    // Destroy info tables.
    //
    g::info_table_mgr.shutdown();

    //
    // Release back log resources.
    //
    g::back_log.shutdown();

    hvpp_info("Hypervisor stopped");
  }
}
