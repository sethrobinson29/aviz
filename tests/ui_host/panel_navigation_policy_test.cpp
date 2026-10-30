#include "ui_host/panel_navigation_policy.h"

#include <string>

#include <gtest/gtest.h>

TEST(PanelNavigationPolicyTest, TrustsTheStartPageAndOtherContentOnThePanelVirtualHost) {
    EXPECT_TRUE(panel_navigation_policy::is_trusted_uri(panel_navigation_policy::kStartPage));
    EXPECT_TRUE(panel_navigation_policy::is_trusted_uri(L"https://aviz.local/"));
    EXPECT_TRUE(panel_navigation_policy::is_trusted_uri(L"https://aviz.local/app.js"));
    EXPECT_TRUE(panel_navigation_policy::is_trusted_uri(L"https://aviz.local/app.css?v=2#top"));
}

TEST(PanelNavigationPolicyTest, RejectsExternalHosts) {
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"https://example.com/"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"https://example.com/index.html"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"http://example.com/"));
}

TEST(PanelNavigationPolicyTest, RejectsLookalikeHostsThatOnlyStartWithThePanelHostName) {
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"https://aviz.local.evil.com/"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"https://aviz.localhost/"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"https://aviz.local:8443/"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"https://aviz.local@evil.com/"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"https://aviz.local"));
}

TEST(PanelNavigationPolicyTest, RejectsExternalUrlThatEmbedsTheTrustedPrefixLater) {
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"https://evil.com/https://aviz.local/index.html"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"https://evil.com/?next=https://aviz.local/"));
}

TEST(PanelNavigationPolicyTest, RejectsThePanelHostOverANonHttpsScheme) {
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"http://aviz.local/index.html"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"ftp://aviz.local/index.html"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"HTTPS://AVIZ.LOCAL/index.html"));
}

TEST(PanelNavigationPolicyTest, RejectsLocalAndInlineSchemes) {
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"file:///C:/Windows/win.ini"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"data:text/html,<script>alert(1)</script>"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"javascript:alert(1)"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"about:blank"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L"blob:https://aviz.local/1234"));
}

TEST(PanelNavigationPolicyTest, RejectsEmptyAndLeadingWhitespaceUris) {
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L""));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(L" https://aviz.local/index.html"));
    EXPECT_FALSE(panel_navigation_policy::is_trusted_uri(std::wstring(L"\0https://aviz.local/", 20)));
}
