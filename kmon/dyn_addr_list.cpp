#include <include.h>

#define DA_ENTRY(NAME, MODULE, TYPE, PATTERN, ...) { FALSE, _XS(NAME), MODULE, TYPE, _XSW(PATTERN), __VA_ARGS__ }
#define DA_ENTRY_OPT(NAME, MODULE, TYPE, PATTERN, ...) { TRUE, _XS(NAME), MODULE, TYPE, _XSW(PATTERN), __VA_ARGS__ }

bool dyn_addr::declare()
{
  /*static entry_t entries[] =
  {
    DA_ENTRY("ps_suspend_thread", nullptr, types::code,
    L"48 8B C4 48 89 58 20 48 89 50 10 48 89 48 08 56 57 41 56 48 83 EC 30 48 8B F2 48 8B F9 83 60 D8 00 65 4C 8B 34 25"),

    DA_ENTRY("ke_balance_set_mgr", nullptr, types::code,
    L"48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 41 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 65 48 8B 0C"),

    DA_ENTRY("load_system_image_fn", nullptr, types::code,
      L"48 89 5C 24 ? 48 89 54 24 ? 48 89 4C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 85 ? ? ? ? 33 FF BE"),

    DA_ENTRY("locate_address", nullptr, types::code,
      L"65 48 8B 04 25 ? ? ? ? 4C 8B C1 4C 8B 88 ? ? ? ? 49 8B 81"),

    DA_ENTRY("ssdt_regular", nullptr, types::pointer,
      L"4C 8D 15 ? ? ? ? 4C 8D 1D ? ? ? ? F7 43", 0x0, 0x3, 0x7),

    DA_ENTRY("ki_nmi_in_progress", nullptr, types::pointer,
    L"48 8D 0D ? ? ? ? E8 ? ? ? ? 33 D2 85 C0 8B CA 8D 42 01 0F 44 C8", 0x0, 0x3, 0x7),

    DA_ENTRY("call_nr_process_fn", nullptr, types::code,
    L"48 89 5C 24 ? 44 88 44 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 81 EC ? ? ? ? 48 8B F1 41 8A F8 48 8B 0D"),

    DA_ENTRY("ps_watch_working_set_fn", nullptr, types::code,
    L"48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 65 48 8B 1C 25 ? ? ? ? 4C 8B DA 44 8B D1 48 8B 83 ? ? ? ? 4C"),
  };

  memset(_data, 0, sizeof(_data));
  memcpy(_data, entries, sizeof(entries));

  _count = (sizeof(entries) / sizeof(entry_t));

  for (size_t i = 0; i < _count; i++)
  {
    auto& entry = _data[i];

    if (entry.m_tmp_name != nullptr)
      strcpy_s(entry.m_name, entry.m_tmp_name);

    if (entry.m_tmp_module_name != nullptr)
      strcpy_s(entry.m_module_name, entry.m_tmp_module_name);

    if (entry.m_tmp_pattern != nullptr)
      wcscpy_s(entry.m_pattern, entry.m_tmp_pattern);
  }

  return true;*/

  return true;
}
