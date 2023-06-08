#pragma once

extern "C"
{
  extern uint64_t ia32_asm_vmx_vmcall(uint64_t rcx, uint64_t rdx, uint64_t r8, uint64_t r9) noexcept;
}
