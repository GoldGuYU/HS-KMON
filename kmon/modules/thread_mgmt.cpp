#include <include.h>

using namespace kmon;

void c_kmon::on_thread_launch(PETHREAD thread)
{
  __noop();
}

void c_kmon::on_thread_exit(PETHREAD thread)
{
  __noop();
}
