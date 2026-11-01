#include "compositor/layer_name.h"

#include <cstddef>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

std::string utf8(char32_t code_point) {
    std::string out;
    if (code_point < 0x80) {
        out.push_back(static_cast<char>(code_point));
    } else if (code_point < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else if (code_point < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    return out;
}

std::string repeated(char32_t code_point, std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count; ++i) {
        out += utf8(code_point);
    }
    return out;
}

std::string bytes(std::initializer_list<int> values) {
    std::string out;
    for (int value : values) {
        out.push_back(static_cast<char>(value));
    }
    return out;
}

}

TEST(LayerNameTest, EmptyStaysEmpty) { EXPECT_EQ(layer_name::normalized(""), std::string()); }

TEST(LayerNameTest, OrdinaryNameIsKeptAsIs) {
    EXPECT_EQ(layer_name::normalized("Kick pulse"), std::string("Kick pulse"));
}

TEST(LayerNameTest, FiftyCodePointsAcceptedAtEveryEncodedWidth) {
    for (char32_t code_point : {U'a', char32_t{0xE9}, char32_t{0x65E5}, char32_t{0x1F600}}) {
        const std::string name = repeated(code_point, 50);
        EXPECT_EQ(layer_name::normalized(name), name) << static_cast<unsigned long>(code_point);
    }
}

TEST(LayerNameTest, FiftyOneCodePointsRejectedAtEveryEncodedWidth) {
    for (char32_t code_point : {U'a', char32_t{0xE9}, char32_t{0x65E5}, char32_t{0x1F600}}) {
        EXPECT_FALSE(layer_name::normalized(repeated(code_point, 51)).has_value())
            << static_cast<unsigned long>(code_point);
    }
}

TEST(LayerNameTest, FiftyFourByteCodePointsReachExactlyTwoHundredBytes) {
    const std::string name = repeated(0x1F600, 50);
    ASSERT_EQ(name.size(), 200u);
    EXPECT_TRUE(layer_name::normalized(name).has_value());
}

TEST(LayerNameTest, CodePointsAtEncodingWidthBoundariesAreAccepted) {
    for (char32_t code_point : {char32_t{0xA1}, char32_t{0x7FF}, char32_t{0x800}, char32_t{0xD7FF}, char32_t{0xE000},
                                char32_t{0xFFFF}, char32_t{0x10000}, char32_t{0x10FFFF}}) {
        const std::string name = "a" + utf8(code_point) + "b";
        EXPECT_EQ(layer_name::normalized(name), name) << static_cast<unsigned long>(code_point);
    }
}

TEST(LayerNameTest, EveryC0ControlIsRejectedIncludingEmbeddedNul) {
    for (char32_t code_point = 0; code_point <= 0x1F; ++code_point) {
        EXPECT_FALSE(layer_name::normalized("a" + utf8(code_point) + "b").has_value())
            << static_cast<unsigned long>(code_point);
    }
}

TEST(LayerNameTest, DeleteAndEveryC1ControlAreRejected) {
    for (char32_t code_point = 0x7F; code_point <= 0x9F; ++code_point) {
        EXPECT_FALSE(layer_name::normalized("a" + utf8(code_point) + "b").has_value())
            << static_cast<unsigned long>(code_point);
    }
}

TEST(LayerNameTest, BidiControlsAreRejected) {
    for (char32_t code_point = 0x202A; code_point <= 0x202E; ++code_point) {
        EXPECT_FALSE(layer_name::normalized("a" + utf8(code_point) + "b").has_value())
            << static_cast<unsigned long>(code_point);
    }
    for (char32_t code_point = 0x2066; code_point <= 0x2069; ++code_point) {
        EXPECT_FALSE(layer_name::normalized("a" + utf8(code_point) + "b").has_value())
            << static_cast<unsigned long>(code_point);
    }
}

TEST(LayerNameTest, NeighborsOfForbiddenRangesAreAccepted) {
    for (char32_t code_point : {U'~', char32_t{0xA0}, char32_t{0x2029}, char32_t{0x202F}, char32_t{0x2065},
                                char32_t{0x206A}}) {
        const std::string name = "a" + utf8(code_point) + "b";
        EXPECT_EQ(layer_name::normalized(name), name) << static_cast<unsigned long>(code_point);
    }
}

TEST(LayerNameTest, ForbiddenCodePointInsideOtherwiseTrimmedPaddingIsStillRejected) {
    EXPECT_FALSE(layer_name::normalized(" " + utf8(0x202E) + " ").has_value());
}

TEST(LayerNameTest, TrimsEveryListedWhitespaceFromBothEnds) {
    for (char32_t code_point :
         {U' ', char32_t{0xA0}, char32_t{0x1680}, char32_t{0x2000}, char32_t{0x200A}, char32_t{0x2028},
          char32_t{0x2029}, char32_t{0x202F}, char32_t{0x205F}, char32_t{0x3000}}) {
        const std::string padding = utf8(code_point);
        EXPECT_EQ(layer_name::normalized(padding + "name" + padding), std::string("name"))
            << static_cast<unsigned long>(code_point);
    }
}

TEST(LayerNameTest, InteriorWhitespaceIsKept) {
    EXPECT_EQ(layer_name::normalized("  a  b  "), std::string("a  b"));
}

TEST(LayerNameTest, NameOfOnlyWhitespaceBecomesEmpty) {
    EXPECT_EQ(layer_name::normalized("   " + utf8(0x3000)), std::string());
}

TEST(LayerNameTest, LengthIsMeasuredAfterTrimming) {
    const std::string name = repeated(U'a', 50);
    EXPECT_EQ(layer_name::normalized("   " + name + "   "), name);
}

TEST(LayerNameTest, TabAndNewlineAreRejectedNotTrimmed) {
    EXPECT_FALSE(layer_name::normalized(utf8(0x09) + "name").has_value());
    EXPECT_FALSE(layer_name::normalized("name" + utf8(0x0A)).has_value());
}

TEST(LayerNameTest, ZeroWidthAndByteOrderMarkAreNotTreatedAsWhitespace) {
    const std::string name = utf8(0xFEFF) + "a" + utf8(0x200B);
    EXPECT_EQ(layer_name::normalized(name), name);
}

TEST(LayerNameTest, MalformedUtf8IsRejected) {
    const std::string cases[] = {
        bytes({0x80}),
        bytes({0xBF}),
        bytes({0xC0, 0x80}),
        bytes({0xC1, 0xBF}),
        bytes({0xE0, 0x80, 0x80}),
        bytes({0xE0, 0x9F, 0xBF}),
        bytes({0xED, 0xA0, 0x80}),
        bytes({0xED, 0xBF, 0xBF}),
        bytes({0xF0, 0x80, 0x80, 0x80}),
        bytes({0xF0, 0x8F, 0xBF, 0xBF}),
        bytes({0xF4, 0x90, 0x80, 0x80}),
        bytes({0xF5, 0x80, 0x80, 0x80}),
        bytes({0xFF}),
        bytes({0xC3}),
        bytes({0xE6, 0x97}),
        bytes({0xF0, 0x9F, 0x98}),
        bytes({0xC3, 0x41}),
        bytes({0xE6, 0x41, 0x97}),
    };
    for (const std::string& malformed : cases) {
        EXPECT_FALSE(layer_name::normalized(malformed).has_value());
        EXPECT_FALSE(layer_name::normalized("ok" + malformed).has_value());
    }
}

TEST(LayerNameTest, EveryAcceptedNameDumpsToJsonWithoutThrowing) {
    const std::string candidates[] = {"plain", repeated(0x1F600, 50), "caf" + utf8(0xE9), utf8(0x10FFFF),
                                      "  padded  "};
    for (const std::string& candidate : candidates) {
        const std::optional<std::string> stored = layer_name::normalized(candidate);
        ASSERT_TRUE(stored.has_value());
        nlohmann::json doc;
        doc["name"] = *stored;
        EXPECT_NO_THROW(doc.dump());
    }
}
