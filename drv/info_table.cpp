#include <include.h>

namespace info_table
{
  void c_manager::shutdown()
  {
    if (!m_count)
      return;

    for (size_t idx = 0; idx < m_count; idx++)
    {
      auto* entry = m_list[idx];

      NT_ASSERT(!(entry == nullptr)); // this is not allowed.

      if (entry == nullptr)
        continue;

      ExFreePool(entry);
    }
  }

  table_t* c_manager::find_table(size_t id)
  {
    if (!m_count)
      return nullptr; // our list is empty.

    table_t* result = nullptr;

    for (size_t idx = 0; idx < m_count; idx++)
    {
      auto* entry = m_list[idx];

      NT_ASSERT(!(entry == nullptr)); // this is not allowed.

      if (entry == nullptr)
        break; // immediately stop iterating because something is wrong.

      if (entry->m_id == id)
      {
        result = entry;
        break;
      }
    }

    return result;
  }

  bool c_manager::table_present(size_t id)
  {
    return !(find_table(id) == nullptr);
  }

  bool c_manager::table_create(size_t id)
  {
    NT_ASSERT((!(m_count >= 64)));
    NT_ASSERT((id > 0x00));
    NT_ASSERT(!(table_present(id)));

    table_t*& new_entry = m_list[m_count];

    NT_ASSERT(!(new_entry)); // the entry is already populated?

    new_entry =
      reinterpret_cast<table_t*>(ExAllocatePool(
        NonPagedPool, sizeof(table_t))
    );

    if (new_entry == nullptr)
      return false;

    memset(new_entry, 0, sizeof(table_t));

    new_entry->m_id = id;

    ++m_count;

    return true;
  }

  bool c_manager::table_read_value(size_t table_id, size_t entry_idx, uint64_t* out)
  {
    NT_ASSERT(out);

    if (!m_count)
      return false; // we've got no tables.

    table_t* table = find_table(table_id);

    if (table == nullptr)
      return false;

    if (entry_idx >= ARRAY_COUNT(table->m_contents, uint64_t))
      return false;

    *out = table->m_contents[entry_idx];

    return true;
  }

  bool c_manager::table_write_value(size_t table_id, size_t entry_idx, uint64_t val)
  {
    if (!m_count)
      return false; // we've got no tables.

    table_t* table = find_table(table_id);

    if (table == nullptr)
      return false;

    if (entry_idx >= ARRAY_COUNT(table->m_contents, uint64_t))
      return false;

    table->m_contents[entry_idx] = val;

    return true;
  }

  void c_manager::table_dump_contents(size_t id)
  {
    if (!m_count)
      return; // we've got no tables.

    table_t* table = find_table(id);

    if (table == nullptr)
      return;

    hvpp_trace(">> TABLE CONTENTS OF ID 0x%zu. \n", id);

    for (size_t idx = 0; idx < ARRAY_COUNT(table->m_contents, uint64_t); idx++)
    {
      if (table->m_contents[idx] == 0)
        continue;

      hvpp_trace("%zu  %llx. \n", idx, table->m_contents[idx]);
    }
  }
}
