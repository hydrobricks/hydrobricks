#include <gtest/gtest.h>

#include "HydroUnit.h"
#include "LandCover.h"

TEST(HydroUnit, BuildsCorrectly) {
    HydroUnit unit(100, HydroUnit::Lumped);

    EXPECT_EQ(unit.GetArea(), 100);
    EXPECT_EQ(unit.GetType(), HydroUnit::Lumped);
}

TEST(HydroUnit, HasCorrectId) {
    HydroUnit unit(100);
    unit.SetId(9);

    EXPECT_EQ(unit.GetId(), 9);
}

TEST(HydroUnit, ValidateEmptyUnit) {
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Unit with no land covers should validate successfully
    EXPECT_TRUE(unit.Validate());
}

TEST(HydroUnit, ValidateWithSingleLandCover) {
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Add a land cover with fraction 1.0
    auto* landCover = new LandCover();
    landCover->SetName("ground");
    landCover->SetAreaFraction(1.0);
    unit.AddBrick(landCover);
    
    EXPECT_TRUE(unit.Validate());
}

TEST(HydroUnit, ValidateWithMultipleLandCovers) {
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Add multiple land covers summing to 1.0
    auto* ground = new LandCover();
    ground->SetName("ground");
    ground->SetAreaFraction(0.6);
    unit.AddBrick(ground);
    
    auto* glacier = new LandCover();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.4);
    unit.AddBrick(glacier);
    
    EXPECT_TRUE(unit.Validate());
}

TEST(HydroUnit, ValidateZeroFraction) {
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Land cover with zero fraction should be valid
    auto* ground = new LandCover();
    ground->SetName("ground");
    ground->SetAreaFraction(1.0);
    unit.AddBrick(ground);
    
    auto* glacier = new LandCover();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.0);
    unit.AddBrick(glacier);
    
    EXPECT_TRUE(unit.Validate());
}

TEST(HydroUnit, ValidateNearZeroFraction) {
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Near-zero fraction at edge of EPSILON_D
    auto* ground = new LandCover();
    ground->SetName("ground");
    ground->SetAreaFraction(1.0 - EPSILON_D);
    unit.AddBrick(ground);
    
    auto* glacier = new LandCover();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(EPSILON_D);
    unit.AddBrick(glacier);
    
    EXPECT_TRUE(unit.Validate());
}

TEST(HydroUnit, ValidateInvalidNegativeFraction) {
    wxLogNull logNo;  // Suppress error messages
    
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Negative fraction should be invalid
    auto* landCover = new LandCover();
    landCover->SetName("ground");
    landCover->SetAreaFraction(-0.1);
    unit.AddBrick(landCover);
    
    EXPECT_FALSE(unit.Validate());
}

TEST(HydroUnit, ValidateInvalidFractionAboveOne) {
    wxLogNull logNo;  // Suppress error messages
    
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Fraction above 1.0 should be invalid
    auto* landCover = new LandCover();
    landCover->SetName("ground");
    landCover->SetAreaFraction(1.5);
    unit.AddBrick(landCover);
    
    EXPECT_FALSE(unit.Validate());
}

TEST(HydroUnit, ValidateInvalidSumNotEqualOne) {
    wxLogNull logNo;  // Suppress error messages
    
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Fractions not summing to 1.0 should be invalid
    auto* ground = new LandCover();
    ground->SetName("ground");
    ground->SetAreaFraction(0.5);
    unit.AddBrick(ground);
    
    auto* glacier = new LandCover();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.4);  // Sum = 0.9
    unit.AddBrick(glacier);
    
    EXPECT_FALSE(unit.Validate());
}

TEST(HydroUnit, ValidateInvalidSumExceedsOne) {
    wxLogNull logNo;  // Suppress error messages
    
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Fractions summing to more than 1.0 should be invalid
    auto* ground = new LandCover();
    ground->SetName("ground");
    ground->SetAreaFraction(0.6);
    unit.AddBrick(ground);
    
    auto* glacier = new LandCover();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.5);  // Sum = 1.1
    unit.AddBrick(glacier);
    
    EXPECT_FALSE(unit.Validate());
}

TEST(HydroUnit, ChangeLandCoverAreaFractionValidatesInvariants) {
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Add land covers
    auto* ground = new LandCover();
    ground->SetName("ground");
    ground->SetAreaFraction(0.7);
    unit.AddBrick(ground);
    
    auto* glacier = new LandCover();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.3);
    unit.AddBrick(glacier);
    
    // Change a land cover fraction - should auto-adjust ground to maintain sum = 1.0
    EXPECT_TRUE(unit.ChangeLandCoverAreaFraction("glacier", 0.4));
    
    // Validate should pass after automatic adjustment
    EXPECT_TRUE(unit.Validate());
}

TEST(HydroUnit, ChangeLandCoverAreaFractionToZero) {
    HydroUnit unit(100, HydroUnit::Lumped);
    
    // Add land covers
    auto* ground = new LandCover();
    ground->SetName("ground");
    ground->SetAreaFraction(0.6);
    unit.AddBrick(ground);
    
    auto* glacier = new LandCover();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.4);
    unit.AddBrick(glacier);
    
    // Change glacier to zero
    EXPECT_TRUE(unit.ChangeLandCoverAreaFraction("glacier", 0.0));
    EXPECT_TRUE(unit.Validate());
}

TEST(HydroUnit, ChangeLandCoverAreaFractionInvalidNegative) {
    wxLogNull logNo;  // Suppress error messages
    
    HydroUnit unit(100, HydroUnit::Lumped);
    
    auto* ground = new LandCover();
    ground->SetName("ground");
    ground->SetAreaFraction(1.0);
    unit.AddBrick(ground);
    
    // Attempting to set negative fraction should fail
    EXPECT_FALSE(unit.ChangeLandCoverAreaFraction("ground", -0.1));
}

TEST(HydroUnit, ChangeLandCoverAreaFractionInvalidAboveOne) {
    wxLogNull logNo;  // Suppress error messages
    
    HydroUnit unit(100, HydroUnit::Lumped);
    
    auto* ground = new LandCover();
    ground->SetName("ground");
    ground->SetAreaFraction(1.0);
    unit.AddBrick(ground);
    
    // Attempting to set fraction above 1.0 should fail
    EXPECT_FALSE(unit.ChangeLandCoverAreaFraction("ground", 1.5));
}
