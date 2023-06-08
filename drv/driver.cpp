#include <include.h>

void driver::get_address_ranges()
{
  driver::highest_user_address = MmHighestUserAddress;
  driver::system_range_start_address = MmSystemRangeStart;
}

bool driver::get_self_module_info()
{
#if defined(HVPP_DEBUG)
  driver::debug_build = true;
#endif

  driver::image_base = &__ImageBase;

  if (!driver::image_base)
    return false;

  auto my_base = reinterpret_cast<uintptr_t>(driver::image_base);

  if (!my_base)
    return false;

  auto* const my_dh =
    reinterpret_cast<PIMAGE_DOS_HEADER>(my_base);

  if (my_dh->e_magic != IMAGE_DOS_SIGNATURE)
    return false;

  auto* const my_nth =
    reinterpret_cast<PIMAGE_NT_HEADERS>(static_cast<uintptr_t>(my_base + my_dh->e_lfanew));

  if (my_nth->Signature != IMAGE_NT_SIGNATURE)
    return false;

  if (!pe::FindSection(PTR_OF(my_base), _XS(".vmx"), &g::sec_vmx))
    return false;

  driver::image_length = my_nth->OptionalHeader.SizeOfImage;

  memcpy(&driver::image_dh, my_dh, sizeof(driver::image_dh));
  memcpy(&driver::image_nh, my_nth, sizeof(driver::image_nh));

  if (driver::image_dh.e_magic != IMAGE_DOS_SIGNATURE ||
    driver::image_nh.Signature != IMAGE_NT_SIGNATURE)
    return false;

  return true;
}

bool driver::get_kernel_main_info()
{
  auto nt_base = BASE_OF(util::get_kernel_base());

  if (!nt_base)
    return false;

  auto* const dh =
    reinterpret_cast<PIMAGE_DOS_HEADER>(nt_base);

  if (dh->e_magic != IMAGE_DOS_SIGNATURE)
    return false;

  auto* const nth =
    reinterpret_cast<PIMAGE_NT_HEADERS>(static_cast<uintptr_t>(nt_base + dh->e_lfanew));

  if (nth->Signature != IMAGE_NT_SIGNATURE)
    return false;

  auto nt_length = nth->OptionalHeader.SizeOfImage;

  if (!nt_length)
    return false;

  g::ntoskrnl.Base = nt_base;
  g::ntoskrnl.Length = nt_length;

  hypervisor::g.nt.base = nt_base;
  hypervisor::g.nt.len = nt_length;

  return true;
}
