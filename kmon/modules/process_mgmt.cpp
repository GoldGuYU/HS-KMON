#include <include.h>

using namespace kmon;

void c_kmon::on_process_launch(PEPROCESS process)
{
  __noop();
}

void c_kmon::on_process_exit(PEPROCESS process)
{
  const auto id = iw::get_process_id(process);

  process_remove_shadow_pages(id);
}

void c_kmon::process_remove_shadow_pages(_handle id)
{
  auto pages = sh::mgr->find_for_proc(reinterpret_cast<int64_t>(id));

  for (auto page : pages)
  {
    LOG_DEBUG("removing shadow page B=%p", page->get_base());

    page->stop_split(true);
    page->deregister_(true);

    LOG_DEBUG("removed that shadow page.");
  }
}
