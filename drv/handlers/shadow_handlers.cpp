#include <include.h>

DECLARE_VMX();

template < typename T >
auto read_type(cr3_t cr3, uintptr_t addr)
{
  cr3_guard _(cr3);
  T result{ };

  __try
  {
    result = *reinterpret_cast<T*>(addr);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    return T{ };
  }

  return result;
}

void start_split_shadow_page(vcpu_t& vp, hvpp::shadow_page_t* full_page) noexcept
{
  HS_ASSERT((full_page->is_splitted == false));

  vp.ept().split_2mb_to_4kb(full_page->page_target.pa & ept_pd_t::mask,
    full_page->page_target.pa & ept_pd_t::mask);

  auto should_map = true;
  auto initial_mem = pa_t{ };
  auto initial_access = epte_t::access_type{ };

  switch (full_page->page_type)
  {
  case page_types::execute_shadow:
  {
    initial_mem = full_page->page_exec.pa;
    initial_access = epte_t::access_type::execute;

    break;
  }
  case page_types::data_shadow:
  {
    should_map = false;

    auto ept_entry = vp.ept().ept_entry(full_page->page_target.pa);

    HS_ASSERT(ept_entry);

    //
    // below access condition cfg translates to NO ACCESS, basically.
    //
    ept_entry->execute_access = false;
    ept_entry->read_access = false;
    ept_entry->write_access = false;

    break;
  }
  default:
    BUG_CHECK_IM();
  }

  if (should_map)
  {
    vp.ept().map_4kb(full_page->page_target.pa, initial_mem,
      initial_access);
  }

  vmx::invept_single_context(vp.ept().ept_pointer());

  full_page->is_splitted = true;
}

void vmexit_custom_handler::on_add_shadow_page(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);

  auto helper = c_shadow_helper(vp.allocator(), &*data.shadow_pages);
  auto page = read_type<hvpp::shadow_page_t>(vp.guest_cr3(), vp.context().rdx);

  if (!helper.add_page(page))
    BUG_CHECK_IM();
}

void vmexit_custom_handler::on_remove_shadow_page(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);

  auto helper = c_shadow_helper(vp.allocator(), &*data.shadow_pages);
  auto page = read_type<hvpp::shadow_page_t>(vp.guest_cr3(), vp.context().rdx);

  if (!helper.remove_page(page))
    BUG_CHECK_IM();
}

void vmexit_custom_handler::on_start_split_shadow_page(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);

  auto helper = c_shadow_helper(vp.allocator(), &*data.shadow_pages);
  auto page = read_type<hvpp::shadow_page_t>(vp.guest_cr3(), vp.context().rdx);

  auto full_page = helper.find_page(page.proc_id, page.page_target);

  if (full_page)
  {
    start_split_shadow_page(vp, full_page);
  }
}

void vmexit_custom_handler::on_start_split_all_shadow_pages_for_process(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);

  auto helper = c_shadow_helper(vp.allocator(), &*data.shadow_pages);
  auto process_id = vp.context().rdx;

  if (process_id)
  {
    auto list = helper.get_pages_for_proc_ex(process_id);

    for (auto _entry = list.first_entry(); _entry != nullptr; _entry = _entry->m_next)
    {
      if (_entry->m_is_head) continue;

      auto sp = _entry->m_data;
      auto sp_ptr = helper.find_page(sp->proc_id, sp->page_target);

      if (sp->is_splitted == false)
      {
        if (sp_ptr)
        {
          start_split_shadow_page(vp, sp_ptr);
        }
        else
        {
          LOG_WARN_SAFE("failed to find sp pointer for sp const reference!");
        }
      }
    }
  }
  else
  {
    LOG_WARN_SAFE("invalid process id was specified (start_split_all)");
  }
}

void vmexit_custom_handler::on_stop_split_shadow_page(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);

  auto helper = c_shadow_helper(vp.allocator(), &*data.shadow_pages);
  auto page = read_type<hvpp::shadow_page_t>(vp.guest_cr3(), vp.context().rdx);

  auto full_page = helper.find_page(page.proc_id, page.page_target);

  if (full_page)
  {
    HS_ASSERT( (full_page->is_splitted == true) );

    vp.ept().join_4kb_to_2mb(full_page->page_target.pa & ept_pd_t::mask,
      full_page->page_target.pa & ept_pd_t::mask);

    vmx::invept_single_context(vp.ept().ept_pointer());
    ia32_asm_inv_page((void*)full_page->page_target.va.ptr());
  }
}

void vmexit_custom_handler::on_add_shadow_page_watcher(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);

  auto helper = c_shadow_helper(vp.allocator(), &*data.shadow_pages);
  auto watcher_info = read_type<hvpp::shadow_watcher_t>(vp.guest_cr3(), vp.context().rdx);

  auto full_page = helper.find_page(watcher_info.pid, watcher_info.target);

  if (full_page)
  {
    HS_ASSERT( full_page->watchers_list.is_full() == false );

    full_page->watchers_list.add(watcher_info);
  }
}

void vmexit_custom_handler::on_remove_shadow_page_watcher(vcpu_t& vp) noexcept
{
  auto& data = user_data(vp);

  auto helper = c_shadow_helper(vp.allocator(), &*data.shadow_pages);
  auto watcher_info = read_type<hvpp::shadow_watcher_t>(vp.guest_cr3(), vp.context().rdx);

  auto full_page = helper.find_page(watcher_info.pid, watcher_info.target);

  if (full_page)
  {
    for (size_t i = 0; i < full_page->watchers_list.count(); i++)
    {
      const auto& current = full_page->watchers_list.at(i);

      full_page->watchers_list.remove(current);
    }
  }
}
