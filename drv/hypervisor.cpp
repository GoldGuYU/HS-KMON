#include <include.h>

using namespace hv;

bool c_hypervisor::init()
{
  auto& hv_status = g::status;

  memset(&hv_status, 0, sizeof(hv_status));

  hv_status.major_version = 21;
  hv_status.entry_time.QuadPart = ia32_asm_read_tsc();

  hv_status.processor_count = mp::cpu_count();
  hv_status.processor_entry = mp::cpu_index();

  return true;
}

bool c_hypervisor::configure_host()
{
  auto failed = false;

  mp::ipi_call([this, &failed]()
    {
      auto result = configure_host_sp();

      if (!result)
      {
        _dbg("failed host configuration on this core.");
        failed = true;
      }
    });

  if (failed)
    return false;

  return true;
}

bool c_hypervisor::configure_host_sp()
{
  const auto disable_tsx = []() -> bool
  {
    //
    // check if tsx is supported in first place.
    //
    msr::arch_capabilities_t arch_cpb{};
    __try
    {
      // may #GP on unsupported systems.
      arch_cpb.flags = ia32_asm_read_msr(arch_cpb.msr_id);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      // if TSX not supported, we good to go.
      return true;
    }

    if (!arch_cpb.tsx_ctrl)
      return true;

    msr::tsx_ctrl_t tsx_ctl{};
    tsx_ctl.flags = ia32_asm_read_msr(tsx_ctl.msr_id);

    tsx_ctl.rtm_disable = 1;
    tsx_ctl.cpuid_clear = 1;

    ia32_asm_write_msr(tsx_ctl.msr_id, tsx_ctl.flags);

    //
    // verify the msr has been actually written.
    //
    msr::tsx_ctrl_t current_tsx_ctl{};
    current_tsx_ctl.flags = ia32_asm_read_msr(current_tsx_ctl.msr_id);

    return (current_tsx_ctl.flags == tsx_ctl.flags);
  };


  //
  // Here you got some time to apply changes for the specific cpu prior to virtualization.
  // This is called for each processor on the machine, use it to set MSR registers on the HOST, for example.
  //

  if (!disable_tsx())
  {
    LOG_WARN_SAFE("failed to disable TSX.");
    return false;
  }

  return true;
}
