#include <include.h>

void ioctl::handle_pre(PDEVICE_OBJECT pDevice, PIRP pIrp, info_t* info)
{

}

void ioctl::handle_post(PDEVICE_OBJECT pDevice, PIRP pIrp, info_t* info, NTSTATUS dwStatus, BOOL should_log)
{

}

void ioctl::setup_completion_redirect(
  PIO_STACK_LOCATION stack, PIRP irp, void* handler, u_long ctl_code
)
{
  auto* const ctx_mem = new context_t();
  auto* const ctx = mem::addr(ctx_mem).As<context_t*>();

  if (ctx == nullptr)
    return;

  RtlZeroMemory(ctx, sizeof(context_t));

  ctx->magic = ctx_magic;
  ctx->control_code = ctl_code;

  ctx->orig_ctx = stack->Context;
  ctx->orig_comp_routine = stack->CompletionRoutine;

  ctx->system_buffer =
    irp->AssociatedIrp.SystemBuffer;

  ctx->system_buffer_len =
    stack->Parameters.DeviceIoControl.OutputBufferLength;

  if (ctx->system_buffer && ctx->system_buffer_len)
  {
    auto copy_length_max = (sizeof(ctx->sbs_buffer) - 1);
    auto copy_length = ctx->system_buffer_len;

    if (copy_length >= copy_length_max)
      copy_length = copy_length_max;

    ctx->sbs_available = util::CopyVirtualMemory(
      ctx->sbs_buffer,
      ctx->system_buffer,
      copy_length
    );
  }

  stack->Control = SL_INVOKE_ON_SUCCESS;
  stack->Context = ctx;

  stack->CompletionRoutine =
    mem::addr(handler)
    .As<PIO_COMPLETION_ROUTINE>();
}

void ioctl::complete_request(context_t* ctx)
{
  if (ctx == nullptr)
    return;

  delete ctx;
}
