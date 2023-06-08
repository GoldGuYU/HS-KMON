#include <include.h>

using namespace kmon;

void c_kmon::on_allocate_virtual_memory(_handle process_id, uintptr_t base, size_t length)
{
  auto info = memory_alloc_t{};

  info.active = true;
  info.process_id = process_id;
  info.address = base;
  info.length = length;

  mem_allocs.insert(info);
}

void c_kmon::on_free_virtual_memory(_handle process_id, uintptr_t base, size_t length)
{
  if (mem_allocs.count() <= 0)
    return;

  for (auto* data = mem_allocs.first_entry();
    data != nullptr;
    data = data->m_next)
  {
    if (data->m_is_head) continue;

    const auto& entry = data->m_data;

    if (!entry.active || entry.process_id != process_id)
      continue;

    if (entry.address == base)
    {
      mem_allocs.remove(entry);
      break;
    }
  }
}

void c_kmon::on_free_virtual_memorys(_handle process_id)
{
  if (mem_allocs.count() <= 0)
    return;

  for (auto* data = mem_allocs.first_entry();
    data != nullptr;
    data = data->m_next)
  {
    if (data->m_is_head) continue;

    auto& entry = data->m_data;

    if (!entry.active || entry.process_id != process_id)
      continue;

    entry.active = false;
  }
}

bool c_kmon::on_query_virtual_memory(HANDLE process_id, uintptr_t base)
{
  if (mem_allocs.count() <= 0)
    return true;

  const auto wanted_base = base;

  for (auto* data = mem_allocs.first_entry();
    data != nullptr;
    data = data->m_next)
  {
    if (data->m_is_head) continue;

    const auto& entry = data->m_data;

    if (!entry.active || entry.process_id != process_id)
      continue;

    if (entry.address == wanted_base)
    {
      LOG_INFO("!!  FOUND QUERY");
      return false;
    }
  }

  return true;
}
