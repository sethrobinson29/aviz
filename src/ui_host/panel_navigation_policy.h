#pragma once

#include <string_view>

namespace panel_navigation_policy {

inline constexpr wchar_t kVirtualHostName[] = L"aviz.local";
inline constexpr wchar_t kStartPage[] = L"https://aviz.local/index.html";
inline constexpr std::wstring_view kTrustedOriginPrefix = L"https://aviz.local/";

bool is_trusted_uri(std::wstring_view uri);

}
