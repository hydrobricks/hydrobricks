#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <numbers>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Tests for the processes introduced for the PREVAH model structure:
 * outflow:linear_threshold, outflow:split, percolation:prevah, melt:degree_day_seasonal.
 */

/**
 * Model: single storage with a linear-threshold outflow (PREVAH Q0 = K0 (SUZ - SGRLUZ)).
 */
class LinearThresholdStorage : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        _model.AddHydroUnitBrick("storage", "storage");
        _model.AddBrickForcing("precipitation");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("outflow", "outflow:linear_threshold");
        _model.SetProcessParameterValue("response_factor_threshold", 0.3f);
        _model.SetProcessParameterValue("threshold", 5.0f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        _model.AddLoggingToItem("outlet");

        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
        data->SetValues({0.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(data));
    }
};

TEST_F(LinearThresholdStorage, DrainsOnlyAboveThreshold) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    // Explicit Euler, daily: out_t = k max(0, S_t - threshold); S_{t+1} = S_t + P_t - out_t
    vecDouble expectedOutputs = {0.000000, 0.000000, 1.500000, 4.050000, 2.835000,
                                 1.984500, 1.389150, 0.972405, 0.680683, 0.476478};

    for (auto& basinOutput : basinOutputs) {
        ASSERT_EQ(basinOutput.size(), expectedOutputs.size());
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // The threshold retains dead storage: the content stays above the threshold
    // while draining toward it, and the water balance closes.
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](9, 0);
    EXPECT_GT(storageContent, 5.0);
    EXPECT_NEAR(20.0 - basinOutputs[0].sum() - storageContent, 0, 0.0000000001);
}

TEST_F(LinearThresholdStorage, ZeroThresholdMatchesLinearReservoir) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SelectHydroUnitBrick("storage");
    _model.SelectProcess("outflow");
    _model.SetProcessParameterValue("threshold", 0.0f);

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    // With a zero threshold the process reduces to outflow:linear (out = k S).
    vecDouble expectedOutputs = {0.000000, 0.000000, 3.000000, 5.100000, 3.570000,
                                 2.499000, 1.749300, 1.224510, 0.857157, 0.600010};

    for (auto& basinOutput : basinOutputs) {
        ASSERT_EQ(basinOutput.size(), expectedOutputs.size());
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}

/**
 * Model: PREVAH upper zone — a linear-threshold outflow (Q0, surface runoff) and a
 * linear outflow (Q1, interflow) drawing from the same storage.
 */
class PrevahUpperZone : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        _model.AddHydroUnitBrick("upper_zone", "storage");
        _model.AddBrickForcing("precipitation");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("q0", "outflow:linear_threshold");
        _model.SetProcessParameterValue("response_factor_threshold", 0.4f);
        _model.SetProcessParameterValue("threshold", 10.0f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");
        _model.AddBrickProcess("q1", "outflow:linear");
        _model.SetProcessParameterValue("response_factor", 0.1f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        _model.AddLoggingToItem("outlet");

        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
        data->SetValues({0.0, 20.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(data));
    }
};

TEST_F(PrevahUpperZone, ThresholdAndLinearOutflowsCoexist) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    // out_t = 0.4 max(0, S_t - 10) + 0.1 S_t; S_{t+1} = S_t + P_t - out_t
    vecDouble expectedOutputs = {0.000000, 0.000000, 6.000000, 3.000000, 1.500000,
                                 0.950000, 0.855000, 0.769500, 0.692550, 0.623295};

    for (auto& basinOutput : basinOutputs) {
        ASSERT_EQ(basinOutput.size(), expectedOutputs.size());
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Water balance closes.
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](9, 0);
    EXPECT_NEAR(20.0 - basinOutputs[0].sum() - storageContent, 0, 0.0000000001);
}

/**
 * Model: fixed-ratio split (PREVAH SLOWCOMP recharge split) — a pass-through storage
 * partitions its inflow 8/9 vs 1/9 into two linear reservoirs.
 */
class SplitStorage : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        _model.AddHydroUnitBrick("split_storage", "storage");
        _model.AddBrickForcing("precipitation");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("split", "outflow:split");
        _model.SetProcessParameterValue("split_fraction", 8.0f / 9.0f);
        _model.AddProcessOutput("slz2");
        _model.AddProcessOutput("slz3");

        _model.AddHydroUnitBrick("slz2", "storage");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("outflow", "outflow:linear");
        _model.SetProcessParameterValue("response_factor", 0.1f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        _model.AddHydroUnitBrick("slz3", "storage");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("outflow", "outflow:linear");
        _model.SetProcessParameterValue("response_factor", 0.1f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        _model.AddLoggingToItem("outlet");

        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
        data->SetValues({0.0, 9.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(data));
    }

    // Logged unit values: 0 = split_storage content, 1 = slz2 content, 2 = slz2 outflow,
    // 3 = slz3 content, 4 = slz3 outflow.
    void RunAndCheckSplit(const string& solver) {
        SettingsBasin basinSettings;
        basinSettings.AddHydroUnit(1, 100);

        SubBasin subBasin;
        EXPECT_TRUE(subBasin.Initialize(basinSettings));

        _model.SetSolver(solver);

        ModelHydro model(&subBasin);
        ASSERT_TRUE(model.Initialize(_model, basinSettings));

        ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
        ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

        EXPECT_TRUE(model.Run());

        vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();
        vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

        // Total received by each reservoir = final content + everything it released.
        double received2 = unitContent[1](9, 0) + unitContent[2].col(0).sum();
        double received3 = unitContent[3](9, 0) + unitContent[4].col(0).sum();
        double splitContent = unitContent[0](9, 0);

        EXPECT_NEAR(received2 + received3 + splitContent, 9.0, 0.0000001);
        EXPECT_NEAR(received2 - 8.0 * received3, 0.0, 0.00001);

        // Global water balance closes.
        double storages = splitContent + unitContent[1](9, 0) + unitContent[3](9, 0);
        EXPECT_NEAR(9.0 - basinOutputs[0].sum() - storages, 0, 0.0000001);
    }
};

TEST_F(SplitStorage, SplitsAtFixedRatioEuler) {
    RunAndCheckSplit("euler_explicit");
}

TEST_F(SplitStorage, SplitsAtFixedRatioHeun) {
    RunAndCheckSplit("heun_explicit");
}

/**
 * Model: PREVAH percolation — an upper zone drains at a constant maximum rate gated
 * by the filling of a separate soil moisture store (the gate brick).
 * Both bricks receive the precipitation forcing directly (duplicated on purpose) so
 * the soil filling can be controlled independently of the upper zone content.
 */
class PrevahPercolation : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        _model.AddHydroUnitBrick("soil", "storage");
        _model.AddBrickParameter("capacity", 100.0f);
        _model.AddBrickForcing("precipitation");
        _model.AddBrickLogging("water_content");
        // Inactive outflow: every brick needs at least one process; a zero response
        // factor keeps the soil content driven by the precipitation only.
        _model.AddBrickProcess("outflow", "outflow:linear");
        _model.SetProcessParameterValue("response_factor", 0.0f);
        _model.AddProcessOutput("outlet");

        _model.AddHydroUnitBrick("upper_zone", "storage");
        _model.AddBrickForcing("precipitation");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("percolation", "percolation:prevah");
        _model.SetProcessParameterValue("percolation_rate", 2.0f);
        _model.SetProcessParameterValue("threshold_fraction", 0.7f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");
        _model.SetProcessGateBrick("soil");

        _model.AddLoggingToItem("outlet");

        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
        data->SetValues({0.0, 50.0, 0.0, 0.0, 30.0, 0.0, 19.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(data));
    }
};

TEST_F(PrevahPercolation, PercolationGatedBySoilMoisture) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    // Soil content (start of day): 0, 0, 50, 50, 50, 80, 80, 99, 99, 99 (no losses).
    // Gating: 0 below 70 (= 0.7 x 100); (SM/FC - 0.7)/0.3 above:
    //   day 6-7: (0.8 - 0.7)/0.3 = 1/3 -> 2/3 mm/d; day 8-10: (0.99 - 0.7)/0.3 -> 1.933333 mm/d.
    vecDouble expectedOutputs = {0.000000, 0.000000, 0.000000, 0.000000, 0.000000,
                                 0.666667, 0.666667, 1.933333, 1.933333, 1.933333};

    for (auto& basinOutput : basinOutputs) {
        ASSERT_EQ(basinOutput.size(), expectedOutputs.size());
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}

TEST_F(PrevahPercolation, MissingGateBrickFailsInitialization) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    // Same structure but without the gate brick declaration.
    SettingsModel settings;
    settings.SetSolver("euler_explicit");
    settings.SetTimer("2020-01-01", "2020-01-10", 1, "day");
    settings.AddHydroUnitBrick("soil", "storage");
    settings.AddBrickParameter("capacity", 100.0f);
    settings.AddBrickForcing("precipitation");
    settings.AddHydroUnitBrick("upper_zone", "storage");
    settings.AddBrickForcing("precipitation");
    settings.AddBrickProcess("percolation", "percolation:prevah");
    settings.SetProcessParameterValue("percolation_rate", 2.0f);
    settings.AddProcessOutput("outlet");
    settings.AddLoggingToItem("outlet");

    ModelHydro model(&subBasin);
    EXPECT_FALSE(model.Initialize(settings, basinSettings));
}

/**
 * Model: snowpack with the PREVAH seasonal degree-day melt factor (sine between
 * CRMFMIN and CRMFMAX, maximum at the summer solstice).
 */
class SeasonalMeltSnowpack : public ::testing::Test {
  protected:
    SettingsModel _model;

    // Expected degree-day factor for a given day of year.
    static double ExpectedDDF(int doy) {
        double mean = (6.0 + 2.0) / 2.0;
        double amplitude = (6.0 - 2.0) / 2.0;
        return mean + amplitude * std::sin(2.0 * std::numbers::pi * (doy - 80) / 366.0);
    }

    void SetUpModel(const string& start, const string& end) {
        _model.SetSolver("euler_explicit");
        _model.SetTimer(start, end, 1, "day");

        _model.AddHydroUnitBrick("snowpack", "snowpack");
        _model.AddBrickLogging({"water_content", "snow_content"});

        _model.AddBrickProcess("melt", "melt:degree_day_seasonal");
        _model.SetProcessParameterValue("degree_day_factor_min", 2.0f);
        _model.SetProcessParameterValue("degree_day_factor_max", 6.0f);
        _model.SetProcessParameterValue("melting_temperature", 0.0f);
        _model.OutputProcessToSameBrick();
        _model.AddProcessLogging("output");

        _model.AddBrickProcess("meltwater", "outflow:direct");
        _model.AddProcessOutput("outlet");
        _model.AddProcessLogging("output");

        _model.AddHydroUnitSplitter("snow_rain", "snow_rain:linear");
        _model.AddSplitterForcing("precipitation");
        _model.AddSplitterForcing("temperature");
        _model.AddSplitterOutput("outlet");                       // rain
        _model.AddSplitterOutput("snowpack", ContentType::Snow);  // snow
        _model.AddSplitterParameter("transition_start", -2.0f);
        _model.AddSplitterParameter("transition_end", 0.0f);

        _model.AddLoggingToItem("outlet");
    }

    // 20 mm of snowfall on the first two days (T = -5), then melt at T = 5.
    void RunAndCheckMelt(const string& start, const string& end, double startMjd, double endMjd, int meltStartDoy) {
        SettingsBasin basinSettings;
        basinSettings.AddHydroUnit(1, 100);

        SubBasin subBasin;
        EXPECT_TRUE(subBasin.Initialize(basinSettings));

        ModelHydro model(&subBasin);
        ASSERT_TRUE(model.Initialize(_model, basinSettings));

        auto precipData = std::make_unique<TimeSeriesDataRegular>(startMjd, endMjd, 1, TimeUnit::Day);
        precipData->SetValues({20.0, 20.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        tsPrecip->SetData(std::move(precipData));

        auto tempData = std::make_unique<TimeSeriesDataRegular>(startMjd, endMjd, 1, TimeUnit::Day);
        tempData->SetValues({-5.0, -5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0});
        auto tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        tsTemp->SetData(std::move(tempData));

        ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
        ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsTemp))));
        ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

        EXPECT_TRUE(model.Run());

        // Logged items: 0 = water content, 1 = SWE, 2 = melt output, 3 = meltwater output.
        vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

        double swe = 40.0;
        for (int j = 2; j < 10; ++j) {
            double expectedMelt = std::min(5.0 * ExpectedDDF(meltStartDoy + (j - 2)), swe);
            EXPECT_NEAR(unitContent[2](j, 0), expectedMelt, 0.000001);
            swe -= expectedMelt;
        }
        EXPECT_NEAR(unitContent[1](9, 0), swe, 0.000001);
    }
};

TEST_F(SeasonalMeltSnowpack, MeltsFastAroundSummerSolstice) {
    SetUpModel("2020-06-15", "2020-06-24");
    // 2020-06-17 is doy 169 (leap year); DDF is close to the maximum (6).
    RunAndCheckMelt("2020-06-15", "2020-06-24", GetMJD(2020, 6, 15), GetMJD(2020, 6, 24), 169);
}

TEST_F(SeasonalMeltSnowpack, MeltsSlowlyInWinter) {
    SetUpModel("2020-01-01", "2020-01-10");
    // 2020-01-03 is doy 3; DDF is close to the minimum (2).
    RunAndCheckMelt("2020-01-01", "2020-01-10", GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 3);
}

/**
 * Model: a forest canopy with the Menzel (1997) asymptotic filling (interception:menzel).
 * Warm temperatures (all rain, no snow) and zero PET isolate the canopy filling, so the
 * logged canopy content must follow the Menzel retention exactly.
 */
class MenzelCanopy : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;
    std::unique_ptr<TimeSeriesUniform> _tsPet;
    static constexpr double SI_MAX = 5.0;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-06", 1, "day");

        _model.GeneratePrecipitationSplitters(true);
        _model.AddLandCoverBrick("ground", "generic_land_cover");
        // Canopy with Menzel throughfall to the land cover, then out to the outlet.
        _model.GenerateCanopyInterception("ground", "ground", "interception:menzel");
        _model.GenerateSnowpacks("melt:degree_day");

        _model.SelectHydroUnitSplitter("snow_rain_transition");
        _model.AddSplitterParameter("transition_start", 0.0f);
        _model.AddSplitterParameter("transition_end", 2.0f);

        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 2.0f);

        _model.SelectHydroUnitBrick("ground_canopy");
        _model.SelectProcess("throughfall");
        _model.SetProcessParameterValue("capacity", static_cast<float>(SI_MAX));
        _model.AddBrickLogging("water_content");

        _model.SelectHydroUnitBrick("ground");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");

        _model.AddLoggingToItem("outlet");

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 6), 1, TimeUnit::Day);
        precip->SetValues({10.0, 10.0, 10.0, 10.0, 10.0, 10.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 6), 1, TimeUnit::Day);
        temp->SetValues({10.0, 10.0, 10.0, 10.0, 10.0, 10.0});  // all rain
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temp));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 6), 1, TimeUnit::Day);
        pet->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});  // no interception ET
        _tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPet->SetData(std::move(pet));
    }
};

TEST_F(MenzelCanopy, CanopyContentFollowsMenzelFilling) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // The only logged hydro-unit series is the canopy water content (cover fraction = 1).
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    // Reproduce the Menzel filling: retained_{t} = min(S + (si_max - S)(1 - e^{-rain/si_max}), si_max).
    double s = 0.0;
    for (int j = 0; j < 6; ++j) {
        double deltaSi = (SI_MAX - s) * (1.0 - std::exp(-10.0 / SI_MAX));
        s = std::min(s + deltaSi, SI_MAX);
        EXPECT_NEAR(unitContent[0](j, 0), s, 0.0001);
    }
    // Asymptotically the canopy approaches (but never exceeds) its capacity.
    EXPECT_LT(unitContent[0](5, 0), SI_MAX);
    EXPECT_GT(unitContent[0](5, 0), 4.9);
}
