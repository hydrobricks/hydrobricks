#include <gtest/gtest.h>

#include <memory>

#include "GenericLandCover.h"
#include "HydroUnit.h"
#include "LandCover.h"
#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

class ModelSpinup : public ::testing::Test {
  protected:
    // Simple linear storage fed by a constant precipitation, so the storage (and the
    // outlet discharge) rises asymptotically: a warmed-up run must start higher than
    // a cold one.
    SettingsModel BuildModelSettings(int spinupDays) {
        SettingsModel settings;
        settings.SetLogAll(true);
        settings.SetSolver("euler_explicit");
        settings.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        if (spinupDays > 0) {
            settings.SetSpinupDays(spinupDays);
        }
        settings.AddHydroUnitBrick("storage", "storage");
        settings.AddBrickForcing("precipitation");
        settings.AddBrickLogging("water_content");
        settings.AddBrickProcess("outflow", "outflow:linear");
        settings.SetProcessParameterValue("response_factor", 0.1f);
        settings.AddProcessLogging("output");
        settings.AddProcessOutput("outlet");
        settings.AddLoggingToItem("outlet");

        return settings;
    }

    std::unique_ptr<TimeSeriesUniform> BuildConstantPrecip() {
        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
        data->SetValues({5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0});
        auto ts = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        ts->SetData(std::move(data));

        return ts;
    }

    axd RunAndGetDischarge(int spinupDays) {
        SettingsModel modelSettings = BuildModelSettings(spinupDays);

        SettingsBasin basinSettings;
        basinSettings.AddHydroUnit(1, 100);

        SubBasin subBasin;
        EXPECT_TRUE(subBasin.Initialize(basinSettings));

        ModelHydro model(&subBasin);
        EXPECT_TRUE(model.Initialize(modelSettings, basinSettings));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(BuildConstantPrecip())));
        EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());
        EXPECT_TRUE(model.Run());

        return model.GetOutletDischarge();
    }
};

TEST_F(ModelSpinup, SpinupWarmsTheInitialState) {
    axd coldDischarge = RunAndGetDischarge(0);
    axd warmDischarge = RunAndGetDischarge(5);

    ASSERT_EQ(coldDischarge.size(), warmDischarge.size());
    // The warmed-up run starts with a partly filled storage, so its first-step
    // discharge exceeds the cold start's.
    EXPECT_GT(warmDischarge[0], coldDischarge[0]);
    // The whole warmed series stays above the cold one (monotonically filling store).
    for (int i = 0; i < coldDischarge.size(); ++i) {
        EXPECT_GE(warmDischarge[i], coldDischarge[i]);
    }
}

TEST_F(ModelSpinup, SpinupLongerThanPeriodClampsToWholePeriod) {
    // 100 days of spin-up on a 10-day period: replay the whole period once.
    axd clampedDischarge = RunAndGetDischarge(100);
    axd wholePeriodDischarge = RunAndGetDischarge(10);

    ASSERT_EQ(clampedDischarge.size(), wholePeriodDischarge.size());
    for (int i = 0; i < clampedDischarge.size(); ++i) {
        EXPECT_DOUBLE_EQ(clampedDischarge[i], wholePeriodDischarge[i]);
    }
}

TEST_F(ModelSpinup, WholePeriodSpinupMatchesManualTwoPassRun) {
    // Manual two-pass reference: run the whole period cold, adopt the end states as
    // initial conditions, reset and rerun.
    SettingsModel modelSettings = BuildModelSettings(0);

    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(modelSettings, basinSettings));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(BuildConstantPrecip())));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());
    ASSERT_TRUE(model.Run());
    model.SaveAsInitialState();
    model.Reset();
    ASSERT_TRUE(model.Run());
    axd manualDischarge = model.GetOutletDischarge();

    axd spinupDischarge = RunAndGetDischarge(10);

    ASSERT_EQ(manualDischarge.size(), spinupDischarge.size());
    for (int i = 0; i < manualDischarge.size(); ++i) {
        EXPECT_NEAR(spinupDischarge[i], manualDischarge[i], 1e-10);
    }
}

TEST_F(ModelSpinup, SpinupIsRepeatableAcrossResets) {
    // A reset + rerun (the calibration-loop pattern) must reproduce the same series.
    SettingsModel modelSettings = BuildModelSettings(5);

    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(modelSettings, basinSettings));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(BuildConstantPrecip())));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    ASSERT_TRUE(model.Run());
    axd firstDischarge = model.GetOutletDischarge();

    model.Reset();
    ASSERT_TRUE(model.Run());
    axd secondDischarge = model.GetOutletDischarge();

    ASSERT_EQ(firstDischarge.size(), secondDischarge.size());
    for (int i = 0; i < firstDischarge.size(); ++i) {
        EXPECT_DOUBLE_EQ(firstDischarge[i], secondDischarge[i]);
    }
}

TEST(HydroUnitSpinup, RestoreInitialAreaFractions) {
    HydroUnit unit(100, HydroUnit::Lumped);

    auto ground = std::make_unique<GenericLandCover>();
    ground->SetName("open");
    ground->SetAreaFraction(0.6);
    ground->SetInitialAreaFraction(0.6);
    unit.AddBrick(std::move(ground));

    auto glacier = std::make_unique<LandCover>();
    glacier->SetName("glacier");
    glacier->SetAreaFraction(0.4);
    glacier->SetInitialAreaFraction(0.4);
    unit.AddBrick(std::move(glacier));

    // Drift the extents (as a land-cover change would), then restore.
    ASSERT_TRUE(unit.ChangeLandCoverAreaFraction("glacier", 0.1));
    unit.RestoreInitialAreaFractions();

    auto* restoredGlacier = dynamic_cast<LandCover*>(unit.GetBrick("glacier"));
    auto* restoredGround = dynamic_cast<LandCover*>(unit.GetBrick("open"));
    ASSERT_NE(restoredGlacier, nullptr);
    ASSERT_NE(restoredGround, nullptr);
    EXPECT_DOUBLE_EQ(restoredGlacier->GetAreaFraction(), 0.4);
    EXPECT_DOUBLE_EQ(restoredGround->GetAreaFraction(), 0.6);
}
