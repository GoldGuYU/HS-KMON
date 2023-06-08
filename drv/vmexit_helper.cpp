#include <include.h>

void* vmexit_custom_handler::map_by_virtual_address(vcpu_t& vp, va_t va, size_t length, cr3_t cr3)
{
  auto& mm_translator = vp.guest_memory_translator();
  auto& mm_mapper = vp.guest_memory_mapper();

  if (length > page_size)
  {
    //
    // memory larger than a single page is currently not supported.
    //

    return nullptr;
  }

  const auto phys_address = mm_translator.va_to_pa(va, cr3);

  if (phys_address == 0)
    return nullptr;

  return mm_mapper.map(phys_address);
}
