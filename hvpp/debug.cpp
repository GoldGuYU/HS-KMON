#include "hvpp/debug.h"

#include <stdarg.h>

__int16 dbg::get_port_for_curr_cpu()
{
  auto cpu_idx = mp::cpu_index();

  if (cpu_idx >= ARRAYSIZE(port_number))
    return -1;

  return port_number[cpu_idx];
}

auto dbg::debug_print_decimal(long long number) -> void
{
  auto port_nr = get_port_for_curr_cpu();
  if (port_nr == -1) return;

  if (number < 0)
  {
    __outbyte(port_nr, '-');
    number = -number;
  }

  for (auto d = 1000000000000000000; d != 0; d /= 10)
  {
    if ((number / d) != 0)
      __outbyte(port_nr, alphabet[(number / d) % 10]);
  }
}

auto dbg::debug_print_hex(u64 number, const bool show_zeros) -> void
{
  auto port_nr = get_port_for_curr_cpu();
  if (port_nr == -1) return;

  for (auto d = 0x1000000000000000; d != 0; d /= 0x10)
  {
    if (show_zeros || (number / d) != 0)
      __outbyte(port_nr, alphabet[(number / d) % 0x10]);
  }
}

auto dbg::internl::println(const char* format, va_list args) -> void
{
  auto port_nr = get_port_for_curr_cpu();
  if (port_nr == -1) return;

  while (format[0])
  {
    if (format[0] == '%')
    {
      switch (format[1])
      {
      case 'd':
      case 'i':
        debug_print_decimal(va_arg(args, int));
        format += 2;
        continue;
      case 'x':
        debug_print_hex(va_arg(args, u32), false);
        format += 2;
        continue;
      case 'l':
        if (format[2] == 'l')
        {
          switch (format[3])
          {
          case 'd':
            debug_print_decimal(va_arg(args, u64));
            format += 4;
            continue;
          case 'x':
            debug_print_hex(va_arg(args, u64), false);
            format += 4;
            continue;
          }
        }
        break;
      case 'p':
        debug_print_hex(va_arg(args, u64), true);
        format += 2;
        continue;
      }
    }
    __outbyte(port_nr, format[0]);
    ++format;
  }
}

auto dbg::println(const char* format, ...) -> void
{
  //mm::my_lock_guard _(&_lck);
  va_list args;

  va_start(args, format);
  internl::println(format, args);
  va_end(args);
}

auto dbg::print(const char* format, ...) -> void
{
  //mm::my_lock_guard _(&_lck);
  va_list args;

  va_start(args, format);
  internl::println(format, args);
  va_end(args);

  internl::println("\r\n", nullptr);
}

auto dbg::printp(const char* prefix, const char* format, ...) -> void
{
  //mm::my_lock_guard _(&_lck);
  va_list args;

  internl::nl::println(_XS("[cpu%i]"), mp::cpu_index());
  internl::nl::println(prefix);

  va_start(args, format);
  internl::println(format, args);
  va_end(args);

  internl::nl::println("\r\n");
}
