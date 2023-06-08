#include <include.h>

void vmexit_handler::handle(vcpu_t& vp) noexcept
{
  const auto handler_index = static_cast<int>(vp.exit_reason());
  (this->*handlers_[handler_index])(vp);
}
