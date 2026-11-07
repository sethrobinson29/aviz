#include "ui_host/panel_source_lint.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace {

const std::filesystem::path kPanelDir{AVIZ_WEB_PANEL_DIR};
const std::filesystem::path kAssetsDir{AVIZ_ASSETS_DIR};

std::string read_all(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    EXPECT_TRUE(file.is_open()) << path;
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

}

TEST(CssColorLintTest, FindsHexInAnyCaseAndLength) {
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: #FFF; background: #1b1b1f; }"),
                ElementsAre("#fff", "#1b1b1f"));
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: #12345678; }"), ElementsAre("#12345678"));
}

TEST(CssColorLintTest, FindsColorFunctionsInAnyCase) {
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: rgb(1, 2, 3); }"), ElementsAre("rgb"));
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: RGBA(1, 2, 3, 0.5); }"), ElementsAre("rgba"));
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: hsl(120 50% 50%); }"), ElementsAre("hsl"));
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: hsla(120, 50%, 50%, 1); }"), ElementsAre("hsla"));
}

TEST(CssColorLintTest, FindsNamedColorsInsideShorthandsAndGradients) {
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: red; }"), ElementsAre("red"));
    EXPECT_THAT(panel_source_lint::css_color_literals("a { border: 1px solid WHITE; }"), ElementsAre("white"));
    EXPECT_THAT(panel_source_lint::css_color_literals("a { background: linear-gradient(90deg, teal, tan); }"),
                ElementsAre("teal", "tan"));
}

TEST(CssColorLintTest, FindsLiteralUsedAsVarFallback) {
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: var(--x, #123456); }"), ElementsAre("#123456"));
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: var(--x, red); }"), ElementsAre("red"));
}

TEST(CssColorLintTest, FindsLiteralInsideMediaBlock) {
    EXPECT_THAT(panel_source_lint::css_color_literals("@media (max-width: 400px) { a { color: #000; } }"),
                ElementsAre("#000"));
}

TEST(CssColorLintTest, FindsLastDeclarationWithoutSemicolon) {
    EXPECT_THAT(panel_source_lint::css_color_literals("a { color: blue }"), ElementsAre("blue"));
}

TEST(CssColorLintTest, IgnoresSelectorsPropertyNamesAndTokenReferences) {
    const char* clean = R"(
        #status.error, .red, .white-card, a:hover {
            color: var(--color-red);
            white-space: nowrap;
            background-color: var(--surface-white);
            border-color: transparent;
            fill: currentColor;
            cursor: grab;
        }
    )";
    EXPECT_THAT(panel_source_lint::css_color_literals(clean), IsEmpty());
}

TEST(JsPatternLintTest, FindsMarkupInjectionSinks) {
    EXPECT_THAT(panel_source_lint::js_forbidden_patterns("el.innerHTML = x;"), ElementsAre("innerHTML"));
    EXPECT_THAT(panel_source_lint::js_forbidden_patterns("el.outerHTML = x;"), ElementsAre("outerHTML"));
    EXPECT_THAT(panel_source_lint::js_forbidden_patterns("el.insertAdjacentHTML('x', y);"),
                ElementsAre("insertAdjacentHTML"));
}

TEST(JsPatternLintTest, FindsSetAttributeStyleWithAnyQuoteAndSpacing) {
    EXPECT_THAT(panel_source_lint::js_forbidden_patterns(R"(el.setAttribute("style", x);)"),
                ElementsAre("setAttribute(style)"));
    EXPECT_THAT(panel_source_lint::js_forbidden_patterns("el.setAttribute( 'style', x);"),
                ElementsAre("setAttribute(style)"));
    EXPECT_THAT(panel_source_lint::js_forbidden_patterns("el.setAttribute (`style`, x);"),
                ElementsAre("setAttribute(style)"));
}

TEST(JsPatternLintTest, AllowsSafeWrites) {
    const char* clean = R"(
        el.textContent = x;
        el.style.setProperty("--p", "0.5");
        el.setAttribute("aria-label", "Layer name");
        el.setAttribute("styled", "yes");
    )";
    EXPECT_THAT(panel_source_lint::js_forbidden_patterns(clean), IsEmpty());
}

TEST(PanelSourceLintTest, AppCssHasNoLiteralColors) {
    EXPECT_THAT(panel_source_lint::css_color_literals(read_all(kPanelDir / "app.css")), IsEmpty());
}

TEST(PanelSourceLintTest, AppJsHasNoForbiddenPatterns) {
    EXPECT_THAT(panel_source_lint::js_forbidden_patterns(read_all(kPanelDir / "app.js")), IsEmpty());
}

TEST(PanelSourceLintTest, EveryTokenAppCssUsesIsDefinedByTheThemeOrSetByAppJs) {
    const std::string theme = read_all(kPanelDir / "themes" / "dark-y2k.css");
    const std::string js = read_all(kPanelDir / "app.js");
    const std::vector<std::string> used = panel_source_lint::css_custom_properties_used(read_all(kPanelDir / "app.css"));

    EXPECT_FALSE(used.empty());
    for (const std::string& name : used) {
        const bool defined_by_theme = theme.find(name + ":") != std::string::npos;
        const bool set_by_app_js = js.find("setProperty(\"" + name + "\"") != std::string::npos;
        EXPECT_TRUE(defined_by_theme || set_by_app_js) << name;
    }
}

TEST(PanelSourceLintTest, SliderFillPropertyIsSetOnlyByAppJsNotByTheTheme) {
    const std::string theme = read_all(kPanelDir / "themes" / "dark-y2k.css");
    const std::string js = read_all(kPanelDir / "app.js");

    EXPECT_EQ(theme.find("--p:"), std::string::npos);
    EXPECT_NE(js.find("setProperty(\"--p\""), std::string::npos);
}

TEST(PanelSourceLintTest, ThemeLoadsBeforeAppCss) {
    const std::string html = read_all(kPanelDir / "index.html");
    const std::size_t theme = html.find(R"(href="themes/dark-y2k.css")");
    const std::size_t app = html.find(R"(href="app.css")");

    ASSERT_NE(theme, std::string::npos);
    ASSERT_NE(app, std::string::npos);
    EXPECT_LT(theme, app);
}

TEST(PanelSourceLintTest, ContentSecurityPolicyIsUnchanged) {
    EXPECT_NE(read_all(kPanelDir / "index.html")
                  .find("default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self'; connect-src 'self'; "
                        "object-src 'none'; base-uri 'none'; form-action 'none';"),
              std::string::npos);
}

TEST(PanelSourceLintTest, PanelLoadsNothingOffBox) {
    for (const char* file : {"index.html", "app.css", "themes/dark-y2k.css"}) {
        const std::string text = read_all(kPanelDir / file);
        EXPECT_EQ(text.find("http:"), std::string::npos) << file;
        EXPECT_EQ(text.find("https:"), std::string::npos) << file;
        EXPECT_EQ(text.find("@import"), std::string::npos) << file;
    }
}

TEST(PanelSourceLintTest, BundledFontsShipWithTheirLicenses) {
    EXPECT_TRUE(std::filesystem::exists(kPanelDir / "fonts" / "inter-latin-wght-normal.woff2"));
    EXPECT_TRUE(std::filesystem::exists(kPanelDir / "fonts" / "OFL.txt"));
    EXPECT_TRUE(std::filesystem::exists(kAssetsDir / "fonts" / "Inter-SemiBold-ascii.ttf"));
    EXPECT_TRUE(std::filesystem::exists(kAssetsDir / "fonts" / "OFL.txt"));
}
