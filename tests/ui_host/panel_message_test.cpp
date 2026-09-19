#include "ui_host/panel_message.h"

#include <gtest/gtest.h>

TEST(PanelMessageParseTest, ParsesAddLayer) {
    std::optional<PanelMessage> message = panel_message::parse(R"({"type":"add_layer"})");
    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(message->type, PanelMessageType::AddLayer);
}

TEST(PanelMessageParseTest, IgnoresUnknownFields) {
    std::optional<PanelMessage> message =
        panel_message::parse(R"({"type":"add_layer","extra":123,"nested":{"a":1}})");
    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(message->type, PanelMessageType::AddLayer);
}

TEST(PanelMessageParseTest, RejectsEmptyString) {
    EXPECT_FALSE(panel_message::parse("").has_value());
}

TEST(PanelMessageParseTest, RejectsMalformedJson) {
    EXPECT_FALSE(panel_message::parse("{not valid json").has_value());
}

TEST(PanelMessageParseTest, RejectsNonObjectJson) {
    EXPECT_FALSE(panel_message::parse("42").has_value());
    EXPECT_FALSE(panel_message::parse("[1,2,3]").has_value());
    EXPECT_FALSE(panel_message::parse(R"("add_layer")").has_value());
}

TEST(PanelMessageParseTest, RejectsMissingTypeField) {
    EXPECT_FALSE(panel_message::parse(R"({"foo":"bar"})").has_value());
}

TEST(PanelMessageParseTest, RejectsUnknownTypeValue) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"delete_everything"})").has_value());
}

TEST(PanelMessageParseTest, RejectsWrongFieldType) {
    EXPECT_FALSE(panel_message::parse(R"({"type":123})").has_value());
    EXPECT_FALSE(panel_message::parse(R"({"type":null})").has_value());
    EXPECT_FALSE(panel_message::parse(R"({"type":["add_layer"]})").has_value());
}
