#include <include.h>

uintptr_t dyn_addr::get(const char* name)
{
  if (_count <= 0)
    return 0;

  for (size_t i = 0; i < _count; i++)
  {
    const auto& entry = _data[i];

    if (!strcmp(entry.m_name, name))
      return entry.m_result;
  }

  return 0;
}

bool dyn_addr::find(entry_t& entry)
{
  auto module_info = k::SYSTEM_MODULE_ENTRY{};
  char module_name[32];

  memset(module_name, 0, sizeof(module_name));

  if (entry.m_tmp_module_name == nullptr)
    strcpy_s(module_name, _XS("ntoskrnl.exe"));
  else
    strcpy_s(module_name, entry.m_module_name);

  if (!util::GetKernelModuleInfo(module_name, module_info))
    return false;

  switch (entry.m_type)
  {
  case types::code:
  {
    entry.m_result = Sig::KMode::FindInKernelSpaceImage(
      mem::addr(module_info.ImageBase).Base(),
      module_info.ImageSize,
      entry.m_pattern,
      entry.m_offset
    );

    return !(entry.m_result == 0);
  }
  case types::pointer:
  {
    entry.m_result = Sig::KMode::FindInKernelSpaceImageAndResolve(
      mem::addr(module_info.ImageBase).Base(),
      module_info.ImageSize,
      entry.m_pattern,
      entry.m_offset,
      entry.m_instr_offset,
      entry.m_instr_length
    );

    return !(entry.m_result == 0);
  }
  default:
    return false;
  }

  return true;
}

bool dyn_addr::initialize()
{
  if (!declare())
    return false;

  LOG_INFO("dyn_addr:  starting search for %i entries.", _count);

  size_t error_count = 0;

  for (size_t i = 0; i < _count; i++)
  {
    auto& entry = _data[i];

    if (find(entry))
      LOG_DEBUG("dyn_addr:  pattern named '%s' has been found at %p.", entry.m_name, entry.m_result);
    else
    {
      if (!entry.m_optional)
      {
        ++error_count;
        LOG_INFO("dyn_addr:  pattern named '%s' could not be found (CRITICAL).", entry.m_name);

        break;
      }
      else
      {
        LOG_WARN("dyn_addr:  pattern named '%s' could not be found (OPTIONAL).", entry.m_name);
      }
    }
  }

  LOG_INFO("dyn_addr:  search complete.");

  return (error_count == 0);
}
