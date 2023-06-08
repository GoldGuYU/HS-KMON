#include <include.h>

VOID testDPC(PRKDPC Dpc, PVOID Context, PVOID SystemArgument1, PVOID SystemArgument2)
{
  _disable();

  ia32_asm_vmx_vmcall((int)hvpp_comms::hypercall_codes::START_SPLIT_ALL_SHADOW_PAGES_FOR_PROC, 4 /*SYSTEM*/, 0, 0);

  iw::signal_call_dpc_sync(SystemArgument2);
  iw::signal_call_dpc_done(SystemArgument1);

  _enable();
}

BOOLEAN Comms::Hv::HandleAddShadowDataWatcher(sdk::comms::Context_t* pContext)
{
  u64 target = pContext->VRegs.vcx;
  u64 length = pContext->VRegs.vdx;
  u8 should_remove = (pContext->VRegs.v8 == TRUE);

  if (!MmIsAddressValid(PTR_OF(target)))
  {
    LOG_WARN("AddShadowDataWatcher -> invalid address!");
    return FALSE;
  }

  if (!should_remove)
  {
    LOG_INFO("AddShadowDataWatcher -> %p (L%llu)", target, length);

    if (!sh::mgr->place_data_shadow_krn(PTR_OF(target), length))
    {
      LOG_INFO("failed to place data shadow!!!!!!");
      return FALSE;
    }
  }
  else
  {
    LOG_INFO("RmShadowDataWatcher -> %p", target);

    sh::mgr->remove_data_shadow_krn(PTR_OF(target));
  }

  iw::generic_call_dpc( testDPC, nullptr );

  return TRUE;
}
