#include <include.h>

DECLARE_VMX();

void vmexit_custom_handler::on_dispatch_privileged(vcpu_t& vp, uint64_t code) noexcept
{
  using code_ids = hvpp_comms::hypercall_codes;

  switch ( static_cast<code_ids>(code) )
  {
  case code_ids::ADD_SHADOW_PAGE:
    on_add_shadow_page(vp);
    break;
  case code_ids::REMOVE_SHADOW_PAGE:
    on_remove_shadow_page(vp);
    break;
  case code_ids::START_SPLIT_SHADOW_PAGE:
    on_start_split_shadow_page(vp);
    break;
  case code_ids::START_SPLIT_ALL_SHADOW_PAGES_FOR_PROC:
    on_start_split_all_shadow_pages_for_process(vp);
    break;
  case code_ids::STOP_SPLIT_SHADOW_PAGE:
    on_stop_split_shadow_page(vp);
    break;
  case code_ids::ADD_SHADOW_PAGE_WATCHER:
    on_add_shadow_page_watcher(vp);
    break;
  case code_ids::REMOVE_SHADOW_PAGE_WATCHER:
    on_remove_shadow_page_watcher(vp);
    break;
  case code_ids::BREAK_INTO_DEBUGGER:
    __debugbreak();
    break;

  default:
    return base_type::handle_execute_vmcall(vp);
  }
}

void vmexit_custom_handler::on_dispatch_regular(vcpu_t& vp, uint64_t code) noexcept
{
  using code_ids = hvpp_comms::hypercall_codes;

  switch ( static_cast<code_ids>(code) )
  {
  case code_ids::ENTER_HYPER_COMMS:
    on_enter_hyper_comms(vp);
    break;
  default:
    return base_type::handle_execute_vmcall(vp);
  }
}
