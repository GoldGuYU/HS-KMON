#include "hvpp/vcpu.h"
#include "hvpp/vmexit.h"

#include "hvpp/lib/assert.h"
#include "hvpp/lib/log.h"
#include "hvpp/lib/mm.h"
#include "hvpp/lib/cr3_guard.h"
#include "hvpp/lib/defs.h"

#include "hvpp/global.h"

#include "hvpp/idt.h"

DECLARE_VMX();

ia32::idt_entry_t idt::create_entry(void* handler)
{
  ia32::idt_entry_t result{};

  memset(&result, 0, sizeof(result));

  result.selector.flags = ia32_asm_read_cs();

  result.access.present = true;
  result.access.type = 0xE; // SEGMENT_DESCRIPTOR_TYPE_INTERRUPT_GATE

  result.base_address(handler);

  return result;
}
