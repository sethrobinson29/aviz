#include "compositor/compositor.h"

#include <gtest/gtest.h>

TEST(CompositorAddLayerTest, ReturnsSequentialIndicesAndGrowsStack) {
    Compositor compositor;
    EXPECT_EQ(compositor.add_layer(VisualType::Waveform), 0u);
    EXPECT_EQ(compositor.add_layer(VisualType::Spectrum), 1u);
    EXPECT_EQ(compositor.add_layer(VisualType::Waveform), 2u);
    ASSERT_EQ(compositor.layers().size(), 3u);
}

TEST(CompositorRemoveLayerTest, RemovesElementAtValidIndex) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);
    compositor.add_layer(VisualType::Waveform);

    compositor.remove_layer(1);

    ASSERT_EQ(compositor.layers().size(), 2u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Waveform);
}

TEST(CompositorRemoveLayerTest, OutOfRangeIndexIsNoOp) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);

    compositor.remove_layer(2);  // == size(): one past the last valid index
    EXPECT_EQ(compositor.layers().size(), 2u);

    compositor.remove_layer(99);  // > size()
    EXPECT_EQ(compositor.layers().size(), 2u);
}

TEST(CompositorRemoveLayerTest, OutOfRangeIndexOnEmptyStackIsNoOp) {
    Compositor compositor;
    compositor.remove_layer(0);
    EXPECT_TRUE(compositor.layers().empty());
}

TEST(CompositorMoveLayerTest, MovesTopToBottom) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);

    compositor.move_layer(1, 0);

    ASSERT_EQ(compositor.layers().size(), 2u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Spectrum);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Waveform);
}

TEST(CompositorMoveLayerTest, NoOpWhenFromEqualsTo) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);

    compositor.move_layer(0, 0);

    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Spectrum);
}
