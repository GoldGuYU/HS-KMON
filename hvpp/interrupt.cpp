#include <ntddk.h>
#include <ntimage.h>
#include "hvpp/hvpp.h"
#include "hvpp/hypervisor.h"
#include "hvpp/config.h"

#include "hvpp/ia32/win32/exception.h"
#include "hvpp/ia32/cpuid/cpuid_eax_01.h"
#include "hvpp/lib/assert.h"
#include "hvpp/lib/driver.h"
#include "hvpp/lib/log.h"
#include "hvpp/lib/mm.h"
#include "hvpp/lib/mp.h"
#include "hvpp/lib/defs.h"
#include "hvpp/interrupt.h"

#include "hvpp/global.h"

DECLARE_VMX();

using namespace hvpp;

extern "C"
{
  void memes(u64 addr)
  {
    if (addr) { __noop(); }
    __halt();
  }
  void interrupt_handle_pf( idt_registers_ecode_t* reg )
  {
    memes(reg->rip);
  }
}
