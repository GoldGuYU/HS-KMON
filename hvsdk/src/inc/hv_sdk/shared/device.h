#pragma once

class c_device
{
public:
  c_device() = default;
  ~c_device()
  {
    if (m_handle)
      CloseHandle(m_handle);
  }

  c_device(const std::string& name)
  {
    if (name.empty())
      return;

    const auto path = fmt::format("\\\\.\\{}", name);

    m_handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);

    if (m_handle == INVALID_HANDLE_VALUE)
    {
      printf("last_error => %i\n ", GetLastError());
      return;
    }

    m_valid = true;
  }

  auto valid() const { return m_valid; }
  auto handle() const { return m_handle; }

  template <typename IB, typename OB >
  NTSTATUS send_control(DWORD code, IB in, OB out, DWORD* rl = nullptr)
  {
    return DeviceIoControl(m_handle, code, in,
      sizeof(in), out, sizeof(out), rl, nullptr);
  }

  template <typename IB, typename OB >
  NTSTATUS send_control(DWORD code, IB in, DWORD in_length, OB out, DWORD out_length, DWORD* rl = nullptr)
  {
    return DeviceIoControl(m_handle, code, in,
      in_length, out, out_length, rl, nullptr);
  }
private:
  bool m_valid = false;
  HANDLE m_handle = nullptr;
};
