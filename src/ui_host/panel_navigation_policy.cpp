#include "ui_host/panel_navigation_policy.h"

namespace panel_navigation_policy {

bool is_trusted_uri(std::wstring_view uri) {
    return uri.size() >= kTrustedOriginPrefix.size() &&
           uri.compare(0, kTrustedOriginPrefix.size(), kTrustedOriginPrefix) == 0;
}

}
