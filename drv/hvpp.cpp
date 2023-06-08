#include <include.h>

struct delta_entry_t
{
  u8 val_b1;
  u8 val_b2;
  u64 offset;
};

auto build_delta_table(vcpu_t& vp, u8* b1, u8* b2, u32 len)
{
  auto result = stl::vmxr::c_linked_list<delta_entry_t>(&vp.allocator(), false);

  for (u32 i = 0; i < len; i++)
  {
    if (b1[i] != b2[i])
    {
      delta_entry_t e;
      RtlZeroMemory(&e, sizeof(e));

      e.offset = BASE_OF(&b1[i]) - BASE_OF(b1);
      e.val_b1 = b1[i];
      e.val_b2 = b2[i];

      result.insert(e);
    }
  }

  return result;
}

void log_ept_violation(vcpu_t& vp, ia32::vmx::exit_qualification_ept_violation_t exit_qual)
{
  const char* access = nullptr;

  if (exit_qual.data_read) access = "r";
  else if (exit_qual.data_write) access = "w";
  else if (exit_qual.data_execute) access = "x";
  else access = "n/a";

  LOG_INFO_SAFE(
    "ept violation for code shadow (VA=%p, PA=%p, A=%s)",
    vp.exit_guest_linear_address().value(),
    vp.exit_guest_physical_address().value(),
    access
  );
}

void hvpp::break_in_debugger()
{
  ia32_asm_vmx_vmcall(0xC3, 0, 0, 0);
}

void hvpp::invalidate_address(void* va) noexcept
{
  if (va == nullptr)
    return;

  auto* page = page_align(va);
  ia32_asm_inv_page(page);

  LOG_DEBUG_SAFE("invalidate_address(%p) -> %p", va, page);
}

bool hvpp::is_privileged_call(uintptr_t rip) noexcept
{
  const auto my_base = mem::addr(driver::image_base).Base();
  const auto my_length = driver::image_length;
  const auto my_end = mem::addr(my_base).Add(my_length).Base();

  if (!my_base || !my_length)
    return false;

  return (rip >= my_base && rip <= my_end);
}

void hvpp::handle_ept_viol_exec_shadow(
  vcpu_t& vp, shadow_page_t* full_page,
  ia32::vmx::exit_qualification_ept_violation_t exit_qual,
  mtf_context_t& mtf_ctx
) noexcept
{
  if (exit_qual.data_read || exit_qual.data_write)
  {
    vp.ept().map_4kb(
      full_page->page_target.pa,
      full_page->page_read.pa,
      epte_t::access_type::read_write
    );
  }
  else if (exit_qual.data_execute)
  {
    vp.ept().map_4kb(
      full_page->page_target.pa,
      full_page->page_exec.pa,
      epte_t::access_type::execute
    );
  }
  else
  {
    _dbg("unexpected exit quali for page with type execute_shadow.");
    BUG_CHECK_IM();
  }
}

void hvpp::handle_ept_viol_data_shadow(
  vcpu_t& vp, shadow_page_t* full_page,
  ia32::vmx::exit_qualification_ept_violation_t exit_qual,
  mtf_context_t& mtf_ctx
) noexcept
{
  auto guest_pa = vp.exit_guest_physical_address();
  auto guest_va = vp.exit_guest_linear_address();

  for (size_t i = 0; i < full_page->watchers_list.count(); i++)
  {
    const auto& watcher = full_page->watchers_list.at(i);

    if (guest_va >= watcher.base && guest_va < (watcher.base + watcher.len))
    {
      const char* access_string = "N/A";

      if (exit_qual.data_read)
        access_string = "r";
      else if (exit_qual.data_write)
        access_string = "w";

      auto is_within_nt = vp.guest_rip() >= g::ntoskrnl.Base &&
        vp.guest_rip() < (g::ntoskrnl.Base + g::ntoskrnl.Length);

      if (!is_within_nt)
      {
        LOG_INFO_SAFE("data access at=%p, by RIP=%p (%s)",
          guest_va.ptr(), vp.guest_rip(), access_string
        );
      }
    }
  }

  auto ept_entry = vp.ept().ept_entry(full_page->page_target.pa);

  HS_ASSERT(ept_entry);

  if (exit_qual.data_read || exit_qual.data_write)
  {
    ept_entry->read_access = true;
    ept_entry->write_access = true;

    mtf_ctx.awaiting_exit = true;
    mtf_ctx.operation = mtf_operations::restore_no_access;
    mtf_ctx.optional.shadow_page = *full_page;

    vp.guest_mtf(true);
  }
  else
  {
    _dbg("unexpected exit quali for page with type data_shadow.");
    BUG_CHECK_IM();
  }
}

bool hvpp::prepare_page(
  hvpp::shadow_page_t* page_info, page_types page_type,
  int process_id, const void* page)
{
  bool requires_copy_pages = true;
  PageCopy_t read_page, write_page, exec_page;

  RtlZeroMemory(&read_page, sizeof(PageCopy_t));
  RtlZeroMemory(&write_page, sizeof(PageCopy_t));
  RtlZeroMemory(&exec_page, sizeof(PageCopy_t));

  if (process_id == 4)
  {
    //
    // Kernel Page
    //

    switch (page_type)
    {
    case page_types::data_shadow:
      requires_copy_pages = false;
      break;
    }

    if (requires_copy_pages)
    {
      if (!util::CreatePageCopyEx(page, read_page) ||
        !util::CreatePageCopyEx(page, write_page) ||
        !util::CreatePageCopyEx(page, exec_page))
        return false;
    }

    page_info->page_target.pa = pa_t::from_va(page);
  }
  else
  {
    //
    // User Page
    //

    // TODO: Adjust...
    DEBUG_BREAK();
  }

  page_info->proc_id = process_id;
  page_info->page_target.va = page;
  page_info->page_type = page_type;

  if (requires_copy_pages)
  {
    page_info->page_read.va = read_page.base_aligned;
    page_info->page_write.va = write_page.base_aligned;
    page_info->page_exec.va = exec_page.base_aligned;

    page_info->page_read.pa = pa_t::from_va(page_info->page_read.va.ptr());
    page_info->page_write.pa = pa_t::from_va(page_info->page_write.va.ptr());
    page_info->page_exec.pa = pa_t::from_va(page_info->page_exec.va.ptr());

    if (!page_info->page_read.va || !page_info->page_write.va || !page_info->page_exec.va)
      return false;

    if (!page_info->page_read.pa || !page_info->page_write.pa || !page_info->page_exec.pa)
      return false;
  }

  return true;
}

void mtf::restore_no_access(vcpu_t& vp, mtf_context_t& mtf_ctx) noexcept
{
  auto ept_entry = vp.ept().ept_entry(mtf_ctx.optional.shadow_page.page_target.pa);

  HS_ASSERT(ept_entry);

  ept_entry->read_access = false;
  ept_entry->write_access = false;

  vmx::invept_single_context(vp.ept().ept_pointer());
}

void mtf::track_exec_page_write(vcpu_t& vp, mtf_context_t& mtf_ctx) noexcept
{
  auto full_page = mtf_ctx.optional.shadow_page;

  LOG_DEBUG_SAFE("track_exec_page_write(VA=%p)", full_page.page_target.va);

  auto exec_buffer = reinterpret_cast<u8*>(full_page.page_exec.va.value());
  auto write_buffer = reinterpret_cast<u8*>(full_page.page_write.va.value());

  auto diff_table = build_delta_table(vp, exec_buffer, write_buffer, page_size);

  LOG_DEBUG_SAFE("differences detected => %i", diff_table.count());

  for (auto p = diff_table.first_entry(); p != nullptr; p = p->m_next)
  {
    if (p->m_is_head) continue;

    auto entry = p->m_data;

    LOG_DEBUG_SAFE("+0x%02x b1=%02x b2=%02x", entry.offset, entry.val_b1, entry.val_b2);
  }

  diff_table.destroy();

  vp.ept().map_4kb(
    full_page.page_target.pa,
    full_page.page_exec.pa,
    epte_t::access_type::execute
  );

  vmx::invept_single_context(vp.ept().ept_pointer());
}
