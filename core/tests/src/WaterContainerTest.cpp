#include <gtest/gtest.h>

#include "Brick.h"
#include "WaterContainer.h"

// Helper brick class for testing
class TestBrick : public Brick {
  public:
    TestBrick()
        : Brick() {
        SetName("test_brick");
    }
};

TEST(WaterContainer, ValidateContentExceedsCapacity) {
    wxLogNull logNo;  // Suppress error messages

    TestBrick brick;
    WaterContainer container(&brick);

    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(110.0);

    // Content exceeding capacity should fail validation
    EXPECT_FALSE(container.IsValid());
}

TEST(WaterContainer, ValidateContentJustOverCapacity) {
    wxLogNull logNo;  // Suppress error messages

    TestBrick brick;
    WaterContainer container(&brick);

    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(100.0 + PRECISION * 2);

    // Content just over capacity (beyond precision) should fail
    EXPECT_FALSE(container.IsValid());
}

TEST(WaterContainer, ValidateContentWithDynamicChangesExceedsCapacity) {
    wxLogNull logNo;

    TestBrick brick;
    WaterContainer container(&brick);

    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(80.0);
    container.AddAmountToDynamicContentChange(25.0);

    // Total = 105.0, should fail
    EXPECT_FALSE(container.IsValid());
}

TEST(WaterContainer, ValidateContentWithStaticChangesExceedsCapacity) {
    wxLogNull logNo;

    TestBrick brick;
    WaterContainer container(&brick);

    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(70.0);
    container.AddAmountToStaticContentChange(35.0);

    // Total = 105.0, should fail
    EXPECT_FALSE(container.IsValid());
}

TEST(WaterContainer, ValidateContentWithBothChangesExceedsCapacity) {
    wxLogNull logNo;

    TestBrick brick;
    WaterContainer container(&brick);

    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(50.0);
    container.AddAmountToDynamicContentChange(30.0);
    container.AddAmountToStaticContentChange(25.0);

    // Total = 105.0, should fail
    EXPECT_FALSE(container.IsValid());
}

TEST(WaterContainer, ValidateNegativeContent) {
    wxLogNull logNo;

    TestBrick brick;
    WaterContainer container(&brick);

    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(-5.0);

    // Negative content should fail
    EXPECT_FALSE(container.IsValid());
}

TEST(WaterContainer, ValidateSlightlyNegativeContent) {
    wxLogNull logNo;

    TestBrick brick;
    WaterContainer container(&brick);

    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(-PRECISION * 2);

    // Negative content beyond precision should fail
    EXPECT_FALSE(container.IsValid());
}
