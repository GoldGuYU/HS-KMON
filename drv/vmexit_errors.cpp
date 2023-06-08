#include <include.h>

void vmexit_custom_handler::handle_fallback(vcpu_t& vp) noexcept
{
  _dbg_fatal("unhandled vm-exit has occurred -> %d", vp.exit_reason());

  BUG_CHECK_EX(hvpp_ecode::ECODE_UNHANDLED_VMEXIT, vp.guest_rip(), 0, 0, 0);
}

void vmexit_custom_handler::handle_ept_misconfiguration(vcpu_t& vp) noexcept
{
  _dbg_fatal("received EPT misconfig vm-exit.");

  BUG_CHECK_EX(hvpp_ecode::ECODE_EPT_MISCONFIGURATION, vp.guest_rip(), 0, 0, 0);
}

void vmexit_custom_handler::handle_error_invalid_guest_state(vcpu_t& vp) noexcept
{
  _dbg_fatal("invalid guest state has been reported.");

  BUG_CHECK_EX(hvpp_ecode::ECODE_VMENTRY_FAILURE_INVALID_GUEST_STATE, vp.guest_rip(), 0, 0, 0);
}

void vmexit_custom_handler::handle_error_machine_check(vcpu_t& vp) noexcept
{
  _dbg_fatal("machine check has been reported.");

  BUG_CHECK_EX(hvpp_ecode::ECODE_VMENTRY_FAILRE_MACHINE_CHECK, vp.guest_rip(), 0, 0, 0);
}

void vmexit_custom_handler::handle_error_msr_load(vcpu_t& vp) noexcept
{
  _dbg_fatal("MSR load error has been reported.");

  BUG_CHECK_EX(hvpp_ecode::ECODE_VMENTRY_FAILURE_MSR_LOAD, vp.guest_rip(), 0, 0, 0);
}

void vmexit_custom_handler::handle_triple_fault(vcpu_t& vp) noexcept
{
  _dbg_fatal("TRIPLE fault has occurred.");

  BUG_CHECK_EX(hvpp_ecode::ECODE_TRIPLE_FAULT, vp.guest_rip(), 0, 0, 0);
}
