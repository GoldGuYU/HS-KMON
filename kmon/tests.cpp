#include <include.h>

bool tests::run()
{
  //routines.insert({ _XS("check_lstar"), &routine::check_lstar });

  auto num_total_tests = routines.count();
  auto num_passed_tests = 0ull;
  auto num_failed_tests = 0ull;

  LOG_INFO("running tests...");

  for (auto i = 0ull; i < routines.count(); i++)
  {
    auto entry = routines[i];

    if (entry == nullptr)
      continue;

    auto status = entry->routine();

    if (NT_SUCCESS(status))
    {
      LOG_INFO("test #%i with name ('%s') has passed.", i, entry->name);

      ++num_passed_tests;
    }
    else
    {
      LOG_INFO("test #%i with name ('%s') has failed.", i, entry->name);

      ++num_failed_tests;
    }
  }

  LOG_INFO("finished tests!");

  return !num_failed_tests && (num_passed_tests == num_total_tests);
}

void tests::shmm_sample_access_notify()
{

}

void tests::shmm_sample_data_shadow()
{

}
