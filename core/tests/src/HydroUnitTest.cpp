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

TEST(HydroUnit, IsValidEmptyUnit) {
    HydroUnit unit(100, HydroUnit::Lumped);

    // Unit with no land covers should be valid
    EXPECT_TRUE(unit.IsValid());
    EXPECT_NO_THROW(unit.Validate());
}

TEST(HydroUnit, IsValidWithSingleLandCover) {
    HydroUnit unit(100, HydroUnit::Lumped);

    // Add a land cover with fraction 1.0 using unique_ptr
    auto landCover = std::make_unique<LandCover>();
    landCover->SetName("ground");
    landCover->SetAreaFraction(1.0);
    unit.AddBrick(std::move(landCover));

    EXPECT_TRUE(unit.IsValid(false));
}

TEST(HydroUnit, IsValidWithMultipleLandCovers) {
    HydroUnit unit(100, HydroUnit::Lumped);

    // Add multiple land covers summing to 1.0
    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(0.6);
    unit.AddBrick(std::move(ground));

    auto glacier = std::make_unique<LandCover>();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.4);
    unit.AddBrick(std::move(glacier));

    EXPECT_TRUE(unit.IsValid(false));
}

TEST(HydroUnit, IsValidZeroFraction) {
    HydroUnit unit(100, HydroUnit::Lumped);

    // Land cover with zero fraction should be valid
    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(1.0);
    unit.AddBrick(std::move(ground));

    auto glacier = std::make_unique<LandCover>();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.0);
    unit.AddBrick(std::move(glacier));

    EXPECT_TRUE(unit.IsValid(false));
}

TEST(HydroUnit, IsValidNearZeroFraction) {
    HydroUnit unit(100, HydroUnit::Lumped);

    // Near-zero fraction at edge of EPSILON_D
    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(1.0 - EPSILON_D);
    unit.AddBrick(std::move(ground));

    auto glacier = std::make_unique<LandCover>();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(EPSILON_D);
    unit.AddBrick(std::move(glacier));

    EXPECT_TRUE(unit.IsValid(false));
}

TEST(HydroUnit, IsValidInvalidNegativeFraction) {
    wxLogNull logNo;  // Suppress error messages

    HydroUnit unit(100, HydroUnit::Lumped);

    // Negative fraction should be invalid (but IsValid auto-corrects small errors)
    auto landCover = std::make_unique<LandCover>();
    landCover->SetName("ground");
    landCover->SetAreaFraction(-0.1);
    unit.AddBrick(std::move(landCover));

    // IsValid returns false for out-of-range fractions
    EXPECT_FALSE(unit.IsValid(false));
    EXPECT_THROW(unit.Validate(), ModelConfigError);
}

TEST(HydroUnit, IsValidInvalidFractionAboveOne) {
    wxLogNull logNo;  // Suppress error messages

    HydroUnit unit(100, HydroUnit::Lumped);

    // Fraction above 1.0 should be invalid
    auto landCover = std::make_unique<LandCover>();
    landCover->SetName("ground");
    landCover->SetAreaFraction(1.5);
    unit.AddBrick(std::move(landCover));

    EXPECT_FALSE(unit.IsValid());
    EXPECT_THROW(unit.Validate(), ModelConfigError);
}

TEST(HydroUnit, IsValidInvalidSumNotEqualOne) {
    wxLogNull logNo;  // Suppress error messages

    HydroUnit unit(100, HydroUnit::Lumped);

    // Fractions not summing to 1.0 should be invalid (beyond TOLERANCE_LOOSE)
    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(0.5);
    unit.AddBrick(std::move(ground));

    auto glacier = std::make_unique<LandCover>();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.4);  // Sum = 0.9
    unit.AddBrick(std::move(glacier));

    EXPECT_FALSE(unit.IsValid());
    EXPECT_THROW(unit.Validate(), ModelConfigError);
}

TEST(HydroUnit, IsValidSumWithinTolerance) {
    HydroUnit unit(100, HydroUnit::Lumped);

    // Fractions summing to slightly off 1.0 but within TOLERANCE_LOOSE should be auto-corrected
    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(0.6000001);
    unit.AddBrick(std::move(ground));

    auto glacier = std::make_unique<LandCover>();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.4);
    unit.AddBrick(std::move(glacier));

    // IsValid should auto-correct small rounding errors
    EXPECT_TRUE(unit.IsValid(false));
}

TEST(HydroUnit, IsValidInvalidSumExceedsOne) {
    wxLogNull logNo;  // Suppress error messages

    HydroUnit unit(100, HydroUnit::Lumped);

    // Fractions summing to more than 1.0 (beyond tolerance) should be invalid
    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(0.6);
    unit.AddBrick(std::move(ground));

    auto glacier = std::make_unique<LandCover>();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.5);  // Sum = 1.1
    unit.AddBrick(std::move(glacier));

    EXPECT_FALSE(unit.IsValid());
    EXPECT_THROW(unit.Validate(), ModelConfigError);
}

TEST(HydroUnit, ChangeLandCoverAreaFractionValidatesInvariants) {
    HydroUnit unit(100, HydroUnit::Lumped);

    // Add land covers
    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(0.7);
    unit.AddBrick(std::move(ground));

    auto glacier = std::make_unique<LandCover>();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.3);
    unit.AddBrick(std::move(glacier));

    // Change a land cover fraction - should auto-adjust ground to maintain sum = 1.0
    EXPECT_TRUE(unit.ChangeLandCoverAreaFraction("glacier", 0.4));

    EXPECT_TRUE(unit.IsValid(false));
}

TEST(HydroUnit, ChangeLandCoverAreaFractionToZero) {
    HydroUnit unit(100, HydroUnit::Lumped);

    // Add land covers
    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(0.6);
    unit.AddBrick(std::move(ground));

    auto glacier = std::make_unique<LandCover>();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.4);
    unit.AddBrick(std::move(glacier));

    // Change glacier to zero
    EXPECT_TRUE(unit.ChangeLandCoverAreaFraction("glacier", 0.0));
    EXPECT_TRUE(unit.IsValid(false));
}

TEST(HydroUnit, ChangeLandCoverAreaFractionInvalidNegative) {
    wxLogNull logNo;  // Suppress error messages

    HydroUnit unit(100, HydroUnit::Lumped);

    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(1.0);
    unit.AddBrick(std::move(ground));

    // Attempting to set negative fraction should fail
    EXPECT_FALSE(unit.ChangeLandCoverAreaFraction("ground", -0.1));
}

TEST(HydroUnit, ChangeLandCoverAreaFractionInvalidAboveOne) {
    wxLogNull logNo;  // Suppress error messages

    HydroUnit unit(100, HydroUnit::Lumped);

    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(1.0);
    unit.AddBrick(std::move(ground));

    // Attempting to set fraction above 1.0 should fail
    EXPECT_FALSE(unit.ChangeLandCoverAreaFraction("ground", 1.5));
}

TEST(HydroUnit, IsValidCallsValidateOnBricks) {
    HydroUnit unit(100, HydroUnit::Lumped);

    // Add a land cover
    auto ground = std::make_unique<LandCover>();
    ground->SetName("ground");
    ground->SetAreaFraction(1.0);
    unit.AddBrick(std::move(ground));

    // IsValid() should cascade to brick validation
    EXPECT_TRUE(unit.IsValid(false));
}

TEST(HydroUnit, IsValidInvalidArea) {
    wxLogNull logNo;  // Suppress error messages

    // Unit with zero or negative area should be invalid
    HydroUnit unit(0, HydroUnit::Lumped);

    EXPECT_FALSE(unit.IsValid());
    EXPECT_THROW(unit.Validate(), ModelConfigError);
}

TEST(HydroUnit, IsValidNegativeArea) {
    wxLogNull logNo;  // Suppress error messages

    HydroUnit unit(-100, HydroUnit::Lumped);

    EXPECT_FALSE(unit.IsValid());
    EXPECT_THROW(unit.Validate(), ModelConfigError);
}
