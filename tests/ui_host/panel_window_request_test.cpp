#include "ui_host/panel_window_request.h"

#include <string>

#include <gtest/gtest.h>

#include "ui_host/panel_message.h"

namespace {

std::optional<PanelWindowDragRegion> parse(const std::string& text) {
    return panel_window_request::parse_drag(text);
}

}

TEST(PanelWindowRequestTest, ParsesEveryDragRegion) {
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"caption"})"), PanelWindowDragRegion::Caption);
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"top"})"), PanelWindowDragRegion::Top);
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"bottom"})"), PanelWindowDragRegion::Bottom);
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"left"})"), PanelWindowDragRegion::Left);
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"right"})"), PanelWindowDragRegion::Right);
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"top_left"})"), PanelWindowDragRegion::TopLeft);
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"top_right"})"), PanelWindowDragRegion::TopRight);
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"bottom_left"})"), PanelWindowDragRegion::BottomLeft);
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"bottom_right"})"), PanelWindowDragRegion::BottomRight);
}

TEST(PanelWindowRequestTest, IgnoresExtraFields) {
    EXPECT_EQ(parse(R"({"type":"window_drag","region":"left","x":5,"path":"C:/x"})"), PanelWindowDragRegion::Left);
}

TEST(PanelWindowRequestTest, RejectsOtherMessageTypes) {
    EXPECT_FALSE(parse(R"({"type":"request_state"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"open_preset_dialog","region":"caption"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"Window_Drag","region":"caption"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag ","region":"caption"})").has_value());
}

TEST(PanelWindowRequestTest, RejectsMissingOrWrongTypedFields) {
    EXPECT_FALSE(parse(R"({"region":"caption"})").has_value());
    EXPECT_FALSE(parse(R"({"type":7,"region":"caption"})").has_value());
    EXPECT_FALSE(parse(R"({"type":null,"region":"caption"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":null})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":2})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":true})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":["caption"]})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":{"name":"caption"}})").has_value());
}

TEST(PanelWindowRequestTest, RejectsUnknownOrNearMissRegions) {
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":""})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":"Caption"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":"caption "})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":"topleft"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":"client"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":"caption\u0000"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":"2"})").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":")" + std::string(100000, 'a') + R"("})").has_value());
}

TEST(PanelWindowRequestTest, RejectsMalformedOrNonObjectJson) {
    EXPECT_FALSE(parse("").has_value());
    EXPECT_FALSE(parse("not json").has_value());
    EXPECT_FALSE(parse(R"({"type":"window_drag","region":"caption")").has_value());
    EXPECT_FALSE(parse(R"(["window_drag","caption"])").has_value());
    EXPECT_FALSE(parse(R"("window_drag")").has_value());
    EXPECT_FALSE(parse("null").has_value());
    EXPECT_FALSE(parse(std::string("{\"type\":\"window_drag\",\"region\":\"cap\xff\"}")).has_value());
}

TEST(PanelWindowRequestTest, WindowDragIsNotAcceptedByTheRenderThreadMessageParser) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"window_drag","region":"caption"})").has_value());
}

namespace {

std::optional<PanelWindowCommand> parse_command(const std::string& text) {
    return panel_window_request::parse_command(text);
}

}

TEST(PanelWindowCommandTest, ParsesCloseAndMinimize) {
    EXPECT_EQ(parse_command(R"({"type":"window_command","command":"close"})"), PanelWindowCommand::Close);
    EXPECT_EQ(parse_command(R"({"type":"window_command","command":"minimize"})"), PanelWindowCommand::Minimize);
}

TEST(PanelWindowCommandTest, IgnoresExtraFields) {
    EXPECT_EQ(parse_command(R"({"type":"window_command","command":"minimize","region":"caption"})"),
              PanelWindowCommand::Minimize);
}

TEST(PanelWindowCommandTest, RejectsCommandsThePanelDoesNotOffer) {
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":"maximize"})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":"restore"})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":"move"})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":""})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":"Close"})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":" close"})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":"close\u0000"})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":"61536"})").has_value());
}

TEST(PanelWindowCommandTest, RejectsMissingOrWrongTypedFields) {
    EXPECT_FALSE(parse_command(R"({"type":"window_command"})").has_value());
    EXPECT_FALSE(parse_command(R"({"command":"close"})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":null})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":61536})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":true})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":["close"]})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":["window_command"],"command":"close"})").has_value());
}

TEST(PanelWindowCommandTest, RejectsMalformedJsonAndOtherTypes) {
    EXPECT_FALSE(parse_command("").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_command","command":"close")").has_value());
    EXPECT_FALSE(parse_command(R"(["window_command","close"])").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_drag","command":"close"})").has_value());
    EXPECT_FALSE(parse_command(R"({"type":"window_drag","region":"caption"})").has_value());
    EXPECT_FALSE(parse_command(std::string("{\"type\":\"window_command\",\"command\":\"clos\xe9\"}")).has_value());
}

TEST(PanelWindowCommandTest, DragAndCommandParsersDoNotAcceptEachOthersMessages) {
    EXPECT_FALSE(panel_window_request::parse_drag(R"({"type":"window_command","command":"close"})").has_value());
    EXPECT_FALSE(panel_window_request::parse_drag(R"({"type":"window_command","region":"caption"})").has_value());
}

TEST(PanelWindowCommandTest, WindowCommandIsNotAcceptedByTheRenderThreadMessageParser) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"window_command","command":"close"})").has_value());
    EXPECT_FALSE(panel_message::parse(R"({"type":"window_command","command":"minimize"})").has_value());
}
