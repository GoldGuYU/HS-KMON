#pragma once

namespace mem
{
  FORCEINLINE DWORD_PTR FindInMemory(PVOID pHayStack, PVOID pNeedle, SIZE_T uHayStackLength, SIZE_T uNeedleLength)
  {
    if (pHayStack == nullptr || pNeedle == nullptr)
      return 0;

    if (uHayStackLength <= NULL || uNeedleLength <= NULL)
      return 0;

    for (auto* pBuffer = static_cast<UINT8*>(pHayStack); uHayStackLength >= uNeedleLength; ++pBuffer, --uHayStackLength)
    {
      if (RtlCompareMemory(pBuffer, pNeedle, uNeedleLength) == uNeedleLength)
        return reinterpret_cast<DWORD_PTR>(pBuffer);
    }

    return 0;
  }

  struct range {
    uintptr_t m_begin{};
    uintptr_t m_end{};
    uintptr_t m_size{};
  };

  class addr
  {
  public:
    addr() = default;
    ~addr() = default;

    bool operator==(const DWORD_PTR& other) const
    {
      return (m_base == other);
    }

    bool operator==(const DWORD_PTR& lhs)
    {
      return (lhs == m_base);
    }

    addr(const DWORD_PTR base)
    {
      m_base = base;
    }

    addr(void* const ptr)
    {
      m_base = reinterpret_cast<DWORD_PTR>(ptr);
    }

    [[nodiscard]] auto Get() const
    {
      return addr(m_base);
    }

    [[nodiscard]] auto Base() const
    {
      return m_base;
    }

    [[nodiscard]] auto Valid() const
    {
      return !(m_base == 0);
    }

    [[nodiscard]] auto Add(const DWORD_PTR t) const
    {
      return addr(m_base + t);
    }

    [[nodiscard]] auto Sub(const DWORD_PTR t) const
    {
      return addr(m_base - t);
    }

    [[nodiscard]] auto Deref() const
    {
      return addr(*reinterpret_cast<DWORD_PTR*>(m_base));
    }

    [[nodiscard]] auto Ptr() const
    {
      return reinterpret_cast<PVOID>(m_base);
    }

    template < class T >
    [[nodiscard]] auto As()
    {
      return reinterpret_cast<T>(m_base);
    }

    template < class T >
    [[nodiscard]] auto Retrieve_as()
    {
      return *reinterpret_cast<T*>(m_base);
    }

    template < class T >
    void Set(const T& value)
    {
      *As<T*>() = value;
    }
  private:
    DWORD_PTR m_base;
  };

  class bare_pointer
  {
  public:
    bare_pointer(void* const ptr)
    {
      m_instance = ptr;
    }

    [[nodiscard]] addr base() const
    {
      return addr(m_instance);
    }

    [[nodiscard]] auto deref() const
    {
      return *static_cast<void**>(m_instance);
    }
  private:
    void* m_instance;
  };

  INLINE DWORD_PTR ResolveAddressToRelative(DWORD_PTR dwInitial, DWORD_PTR dwInstructionOffset, DWORD_PTR dwPostOffset)
  {
    if (dwInitial == NULL)
      return NULL;

    CONST AUTO dwRelativeAddyOffset = (dwInitial + dwInstructionOffset);
    CONST AUTO dwRelativeAddy = *reinterpret_cast<LONG*>(dwRelativeAddyOffset);
    CONST AUTO dwRelativeResult = dwInitial + dwRelativeAddy;

    if (dwPostOffset != NULL)
      return dwRelativeResult + dwPostOffset;

    return dwRelativeResult;
  }
}

#define RESOLVE_ADDRESS_TO_RELATIVE(INITIAL, INSTR_OFFS, OPCODE_LENGTH) mem::ResolveAddressToRelative(INITIAL, INSTR_OFFS, OPCODE_LENGTH)
