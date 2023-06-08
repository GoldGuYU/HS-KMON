#include <include.h>

DECLARE_VMX();

namespace hyper_comms
{
  enum Requests
  {
    REQUEST_QUERY_INFO = 1,
    REQUEST_FORCE_VMXR_EXCEPTION,
    REQUEST_FOR_VMXR_HANG,
  };

  struct request_t
  {
    uint32_t m_magic;
    uint32_t m_id;

    bool m_processed;
    bool m_completed;

    uint64_t m_return_code;

    struct
    {
      void* address;
      size_t length;
    } m_input_buffer, m_output_buffer;

    struct
    {
      uint64_t vcx;
      uint64_t vdx;
      uint64_t v8;
      uint64_t v9;
      uint64_t v10;
      uint64_t v11;
      uint64_t v12;
      uint64_t v13;
      uint64_t v14;
      uint64_t v15;
    } m_vregs;
  };

  bool post_result(vcpu_t& vp, request_t* request, void* wish_data, size_t wish_length)
  {
    if (!request || !wish_data || !wish_length)
      return false;

    auto& buffer_address = request->m_vregs.vdx;
    auto& buffer_length = request->m_vregs.v8;

    if (!buffer_address || !buffer_length)
    {
      buffer_length = wish_length;
      return false;
    }

    if (buffer_length < wish_length)
    {
      buffer_length = wish_length;
      return false;
    }

    if (vp.guest_write_memory(buffer_address, wish_data, wish_length, true).value() != 0)
      return false;

    return true;
  }

  ///
  /// VCX  =  INFO QUERY ID
  /// VDX  =  USER BUFFER ADDRESS
  /// V8   =  USER BUFFER LENGTH
  /// 
  bool handle_query_info(vcpu_t& vp, request_t* request)
  {
    auto query_id = request->m_vregs.vcx;

    bool result = true;

    switch (query_id)
    {
    case sdk::comms::HVI_STATUS:
    {
      result = post_result(vp, request, &g::status, sizeof(g::status));
      break;
    }
    case sdk::comms::HVI_BACK_LOG:
    {
      result = post_result(vp, request, PTR_OF(g::back_log.get()), g::back_log.length());
      break;
    }
    default:
      result = false;
      break;
    }

    return result;
  }
}

void vmexit_custom_handler::on_enter_hyper_comms(vcpu_t& vp) noexcept
{
  on_handle_hyper_comms(vp);
}

bool vmexit_custom_handler::on_handle_hyper_comms(vcpu_t& vp) noexcept
{
  const auto info_addr = vp.context().rdx;

  if (info_addr == 0)
    return false;

  auto* const info_mapped =
    map_by_virtual_address(
      vp, info_addr,
      sizeof(hyper_comms::request_t),
      vp.guest_cr3()
    );

  if (info_mapped == nullptr)
    return false;

  auto* const info =
    mem::addr(info_mapped)
    .As<hyper_comms::request_t*>();

  auto __mp_guard =
    k::mapper_guard(vp.guest_memory_mapper());

  info->m_processed = true;

  using req = hyper_comms::Requests;

  switch( info->m_id )
  {
  case req::REQUEST_QUERY_INFO:
  {
    info->m_completed = hyper_comms::handle_query_info(vp, info);
    break;
  }
  case req::REQUEST_FORCE_VMXR_EXCEPTION:
  {
    /* the line below will force an exception in VMX root. */

    if (info->m_vregs.vcx /* should it be handled or not? */)
    {
      //
      // Safe exception, if functional our exception handling will just catch this one.
      //

      __try
      {
        *(int*)(0x0) = 1337;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
        __noop();
      }
    }
    else
    {
      //
      // Unsafe exception, the system will crash if everything works properly.
      //

      *(int*)(0x0) = 1337;

      /* should not reach here */
    }

    break;
  }
  case req::REQUEST_FOR_VMXR_HANG:
  {
    _dbg("forcing VMXR hang due to request.");

    spinlock lock{ };

    lock.lock();
    lock.lock();

    _dbg_warn("forced VMXR hang, but it somehow escaped.");

    //
    // should not reach here.
    //

    break;
  }
  default:
    info->m_processed = false;
    break;
  }

  return info->m_processed;
}
