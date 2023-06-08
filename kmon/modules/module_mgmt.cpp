#include <include.h>

using namespace module_;

void manager::on_driver_load(c_system_modules::module_entry_t& driver_info)
{
  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head) continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    entry->on_driver_load(driver_info);
  }
}

void manager::on_driver_shut(c_system_modules::module_entry_t& driver_info)
{
  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head) continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    entry->on_driver_shut(driver_info);
  }
}

void manager::on_create_device(
  PDRIVER_OBJECT pDriverObject, ULONG uDeviceExtensionSize,
  PUNICODE_STRING pszDeviceName, ULONG uDeviceType,
  ULONG uDeviceCharacteristics, BOOLEAN bExclusive,
  PDEVICE_OBJECT* pDeviceObject
)
{
  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    entry->on_create_device(
      pDriverObject, uDeviceExtensionSize,
      pszDeviceName, uDeviceType,
      uDeviceCharacteristics, bExclusive,
      pDeviceObject
    );
  }
}

void manager::on_process_open(
  PHANDLE            ProcessHandle,
  ACCESS_MASK        DesiredAccess,
  POBJECT_ATTRIBUTES ObjectAttributes,
  PCLIENT_ID         ClientId
)
{
  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    entry->on_process_open(
      ProcessHandle,
      DesiredAccess,
      ObjectAttributes,
      ClientId
    );
  }
}

bool manager::on_copy_memory(void* target_address, MM_COPY_ADDRESS source_address,
  size_t number_of_bytes, u_long uFlags, size_t* number_of_bytes_transferred,
  intercept_info_t& ici
)
{
  auto result = true;

  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    if (!entry->on_copy_memory(
      target_address, source_address,
      number_of_bytes, uFlags,
      number_of_bytes_transferred,
      ici))
    {
      result = false;
      break;
    }
  }

  return result;
}

bool manager::on_register_nmi_callback(
  PNMI_CALLBACK Routine, PVOID Context,
  intercept_info_t& ici
)
{
  auto result = true;

  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    if (!entry->on_register_nmi_callback(Routine, Context, ici))
    {
      result = false;
      break;
    }
  }

  return result;
}

bool manager::on_deregister_nmi_callback(
  PVOID Handle,
  intercept_info_t& ici
)
{
  auto result = true;

  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    if (!entry->on_deregister_nmi_callback(Handle, ici))
    {
      result = false;
      break;
    }
  }

  return result;
}

bool manager::on_send_nmi(void* unk_0, intercept_info_t& ici)
{
  auto result = true;

  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    if (!entry->on_send_nmi(unk_0, ici))
    {
      result = false;
      break;
    }
  }

  return result;
}

bool manager::on_ipi_generic_call(PKIPI_BROADCAST_WORKER BroadcastFunction, ULONG_PTR Context, intercept_info_t& ici)
{
  auto result = true;

  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    if (!entry->on_ipi_generic_call(BroadcastFunction, Context, ici))
    {
      result = false;
      break;
    }
  }

  return result;
}

bool manager::on_call_pre_op_callbacks(
  POBJECT_TYPE pObjectType,
  POB_PRE_OPERATION_INFORMATION pOperationInformation,
  DWORD_PTR* a3, intercept_info_t& ici
)
{
  auto result = true;

  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    if (!entry->on_call_pre_op_callbacks(
      pObjectType,
      pOperationInformation,
      a3, ici
    ))
    {
      result = false;
      break;
    }
  }

  return result;
}

bool manager::on_call_post_op_callback(
  POB_POST_OPERATION_INFORMATION pOperationInformation,
  DWORD_PTR* a3, intercept_info_t& ici
)
{
  auto result = true;

  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    if (!entry->on_call_post_op_callback(
      pOperationInformation,
      a3, ici
    ))
    {
      result = false;
      break;
    }
  }

  return result;
}

void manager::on_alloc_virtual_memory(
  HANDLE    ProcessHandle,
  PVOID* BaseAddress,
  ULONG_PTR ZeroBits,
  PSIZE_T   RegionSize,
  ULONG     AllocationType,
  ULONG     Protect,
  intercept_info_t& ici)
{
  for (auto* data = m_list.first_entry(); data != nullptr; data = data->m_next)
  {
    if (data->m_is_head)
      continue;

    auto& entry = data->m_data;

    if (!entry->active())
      continue;

    entry->on_alloc_virtual_memory(
      ProcessHandle,
      BaseAddress,
      ZeroBits,
      RegionSize,
      AllocationType,
      Protect,
      ici
    );
  }
}
