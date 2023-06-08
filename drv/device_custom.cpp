#include <include.h>

error_code_t device_custom::on_ioctl(void* buffer, size_t buffer_size, uint32_t code) noexcept
{
  switch (code)
  {
    case ioctl_enable_io_debugbreak_t::code:
      return ioctl_enable_io_debugbreak(buffer, buffer_size);

    case ioctl_send_symbol_data_t::code:
      return ioctl_send_symbol_data(buffer, buffer_size);

    case ioctl_disable_t::code:
      return ioctl_disable();

    case ioctl_enable_kmon_t::code:
      return ioctl_enable_kmon();

    case ioctl_enable_hooks_t::code:
      return ioctl_enable_hooks();

    default:
      return make_error_code_t(std::errc::invalid_argument);
  }
}

error_code_t device_custom::ioctl_enable_io_debugbreak(void* buffer, size_t buffer_size)
{
  return {};
}

error_code_t device_custom::ioctl_send_symbol_data(void* buffer, size_t buffer_size)
{
  /*if (!buffer)
    return make_error_code_t(std::errc::invalid_argument);

  auto data = reinterpret_cast<ds::symbol_data_t*>(buffer);

  if (data && data->length > 0)
  {
    uintptr_t module_base{ };

    if (!strcmp(data->image_name, _XS("ntoskrnl.exe")))
      module_base = g::ntoskrnl.Base;
    else
    {
      k::SYSTEM_MODULE_ENTRY image_info{ };
      
      if (util::GetKernelModuleInfo(data->image_name, image_info, FALSE, TRUE))
      {
        module_base = BASE_OF(image_info.ImageBase);
      }
      else
      {
        return make_error_code_t(std::errc::invalid_argument);
      }
    }

    if (module_base != 0)
    {
      c_symbols::create_module(data->image_name, module_base);

      if (data->count > 0)
      {
        for (auto i = 0; i < data->count; i++)
        {
          auto entry = data->list[i];

          LOG_INFO("@ adding symbol (%s) ['%s', '%x']", data->image_name, entry.function_name, entry.function_rva);

          c_symbols::add_for_module(data->image_name, entry.function_name, entry.function_rva);
          g::status.symbols_count++;
        }
      }
    }
  }

  //
  // NOTE: This does not indicate that ALL required symbols were received.
  // Instead it just says if any symbols have been received at all!!
  //
  g::status.symbols_received = true;*/

  return {};
}

error_code_t device_custom::ioctl_disable()
{
  this->destroy();

  LOG_WARN("ioctl interface has been destroyed.");

  return {};
}

error_code_t device_custom::ioctl_enable_kmon()
{
  // DEPRECATED
  return {};
}

void thr(void* p)
{
  static bool were_initialized = false;

  if (!were_initialized)
  {
    were_initialized = hooks::Initialize();

    if (!were_initialized)
      BUG_CHECK_IM();
  }
}

error_code_t device_custom::ioctl_enable_hooks()
{
  auto handle = util::CreateThreadEx(thr);

  if (handle)
  {
    if (!util::WaitForThread(handle))
      BUG_CHECK_IM();
  }

  return {};
}
