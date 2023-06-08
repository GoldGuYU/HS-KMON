#pragma once

namespace hyper_comms
{
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
}

namespace hv::sdk
{
  enum HvInfo : uint64_t
  {
    HVI_NOT_SET = 0,

    HVI_STATUS,
    HVI_BACK_LOG,
    HVI_DIAG_INFO,

    HVI_AC_CONTEXT_EAC,

    HVI_OS_UNLOADED_DRIVERS,
    HVI_OS_CACHED_DRIVERS,
    HVI_OS_LOADED_DRIVERS,
    HVI_OS_NOTIFY_CALLBACKS_THREAD,
  };

  enum HviQueryResult : int32_t
  {
    HVI_QUERY_RESULT_NOT_SET = 0,

    HVI_QUERY_RESULT_SUCCESS,
    HVI_QUERY_RESULT_ACCESS_DENIED,

    HVI_QUERY_RESULT_INSUFFICIENT_MEMORY,
    HVI_QUERY_RESULT_BUFFER_TOO_SMALL,
  };

  enum tables : uint64_t
  {
    table_generic = 0x1,
    table_debug,
  };

  struct module_info_ex
  {
    uintptr_t base;
    uintptr_t length;
    wchar_t name[64];
  };

  struct Context_t
  {
    ULONG Id;
    PVOID Data;

    struct
    {
      uint64_t vcx;
      uint64_t vdx;
      uint64_t v8;
      uint64_t v9;
    } VRegs;

    uint64_t ReturnStatus;
  };

  struct InfoQuery_t
  {
    DWORD_PTR Id;
    PSIZE_T Length;
    INT Result;
    PVOID Buffer;
  };

  struct MsrAccess_t
  {
    enum Types
    {
      MSR_ACCESS_READ,
      MSR_ACCESS_WRITE,
    };

    Types Type;
    DWORD_PTR Address;
    DWORD_PTR* Buffer;
  };

  struct SystemModuleExportInfo_t
  {
    char ModuleName[128];
    char RoutineName[256];
    DWORD_PTR Result;
  };

  namespace mmp
  {
    struct module_info_t
    {
      uintptr_t base;
      uintptr_t length;
    };

    struct virtual_memory_info_t
    {
      uint64_t address;
      u_long page_size;
      u_long protect;
      k::_MMVAD_FLAGS flags;
    };

    struct query_virtual_memory_t
    {
      uint32_t process_id;
      uint64_t address;
      uint64_t length;
    };

    struct read_virtual_memory_t
    {
      uint32_t process_id;
      uint64_t source_address;
      uint64_t length;
      void* destination_buffer;
    };

    struct write_virtual_memory_t
    {
      uint32_t process_id;
      uint64_t destination_address;
      uint64_t length;
      void* source_buffer;
      bool stealth;
      bool stealth_data;

      struct
      {
        struct
        {
          uintptr_t target_rip_start;
          uintptr_t target_rip_length;
        } data_shadow;
      } ts;
    };

    struct allocate_virtual_memory_t
    {
      uint32_t process_id;
      uint64_t address;
      uint64_t length;
      u_long protect;
      bool free;
      bool unset_disable_execute_bit;
    };

    struct protect_virtual_memory_t
    {
      uint32_t process_id;
      uint64_t address;
      uint64_t length;
      u_long protect_new;
      u_long protect_old;
    };

    struct map_virtual_memory_t
    {
      uint32_t process_id;
      uint64_t source_address;
      uint64_t mapped_address;
      uint64_t mdl;
      uint64_t length;
      bool release;
    };

    struct read_physical_memory_t
    {
      uint32_t process_id;
      uint64_t source_address;
      uint64_t length;
      void* destination_buffer;
    };

    struct write_physical_memory_t
    {
      uint32_t process_id;
      uint64_t destination_address;
      uint64_t length;
      void* source_buffer;
    };

    struct map_shadow_image_t
    {
      uint32_t process_id;
      uint64_t destination_address;
      uint64_t image_length;
      void* image_data;
    };

    struct scan_phys_mem_t
    {
      size_t data_length;
      void* data_buffer;
    };

    enum class execution_type
    {
      none = 0,

      iat_swap,
      vmt_swap,

      apc,
    };

    struct map_params_t
    {
      HANDLE process_id;
      size_t image_length;

      uintptr_t* remote_base;
      uintptr_t* remote_entry_point;

      execution_type exec_type;

      void* image_data;

      struct type_specific_data_t
      {
        //
        // type-specific data.
        //

        struct
        {
          //
          // execution-type specific.
          //

          struct
          {
            char target_module[128];
            char target_name[128];
          } iat_swap;

          struct
          {
            uintptr_t target_address;
          } vmt_swap;
        } et;
      } ts;
    };
  }

  constexpr ULONG Magic = 0x5968DEBB;
}

namespace hv::sdk
{
  extern NTSTATUS invoke(uint64_t rcx, uint64_t rdx, void* r8, uint64_t r9, uint64_t a5);
  extern bool invoke_ex(hyper_comms::request_t& request, bool strict = false) noexcept;

  extern bool send_symbol_data(c_device& device, const char* image_name, ds::symbol_entry* se, size_t len);
  extern void add_symbol_data(ds::symbol_entry* se, size_t* cnt, const char* name, uintptr_t rva);
  extern size_t query_info_length_ex(uint64_t id);
  extern void* query_info_ex(uint64_t id, size_t* length);
  extern bool trigger_vmxr_exception(bool safe = false);
  extern bool trigger_vmxr_hang();
  extern bool trigger_debug_break();
  extern bool trigger_bug_check();
  extern bool trigger_pg_checks();
  extern uintptr_t get_system_module_export(const std::string& module_name, const std::string& export_name);
  extern bool info_table_read(tables table_id, size_t entry_id, uint64_t* out_result);
  extern bool info_table_write(tables table_id, size_t entry_id, uint64_t value);
  extern size_t get_back_log_length();
  extern bool query_info(uint64_t id, void** buffer, size_t* length);
  extern bool map_image(mmp::map_params_t& info);
  extern bool clear_back_log();
  extern bool translate_virt_to_phys(uint64_t address, uint64_t* out);
  extern bool get_main_module_base(uint32_t pid, uint64_t* out);
  extern bool get_module_list(uint32_t pid, std::vector<module_info_ex>& buffer);
  extern bool get_module_info(uint32_t pid, const std::wstring& name, mmp::module_info_t* out);
  extern bool query_virt_memory(uint32_t pid, uintptr_t address, size_t length, mmp::virtual_memory_info_t* buffer);
  extern bool read_virt_memory(uint32_t pid, uint64_t src, void* dst, uint64_t length);
  extern bool write_virt_memory(uint32_t pid, uint64_t dst, void* src, uint64_t length, bool stealth = false);
  extern bool alloc_virt_memory(uint32_t pid, size_t length, uintptr_t& address, u_long protect);
  extern bool free_virt_memory(uint32_t pid, uintptr_t address, size_t length);
  extern bool protect_virt_memory(uint32_t pid,
    uintptr_t address, size_t length,
    u_long protect_new, u_long* protect_old = nullptr);
  extern bool read_phys_memory(uint32_t pid, uint64_t src, void* dst, uint64_t length);
  extern bool write_phys_memory(uint32_t pid, uint64_t dst, void* src, uint64_t length);
  extern bool read_virt_memory_ex(uint32_t pid, uint64_t src, void* dst, uint64_t length);
  extern bool write_virt_memory_ex(uint32_t pid, uint64_t dst, void* src, uint64_t length);
  extern bool get_system_module(const std::string& name, k::SYSTEM_MODULE_ENTRY* out_info);
  extern bool read_msr(uint64_t address, uint64_t* buffer);
  extern bool write_msr(uint64_t address, uint64_t value);
  extern bool add_data_shadow_watcher(uint64_t address, uint64_t length, bool remove = false);
}

namespace hv::sdk::helper
{
  __forceinline auto get_loaded_drivers() noexcept
  {
    auto result = std::vector<hs::sdk::loaded_driver_data_t::entry_t>();

    hs::sdk::loaded_driver_data_t* data = nullptr;
    size_t length{ };

    if (query_info(HVI_OS_LOADED_DRIVERS, (void**)&data, &length))
    {
      for (auto i = 0ull; i < data->hdr.count; i++)
      {
        result.push_back(data->m[i]);
      }
    }

    free(data);

    return result;
  }

  __forceinline auto get_unloaded_drivers() noexcept
  {
    auto result = std::vector<hs::sdk::unloaded_driver_data_t::entry_t>();

    hs::sdk::unloaded_driver_data_t* data = nullptr;
    size_t length{ };

    if (query_info(HVI_OS_UNLOADED_DRIVERS, (void**)&data, &length))
    {
      for (auto i = 0ull; i < data->hdr.count; i++)
      {
        result.push_back(data->m[i]);
      }
    }

    free(data);

    return result;
  }

  __forceinline auto get_cached_drivers() noexcept
  {
    auto result = std::vector<hs::sdk::cached_driver_data_t::entry_t>();

    hs::sdk::cached_driver_data_t* data = nullptr;
    size_t length{ };

    if (query_info(HVI_OS_CACHED_DRIVERS, (void**)&data, &length))
    {
      for (auto i = 0ull; i < data->hdr.count; i++)
      {
        result.push_back(data->m[i]);
      }
    }

    free(data);

    return result;
  }
}
