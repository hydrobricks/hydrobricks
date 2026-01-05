#include <gtest/gtest.h>

#include "Brick.h"
#include "WaterContainer.h"

// Helper brick class for testing
class TestBrick : public Brick {
  public:
    TestBrick() : Brick() {
        SetName("test_brick");
    }
};

TEST(WaterContainer, ValidateEmptyContainer) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    // Empty container should validate
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateWithContentNoCapacity) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    // Set some content without capacity
    container.UpdateContent(10.0);
    
    // Should validate - no capacity limit
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateContentBelowCapacity) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(50.0);
    
    // Content below capacity should validate
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateContentEqualToCapacity) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(100.0);
    
    // Content equal to capacity should validate
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateContentNearCapacity) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(100.0 - PRECISION / 2);
    
    // Content just below capacity should validate
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateContentExceedsCapacity) {
    wxLogNull logNo;  // Suppress error messages
    
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(110.0);
    
    // Content exceeding capacity should fail validation
    EXPECT_FALSE(container.Validate());
}

TEST(WaterContainer, ValidateContentJustOverCapacity) {
    wxLogNull logNo;  // Suppress error messages
    
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(100.0 + PRECISION * 2);
    
    // Content just over capacity (beyond precision) should fail
    EXPECT_FALSE(container.Validate());
}

TEST(WaterContainer, ValidateContentWithinPrecisionOfCapacity) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(100.0 + PRECISION / 2);
    
    // Content within PRECISION of capacity should validate
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateContentWithDynamicChanges) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(80.0);
    container.AddAmountToDynamicContentChange(15.0);
    
    // Total = 95.0, should validate
    EXPECT_TRUE(container.Validate());
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
    EXPECT_FALSE(container.Validate());
}

TEST(WaterContainer, ValidateContentWithStaticChanges) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(70.0);
    container.AddAmountToStaticContentChange(20.0);
    
    // Total = 90.0, should validate
    EXPECT_TRUE(container.Validate());
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
    EXPECT_FALSE(container.Validate());
}

TEST(WaterContainer, ValidateContentWithBothChanges) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(50.0);
    container.AddAmountToDynamicContentChange(25.0);
    container.AddAmountToStaticContentChange(20.0);
    
    // Total = 95.0, should validate
    EXPECT_TRUE(container.Validate());
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
    EXPECT_FALSE(container.Validate());
}

TEST(WaterContainer, ValidateZeroContent) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(0.0);
    
    // Zero content should validate
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateNearZeroContent) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(PRECISION / 2);
    
    // Near-zero positive content should validate
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateNegativeContent) {
    wxLogNull logNo;
    
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(-5.0);
    
    // Negative content should fail
    EXPECT_FALSE(container.Validate());
}

TEST(WaterContainer, ValidateSlightlyNegativeContent) {
    wxLogNull logNo;
    
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(-PRECISION * 2);
    
    // Negative content beyond precision should fail
    EXPECT_FALSE(container.Validate());
}

TEST(WaterContainer, ValidateContentWithinNegativePrecision) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    float capacity = 100.0f;
    container.SetMaximumCapacity(&capacity);
    container.UpdateContent(-PRECISION / 2);
    
    // Content within negative precision should validate
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateInfiniteStorage) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    container.SetAsInfiniteStorage();
    // Infinite storage should always validate regardless of operations
    EXPECT_TRUE(container.Validate());
}

TEST(WaterContainer, ValidateInfiniteStorageWithChanges) {
    TestBrick brick;
    WaterContainer container(&brick);
    
    container.SetAsInfiniteStorage();
    // These operations should not affect infinite storage
    container.AddAmountToDynamicContentChange(1000.0);
    container.AddAmountToStaticContentChange(1000.0);
    
    EXPECT_TRUE(container.Validate());
}
