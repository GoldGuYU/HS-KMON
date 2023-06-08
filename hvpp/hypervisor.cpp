#include "hvpp/hypervisor.h"
#include "hvpp/config.h"

#include "hvpp/ia32/cpuid/cpuid_eax_01.h"
#include "hvpp/lib/assert.h"
#include "hvpp/lib/driver.h"
#include "hvpp/lib/log.h"
#include "hvpp/lib/mm.h"
#include "hvpp/lib/mp.h"
#include "hvpp/lib/ntapi.h"

#include "hvpp/global.h"
#include "hvpp/idt.h"

namespace hvpp::hypervisor
{
  globals_t g;

  namespace detail
  {
    static
    bool
    check_cpu_features(
      void
      ) noexcept
    {
      cpuid_eax_01 cpuid_info;
      ia32_asm_cpuid(cpuid_info.cpu_info, 1);
      if (!cpuid_info.feature_information_ecx.virtual_machine_extensions)
      {
        return false;
      }

      const auto cr4 = read<cr4_t>();
      if (cr4.vmx_enable)
      {
        return false;
      }

      const auto vmx_basic = msr::read<msr::vmx_basic_t>();
      if (
          vmx_basic.vmcs_size_in_bytes > page_size ||
          vmx_basic.memory_type != uint64_t(memory_type::write_back) ||
         !vmx_basic.true_controls
        )
      {
        return false;
      }

      const auto vmx_ept_vpid_cap = msr::read<msr::vmx_ept_vpid_cap_t>();
      if (
        !vmx_ept_vpid_cap.page_walk_length_4      ||
        !vmx_ept_vpid_cap.memory_type_write_back  ||
        !vmx_ept_vpid_cap.invept                  ||
        !vmx_ept_vpid_cap.invept_all_contexts     ||
        !vmx_ept_vpid_cap.execute_only_pages      ||
        !vmx_ept_vpid_cap.pde_2mb_pages
        )
      {
        return false;
      }

      return true;
    }

    static bool setup_host_idt()
    {
      auto idtr = read<idtr_t>();

      //
      // Setup IDT, copy original entries and intercept some here.
      //
      idtr.copy_to(g::idt.table);
      idtr.copy_to(g::idt.original);

      //
      // Modify it's entries here if required.
      //

      return true;
    }

    static bool setup_host_page_tables()
    {
      //
      // Setup our custom paging tables.
      //

      PHYSICAL_ADDRESS PageMapLevel4{ };
      PHYSICAL_ADDRESS PageMapLevel4Clone{ };
      PVOID PageMapLevel4Virt{ };
      NTSTATUS Result{ };

      PageMapLevel4.QuadPart = cr3_t{ __readcr3() }.page_frame_number << 12;
      PageMapLevel4Virt = MmGetVirtualForPhysical(PageMapLevel4);

      if (!NT_SUCCESS((Result = MmAllocateCopyRemove(PageMapLevel4Virt, 0x1000, &PageMapLevel4Clone))))
        return false;

      //
      // Craft the new host CR3 value.
      //

      cr3_t cr3_value{ };

      cr3_value.flags = __readcr3();
      cr3_value.page_frame_number =
        (PageMapLevel4Clone.QuadPart >> 12);

      mm::my_cr3 = cr3_value;

      return true;
    }
  }

  struct global_t
  {
    vcpu_t* vcpu_list;
    bool    running;
  };

  static global_t global;

  auto start(vmexit_handler& handler) noexcept -> error_code_t
  {
    _dbg("hypervisor->start( )");

    //
    // If hypervisor is already running,
    // don't do anything.
    //
    hvpp_assert(!global.running);
    if (global.running)
    {
      return make_error_code_t(std::errc::operation_not_permitted);
    }

    //
    // Create array of VCPUs.
    //
    // Note that since:
    //   - vcpu_t is not default-constructible
    //   - operator new[] doesn't support constructing objects
    //     with parameters
    // ... we have to construct this array "placement new".
    //
    hvpp_assert(global.vcpu_list == nullptr);

    global.vcpu_list = reinterpret_cast<vcpu_t*>(operator new(sizeof(vcpu_t) * mp::cpu_count()));
    if (!global.vcpu_list)
    {
      return make_error_code_t(std::errc::not_enough_memory);
    }

    //
    // Construct each vcpu_t object as `vcpu_t(handler)'.
    //
    std::for_each_n(global.vcpu_list, mp::cpu_count(),
      [&](vcpu_t& vp) {
        ::new (std::addressof(vp)) vcpu_t(handler);
      });

    //
    // Check if hypervisor-allocator has been set.
    //
    // If the driver is calling `hypervisor::start()' from
    // the `driver::initialize()' method without manually
    // setting the allocator, then the allocator will be empty.
    //
    if (!mm::hypervisor_allocator())
    {
      //
      // Hypervisor-allocator has not been set - create default one.
      //
      // Note:
      //   Default hypervisor allocator is automatically destroyed
      //   in the `driver::common::destroy()' function.
      //
      if (auto err = driver::common::hypervisor_allocator_default_initialize())
      {
        return err;
      }
    }

    //
    // Setup and Pre-Allocate our VCPU allocators.
    //
    auto init_allocator = [](vcpu_t& vp)
    {
      auto& alloc = vp.allocator();
      auto cap = (driver::common::hypervisor_allocator_recommended_capacity() / mp::cpu_count());

      if (cap <= 0)
        return false;

      auto storage = mm::system_allocator()->allocate(cap);

      if (storage == nullptr)
        return false;

      memset(storage, 0, cap);

      if (alloc.attach(storage, cap).value() != 0)
        return false;

      return true;
    };

    std::for_each_n(global.vcpu_list, mp::cpu_count(),
      [&](vcpu_t& vp) {
        if (!init_allocator(vp))
        {
          ia32_asm_int3();
        }
      });

    //
    // Check that CPU supports all required features to
    // run this hypervisor.
    //
    // Note:
    //   This check is performed only on current CPU
    //   and assumes all CPUs are symmetrical.
    //
    if (!detail::check_cpu_features())
    {
      return make_error_code_t(std::errc::not_supported);
    }

    //
    // Identify the processor vendor and check if the system was virtualized beforehand.
    // We need this in order to be able to estimate our environment better and support edge cases.
    //
    g::cpu_id::cpu_vendor = cpuident::query_processor_vendor();

    if (g::cpu_id::cpu_vendor == cpuident::processor_vendor::invalid ||
      !cpuident::query_hypervisor_info(&g::cpu_id::hv_info))
    {
      return make_error_code_t(std::errc::not_supported);
    }

    //
    // Setup global host tables.
    //
    if (!detail::setup_host_idt())
    {
      return make_error_code_t(std::errc::operation_canceled);
    }

    if (!detail::setup_host_page_tables())
    {
      return make_error_code_t(std::errc::operation_canceled);
    }

    //
    // Start virtualization on all CPUs.
    //
    // Note:
    //   IPI callbacks run at IRQL IPI_LEVEL,
    //   At that level, we cannot call ExAllocatePoolWithTag,
    //   therefore we have to use hypervisor allocator.
    //
    auto start_err = std::atomic<error_code_t>{};

    mp::ipi_call([&start_err]() {
      mm::allocator_guard _;

      const auto idx = mp::cpu_index();
      const auto err = global.vcpu_list[idx].start();

      auto expected = error_code_t{};
      start_err.compare_exchange_strong(expected, err);
    });

    //
    // Signalize that hypervisor has started.
    //
    global.running = true;

    //
    // If any VCPU started with an error, stop
    // the hypervisor.
    //
    // Note:
    //   This check must come AFTER setting "running"
    //   to true, otherwise "stop()" would perform
    //   early-exit without any actual stopping.
    //
    if (auto err = start_err.load())
    {
      stop();
      return err;
    }

    _dbg("driver base = %p", driver::image_base);
    _dbg("driver length = %llx", driver::image_length);

    _dbg("hypervisor->start( )  OK");

    return {};
  }

  void stop() noexcept
  {
    _dbg("hypervisor->stop()");

    //
    // If hypervisor is already stopped,
    // don't do anything.
    //
    hvpp_assert(global.running);
    if (!global.running)
    {
      return;
    }

    //
    // Stop virtualization on all CPUs.
    //
    mp::ipi_call([]() {
      mm::allocator_guard _;

      const auto idx = mp::cpu_index();
      global.vcpu_list[idx].stop();
    });

    //
    // Destroy array of VCPUs.
    //
    std::destroy_n(global.vcpu_list, mp::cpu_count());
    delete static_cast<void*>(global.vcpu_list);

    global.vcpu_list = nullptr;

    //
    // Signalize that hypervisor has stopped.
    //
    global.running = false;

    _dbg("hypervisor->stop()  OK");
  }

  bool is_running() noexcept
  {
    return global.running;
  }

  /*
  * ONLY CALL THIS FROM A CONTEXT WHERE THE CPU CORE IS CERTAIN.
  * FOR EXAMPLE, VM-EXIT HANDLERS OR OTHER HV ROUTINES.
  */
  __declspec(noreturn) void on_fatal_vcpu_state(vcpu_t& vp) noexcept
  {
    _dbg("[hv] fatal vcpu state reported for this core, halting!");

    ia32_asm_halt();
  }
}
