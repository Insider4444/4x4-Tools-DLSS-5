#pragma once
#include <string>
namespace adobe_dlss5::updates {
std::string statusText() noexcept;
bool enabled() noexcept;
void setEnabled(bool value) noexcept;
void requestCheck(bool force=false) noexcept;
void openReleasePage() noexcept;
}
