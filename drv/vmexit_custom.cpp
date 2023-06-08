#include <include.h>

auto vmexit_custom_handler::user_data(vcpu_t& vp) noexcept -> per_vcpu_data&
{
  return *reinterpret_cast<per_vcpu_data*>(vp.user_data());
}

auto vmexit_custom_handler::setup(vcpu_t& vp) noexcept -> error_code_t
{
  base_type::setup(vp);

  auto data = new per_vcpu_data{ };

  data->ept.map_identity();

  data->mtf_context = {};

  data->shadow_pages.initialize(&vp.allocator());

  vp.user_data(data);

  vp.ept(data->ept);
  vp.ept_enable();

  std::invoke([&]() {
      //auto ctl = vp.processor_based_controls2();
      //ctl.descriptor_table_exiting = true;
      //vp.processor_based_controls2(ctl);
  });

  // uncommenting below lines causes
  // every cr0/cr4 write (check read later maybe)
  // to trigger an exit.
  //vp.cr0_guest_host_mask(cr0_t{ ~0ull });
  //vp.cr4_guest_host_mask(cr4_t{ ~0ull });

  return {};
}

void vmexit_custom_handler::teardown(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);

  data.shadow_pages.destroy();

  delete &data;

  vp.user_data(nullptr);
}

void vmexit_custom_handler::handle_execute_hlt(vcpu_t& vp) noexcept
{
  // ...
}

void vmexit_custom_handler::handle_execute_cpuid(vcpu_t& vp) noexcept
{
  base_type::handle_execute_cpuid(vp);
}

void vmexit_custom_handler::handle_execute_vmcall(vcpu_t& vp) noexcept
{
  _dbg("vm_call(%p)", vp.guest_rip());

  const auto rip = vp.guest_rip();
  const auto code = vp.context().rcx;

  if (is_privileged_call(rip))
    return on_dispatch_privileged(vp, code);

  return on_dispatch_regular(vp, code);
}

void vmexit_custom_handler::handle_vm_fallback(vcpu_t& vp) noexcept
{
  _dbg("vm_fallback(%p, %d)", vp.guest_rip(), vp.exit_reason());

  base_type::handle_vm_fallback(vp);
}

void vmexit_custom_handler::handle_execute_rdtsc(vcpu_t& vp) noexcept
{
  uint64_t tsc = vp.tsc_virt();

  vp.context().rax = tsc & 0xffffffff;
  vp.context().rdx = tsc >> 32;

  vp.compensate_tsc(false);
}

void vmexit_custom_handler::handle_ept_violation(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);

  auto helper = c_shadow_helper(vp.allocator(), &*data.shadow_pages);

  auto guest_pa = vp.exit_guest_physical_address();
  auto guest_va = vp.exit_guest_linear_address();
  auto exit_qual = vp.exit_qualification().ept_violation;

  auto full_page = helper.find_page_by_pa_ex( page_align( guest_pa.value() ) );

  if (full_page)
  {
    auto ept_entry = vp.ept().ept_entry(full_page->page_target.pa);

    HS_ASSERT( (ept_entry != nullptr) );

    switch (full_page->page_type)
    {
    case page_types::execute_shadow:
      hvpp::handle_ept_viol_exec_shadow(vp, full_page, exit_qual, data.mtf_context);
      break;
    case page_types::data_shadow:
      hvpp::handle_ept_viol_data_shadow(vp, full_page, exit_qual, data.mtf_context);
      break;
    default:
      BUG_CHECK_IM();
    }
  }
  else
  {
    LOG_WARN("unrecognized EPT violation for page PA=%p/VA=%p", guest_pa.value(), guest_va.value());
    DEBUG_BREAK();
  }

  vp.suppress_rip_adjust();
}

void vmexit_custom_handler::handle_interrupt(vcpu_t& vp) noexcept
{
  return base_type::handle_interrupt(vp);
}

void vmexit_custom_handler::handle_monitor_trap_flag(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);
  auto& mtf_ctx = data.mtf_context;

  if (mtf_ctx.awaiting_exit)
  {
    switch (mtf_ctx.operation)
    {
    case mtf_operations::restore_no_access: mtf::restore_no_access(vp, mtf_ctx); break;
    case mtf_operations::exec_shadow_track_write: mtf::track_exec_page_write(vp, mtf_ctx); break;
    default:
      _dbg("unrecognized MTF operation = %d", static_cast<int>(mtf_ctx.operation));
      break;
    }
  }
  else
  {
    _dbg("unexpected MTF while vcpu is not awaiting one.");
  }

  mtf_ctx = { };

  vp.guest_mtf(false);
  vp.suppress_rip_adjust();
}

void vmexit_custom_handler::handle_mov_cr(vcpu_t& vp) noexcept
{
  base_type::handle_mov_cr(vp);
}

void vmexit_custom_handler::handle_execute_rdmsr(vcpu_t& vp) noexcept
{
  base_type::handle_execute_rdmsr(vp);
}

void vmexit_custom_handler::handle_execute_wrmsr(vcpu_t& vp) noexcept
{
  base_type::handle_execute_wrmsr(vp);
}

void vmexit_custom_handler::handle_gdtr_idtr_access(vcpu_t& vp) noexcept
{
  base_type::handle_gdtr_idtr_access(vp);
}

void vmexit_custom_handler::handle_execute_invlpg(vcpu_t& vp) noexcept
{
  if (!hypervisor::is_within_ntos(vp.guest_rip()))
    _dbg("invlpg %p", vp.guest_rip());

  base_type::handle_execute_invlpg(vp);
}
