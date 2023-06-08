#include "hvpp/cpuident.h"
#include "hvpp/lib/crcpp.h"

#include <cinttypes>
#include <intrin.h>
#include <ntddk.h>

cpuident::processor_vendor cpuident::query_processor_vendor()
{
  char buffer[64];

  int data[4];
  memset(data, 0, sizeof(data));

  __cpuid(data, 0);

  if (!data[0] && !data[1] && !data[2] && !data[3])
    return processor_vendor::invalid;

  char vendor[64];
  memset(vendor, 0, sizeof(vendor));

  *reinterpret_cast<int*>(vendor) = data[1];
  *reinterpret_cast<int*>(vendor + 4) = data[3];
  *reinterpret_cast<int*>(vendor + 8) = data[2];

  strcpy_s(buffer, sizeof(buffer), vendor);

  unsigned long hash = CRC::Calculate(buffer, strlen(buffer), CRC::CRC_32());

  switch (hash)
  {
  case 0xa6cfcae7:
    return processor_vendor::intel;
  case 0x6cc10d48:
    return processor_vendor::amd;
  }

  return processor_vendor::none;
}

bool cpuident::query_hypervisor_info(hv_info_t* buffer)
{
  int data[4];
  memset(data, 0, sizeof(data));

  __cpuid(data, 1);

  if (!data[0] && !data[1] && !data[2] && !data[3])
    return false;

  bool present = (data[2] >> 31);

  if (present)
  {
    buffer->present = true;

    memset(data, 0, sizeof(data));

    __cpuid(data, 0x40000000);

    if (!data[0] && !data[1] && !data[2] && !data[3])
      return false;

    char vendor[13];
    memset(vendor, 0, sizeof(vendor));

    memcpy(vendor + 0, &data[1], 4);
    memcpy(vendor + 4, &data[2], 4);
    memcpy(vendor + 8, &data[3], 4);
    vendor[12] = '\0';

    strcpy_s(buffer->vendor_name, vendor);

    unsigned long hash = CRC::Calculate(buffer->vendor_name, strlen(buffer->vendor_name), CRC::CRC_32());

    switch (hash)
    {
    case 0x38a15341:
      buffer->vendor_id = hypervisor_vendor::vmware;
      break;
    }
  }

  return true;
}
