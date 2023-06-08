#include <include.h>

using namespace kmon;

void c_kmon::on_query_value_key(
  KEY_VALUE_INFORMATION_CLASS info_class, PKEY_NAME_INFORMATION key_name,
  PUNICODE_STRING value_name, PVOID info_buffer, SIZE_T info_length
)
{
  if (!info_buffer || !info_length)
    return;

  switch (info_class)
  {
  case KeyValuePartialInformation:
  {
    auto* const pInfo = mem::addr(info_buffer).As<PKEY_VALUE_PARTIAL_INFORMATION>();

    if (pInfo)
    {
      if (wcsstr(key_name->Name, _XSW(LR"(\REGISTRY\MACHINE\BCD00000000\Objects\)")) &&
        util::EqualStringU(value_name, _XSW(L"Element")))
      {
        LOG_WARN("[!] unsetting the testsign BCD element.");

        *(uint8_t*)(pInfo->Data) = 0;
      }
      else if (wcsstr(key_name->Name, _XSW(LR"(\REGISTRY\MACHINE\SYSTEM\ControlSet001\Control)")) &&
        util::EqualStringU(value_name, _XSW(L"SystemStartOptions")))
      {
        LOG_WARN("[!] removing testsign startup options.");

        static CHAR pszSafeData[25] = { 0 };

        if (pszSafeData[0] == NULL)
          strcpy_s(pszSafeData, _XS("NOEXECUTE=OPTIN  NODEBUG"));

        memset(pInfo->Data, 0, pInfo->DataLength);
        memcpy(pInfo->Data, pszSafeData, sizeof(pszSafeData));

        pInfo->DataLength = sizeof(pszSafeData);
      }
    }

    break;
  }
  default:
    break;
  }
}
