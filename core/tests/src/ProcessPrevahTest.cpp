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
 * Model: a land cover with a canopy whose evaporation uses the PREVAH snow-albedo
 * reduction (et:open_water_prevah). The canopy is filled by warm rain on day 1, snow
 * falls on day 2, and the PET starts on day 3, so the expected canopy ET is analytic:
 * PET x (1 - albedo)/0.8 with the albedo driven by the unit's snow cover.
 */
class EtPrevahCanopy : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;
    std::unique_ptr<TimeSeriesUniform> _tsPet;

    void SetUpModel() {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-05", 1, "day");

        _model.GeneratePrecipitationSplitters(true);
        _model.AddLandCoverBrick("ground", "generic_land_cover");
        // Canopy on the rain path with the PREVAH albedo-reduced evaporation.
        _model.GenerateCanopyInterception("ground", "ground", "outflow:threshold", "et:open_water_prevah");
        _model.GenerateSnowpacks("melt:degree_day");

        _model.SelectHydroUnitSplitter("snow_rain_transition");
        _model.AddSplitterParameter("transition_start", 0.0f);
        _model.AddSplitterParameter("transition_end", 2.0f);

        // No melt during the test (high melting temperature).
        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 20.0f);

        _model.SelectHydroUnitBrick("ground_canopy");
        _model.SelectProcess("throughfall");
        _model.SetProcessParameterValue("capacity", 5.0f);
        _model.SelectProcess("interception_et");
        _model.AddProcessLogging("output");

        _model.SelectHydroUnitBrick("ground");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");

        _model.AddLoggingToItem("outlet");
    }

    // Day 1: warm rain fills the canopy. Day 2: snowfall (cold). Days 3-5: dry, PET = 2.
    void SetForcing(double tempDay2) {
        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
        precip->SetValues({10.0, 20.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
        temp->SetValues({10.0, tempDay2, 0.0, 0.0, 0.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temp));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
        pet->SetValues({0.0, 0.0, 2.0, 2.0, 2.0});
        _tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPet->SetData(std::move(pet));
    }

    axd RunAndGetCanopyEt() {
        SettingsBasin basinSettings;
        basinSettings.AddHydroUnit(1, 100);

        SubBasin subBasin;
        EXPECT_TRUE(subBasin.Initialize(basinSettings));

        ModelHydro model(&subBasin);
        EXPECT_TRUE(model.Initialize(_model, basinSettings));

        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
        EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());

        EXPECT_TRUE(model.Run());

        // The only logged hydro-unit series is the canopy interception ET output.
        vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
        return unitContent[0].col(0);
    }
};

TEST_F(EtPrevahCanopy, SnowFreeMatchesOpenWaterEt) {
    SetUpModel();
    SetForcing(10.0);  // day 2 also warm: all rain, no snow at all.

    axd et = RunAndGetCanopyEt();

    // No snow: albedo = albedo_land = 0.2 -> factor 1 -> canopy ET = PET (content ok).
    EXPECT_NEAR(et(2), 2.0, 1e-6);
    EXPECT_NEAR(et(3), 2.0, 1e-6);
}

TEST_F(EtPrevahCanopy, SnowCoverReducesCanopyEt) {
    SetUpModel();
    SetForcing(-5.0);  // day 2 snows; T = 0 afterwards keeps the snow (melting temp 20).

    axd et = RunAndGetCanopyEt();

    // Snow-covered: canopy ET = PET * (1 - albedo)/0.8 with the age-dependent snow
    // albedo, so it is reduced (in [0.375, 1.5] for PET = 2) and rises as the snow ages.
    for (int j = 2; j <= 4; ++j) {
        EXPECT_GT(et(j), 0.375 - 1e-6);
        EXPECT_LT(et(j), 1.5 + 1e-6);
    }
    EXPECT_GT(et(3), et(2));
    EXPECT_GT(et(4), et(3));
}

/**
 * Model: a snowpack with the PREVAH snow evaporation (sublimation:prevah). Snow falls on
 * day 1, PET starts on day 2, so the logged snow-evaporation output must equal
 * PET * (1 - albedo)/0.8 with the snowpack's age-dependent albedo.
 */
class SublimationPrevah : public ::testing::Test {
  protected:
    SettingsModel _model;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-05", 1, "day");

        _model.GeneratePrecipitationSplitters(true);
        _model.AddLandCoverBrick("ground", "generic_land_cover");
        _model.GenerateSnowpacks("melt:degree_day");
        _model.AddSnowpackSublimation("sublimation:prevah");

        _model.SelectHydroUnitSplitter("snow_rain_transition");
        _model.AddSplitterParameter("transition_start", 0.0f);
        _model.AddSplitterParameter("transition_end", 2.0f);

        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 20.0f);  // no melt
        _model.SelectProcess("sublimation");
        _model.AddProcessLogging("output");

        _model.SelectHydroUnitBrick("ground");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");

        _model.AddLoggingToItem("outlet");
    }
};

TEST_F(SublimationPrevah, EvaporatesAtAgedAlbedoReducedPet) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
    precip->SetValues({50.0, 0.0, 0.0, 0.0, 0.0});  // one big snowfall on day 1
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    tsPrecip->SetData(std::move(precip));
    auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
    temp->SetValues({-5.0, -5.0, -5.0, -5.0, -5.0});  // all snow, no melt
    auto tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
    tsTemp->SetData(std::move(temp));
    auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
    pet->SetValues({0.0, 2.0, 2.0, 2.0, 2.0});
    auto tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
    tsPet->SetData(std::move(pet));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // The only logged hydro-unit series is the snow-evaporation output.
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    // Snow evaporation = PET * (1 - albedo)/0.8 with the age-dependent snow albedo, so
    // for PET = 2 it lies in [0.375, 1.5] and rises as the snow ages (albedo drops).
    for (int j = 1; j <= 3; ++j) {
        EXPECT_GT(unitContent[0](j, 0), 0.375 - 1e-6);
        EXPECT_LT(unitContent[0](j, 0), 1.5 + 1e-6);
    }
    EXPECT_GT(unitContent[0](2, 0), unitContent[0](1, 0));
    EXPECT_GT(unitContent[0](3, 0), unitContent[0](2, 0));
}

TEST_F(SublimationPrevah, HasPriorityOverMeltWhenPackDrains) {
    // PREVAH serves the snow evaporation sequentially BEFORE melt (s_SNOEVAP1 then
    // s_snowmelt), so on a day the melt demand alone would drain the pack, the
    // sublimation still gets its full potential rate and melt only takes the rest.
    _model.SelectHydroUnitBrick("ground_snowpack");
    _model.SelectProcess("melt");
    _model.SetProcessParameterValue("degree_day_factor", 30.0f);   // huge melt demand
    _model.SetProcessParameterValue("melting_temperature", 0.0f);  // melt active

    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
    precip->SetValues({10.0, 0.0, 0.0, 0.0, 0.0});  // one snowfall, then a warm drain day
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    tsPrecip->SetData(std::move(precip));
    auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
    temp->SetValues({-5.0, 5.0, 5.0, 5.0, 5.0});  // melt demand 150 mm/d vs a ~10 mm pack
    auto tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
    tsTemp->SetData(std::move(temp));
    auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
    pet->SetValues({0.0, 2.0, 2.0, 2.0, 2.0});
    auto tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
    tsPet->SetData(std::move(pet));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    // Day 2 (index 1): snow age 1 -> albedo = 0.4 + 0.45 exp(-0.15) = 0.78732, so the
    // sublimation demand is 2 * (1 - albedo)/0.8 = 0.53170 mm. The melt demand (150 mm)
    // dwarfs the pack; proportional sharing would leave the sublimation ~0.035 mm, the
    // PREVAH priority preserves the full potential rate.
    EXPECT_NEAR(unitContent[0](1, 0), 0.53170, 1e-4);
    // Day 3: the pack fully drained the day before, nothing left to evaporate.
    EXPECT_NEAR(unitContent[0](2, 0), 0.0, 1e-6);
}

/**
 * Model: a snowpack with the PREVAH liquid-water release (outflow:snow_holding_prevah)
 * and a degree-day melt, with the CEXLIQ graded partition of the fresh melt exercised
 * against a hand-computed PREVAH trace (CWH = 0.1, CEXLIQ = 0.5).
 *
 * Day 1: 100 mm snowfall at -5 degC (no melt, no release).
 * Day 2-4: +2 degC, degree-day factor 5 -> M = 10 mm/d melt.
 */
class SnowHoldingPrevahCexliq : public ::testing::Test {
  protected:
    SettingsModel _model;

    void SetUpWithExponent(float cexliq) {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-04", 1, "day");

        _model.GeneratePrecipitationSplitters(true);
        _model.AddLandCoverBrick("ground", "generic_land_cover");
        _model.GenerateSnowpacksWithWaterRetention("melt:degree_day", "outflow:snow_holding_prevah", false);

        _model.SelectHydroUnitSplitter("snow_rain_transition");
        _model.AddSplitterParameter("transition_start", 0.0f);
        _model.AddSplitterParameter("transition_end", 2.0f);

        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 5.0f);
        _model.SetProcessParameterValue("melting_temperature", 0.0f);
        _model.SelectProcess("meltwater");
        _model.SetProcessParameterValue("water_holding_capacity", 0.1f);
        _model.SetProcessParameterValue("melting_temperature", 0.0f);
        _model.SetProcessParameterValue("liquid_release_exponent", cexliq);
        _model.AddProcessLogging("output");

        _model.SelectHydroUnitBrick("ground");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");

        _model.AddLoggingToItem("outlet");
    }

    vecAxxd Run() {
        SettingsBasin basinSettings;
        basinSettings.AddHydroUnit(1, 100);
        SubBasin subBasin;
        EXPECT_TRUE(subBasin.Initialize(basinSettings));
        ModelHydro model(&subBasin);
        EXPECT_TRUE(model.Initialize(_model, basinSettings));

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 4), 1, TimeUnit::Day);
        precip->SetValues({100.0, 0.0, 0.0, 0.0});
        auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        tsPrecip->SetData(std::move(precip));
        auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 4), 1, TimeUnit::Day);
        temp->SetValues({-5.0, 2.0, 2.0, 2.0});
        auto tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        tsTemp->SetData(std::move(temp));

        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsTemp))));
        EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());
        EXPECT_TRUE(model.Run());

        return model.GetLogger()->GetHydroUnitValues();
    }
};

TEST_F(SnowHoldingPrevahCexliq, GradedReleaseMatchesPrevahTrace) {
    SetUpWithExponent(0.5f);
    vecAxxd out = Run();  // meltwater (release) output per day

    // Day 1: all snow at -5 degC, no melt and no liquid -> no release.
    EXPECT_NEAR(out[0](0, 0), 0.0, 1e-6);
    // Day 2: melt M = 10, liquid before melt L = 0 -> atsliq = 0, release = (1-CWH)*C
    //        = 0.9 * 10 = 9; the pack keeps CWH*C = 1 mm liquid.
    EXPECT_NEAR(out[0](1, 0), 9.0, 1e-4);
    // Day 3: L = 1 (kept), M = 10, C = 11; solid after melt = 80, sliqmax = 8,
    //        atsliq = (1/8)^0.5 = 0.353553; release = 0.9*11 + 0.1*0.353553*10 = 10.25355.
    EXPECT_NEAR(out[0](2, 0), 10.25355, 1e-4);
    // Day 4: L = 0.746447, M = 10, C = 10.746447; solid after = 70, sliqmax = 7,
    //        atsliq = (0.746447/7)^0.5 = 0.326550; release = 0.9*C + 0.1*atsliq*10 = 9.99835.
    EXPECT_NEAR(out[0](3, 0), 9.99835, 1e-4);
}

TEST_F(SnowHoldingPrevahCexliq, ZeroExponentIsPlainCollapse) {
    SetUpWithExponent(0.0f);
    vecAxxd out = Run();

    // With CEXLIQ = 0 the graded term vanishes: every melt day releases (1-CWH)*C.
    // Day 3: C = 11 -> release = 0.9 * 11 = 9.9 (vs 10.25355 with CEXLIQ = 0.5).
    EXPECT_NEAR(out[0](1, 0), 9.0, 1e-4);
    EXPECT_NEAR(out[0](2, 0), 9.9, 1e-4);
}

/**
 * Model: a fast baseflow store (slz1) with the PREVAH SLOWCOMP overflow
 * (outflow:slowcomp) fed by a draining source, checked against a hand-computed trace.
 * The store fills asymptotically toward max_content with its baseflow time constant, so
 * it overflows the excess inflow to the slow stores EVEN far below max_content.
 *
 * Euler solver (one evaluation per step) so the trace is exact. A land cover receives
 * 100 mm of rain on day 1 and drains at k = 0.5/d into slz1; slz1 has baseflow
 * k1 = 0.024/d and max_content = 112.9 mm.
 */
class SlowcompOverflow : public ::testing::Test {
  protected:
    SettingsModel _model;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-04", 1, "day");

        // A solver storage 'src' receives the rain and drains it into slz1 (so the
        // inflow to slz1 is a dynamic solver flux, as the percolation is in the model).
        _model.GeneratePrecipitationSplitters(false);
        _model.AddHydroUnitBrick("src", "storage");
        _model.SelectHydroUnitSplitter("rain_splitter");
        _model.AddSplitterOutput("src");
        _model.SelectHydroUnitBrick("src");
        _model.AddBrickProcess("outflow", "outflow:linear", "slz1");
        _model.SetProcessParameterValue("response_factor", 0.5f);
        _model.AddProcessLogging("output");

        _model.AddHydroUnitBrick("slz1", "storage");
        _model.SelectHydroUnitBrick("slz1");
        _model.AddBrickProcess("baseflow1", "outflow:linear", "outlet");
        _model.SetProcessParameterValue("response_factor", 0.024f);
        _model.AddBrickProcess("overflow", "outflow:slowcomp", "outlet");
        _model.SetProcessParameterValue("max_content", 112.9f);
        _model.AddProcessLogging("output");

        _model.AddLoggingToItem("outlet");
    }
};

TEST_F(SlowcompOverflow, DivertsExcessInflowBelowCap) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 4), 1, TimeUnit::Day);
    precip->SetValues({100.0, 0.0, 0.0, 0.0});
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    tsPrecip->SetData(std::move(precip));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());
    EXPECT_TRUE(model.Run());

    // out[0] = src outflow (0, 50, 25, 12.5), out[1] = slz1 SLOWCOMP overflow.
    vecAxxd out = model.GetLogger()->GetHydroUnitValues();

    // Days 1-2 are the source->slz1 fill warmup (the rain reaches src at the end of day
    // 1, and its outflow reaches slz1 as content over day 2). By day 3 the store holds
    // 50 mm (the day-2 inflow it could not divert while empty) and the overflow follows
    // the SLOWCOMP rule exactly: with inflow 25 the store can absorb only
    // (max_content - content)*k1 = (112.9 - 50)*0.024 = 1.51, so it overflows
    // 25 - 1.51 = 23.49 to the slow stores, though it is well below the 112.9 mm cap
    // (a plain bucket would divert nothing here).
    EXPECT_NEAR(out[1](2, 0), 23.4904, 1e-3);
    // Day 4: inflow 12.5, content 50.31 -> overflow 12.5 - (112.9 - 50.31)*0.024 = 11.00.
    EXPECT_NEAR(out[1](3, 0), 10.998, 1e-3);
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

/**
 * Model: land cover(s) with snowpacks feeding a soil storage carrying the PREVAH ET
 * (et:prevah). The PET is zero during the filling days and constant afterwards, so the
 * expected ET rate is analytic: PET x (1 - albedo)/0.8, with the albedo interpolated by
 * the snow-covered fraction (lp is set low so the soil-moisture ratio stays at 1).
 */
class EtPrevahSoil : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;
    std::unique_ptr<TimeSeriesUniform> _tsPet;

    void SetUpModel(const std::vector<string>& covers, float meltingTemperature2 = 20.0f) {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-05", 1, "day");

        _model.GeneratePrecipitationSplitters(true);
        for (const auto& cover : covers) {
            _model.AddLandCoverBrick(cover, "generic_land_cover");
        }
        _model.GenerateSnowpacks("melt:degree_day");

        _model.SelectHydroUnitSplitter("snow_rain_transition");
        _model.AddSplitterParameter("transition_start", 0.0f);
        _model.AddSplitterParameter("transition_end", 2.0f);

        // First snowpack: never melts (high melting temperature). Second (if any):
        // melts instantly (low melting temperature, large factor).
        for (size_t i = 0; i < covers.size(); ++i) {
            _model.SelectHydroUnitBrick(covers[i] + "_snowpack");
            _model.SelectProcess("melt");
            _model.SetProcessParameterValue("degree_day_factor", 10.0f);
            _model.SetProcessParameterValue("melting_temperature", i == 0 ? 20.0f : meltingTemperature2);
        }

        for (const auto& cover : covers) {
            _model.SelectHydroUnitBrick(cover);
            _model.AddBrickProcess("outflow", "outflow:direct", "soil");
        }

        // Soil storage with the PREVAH ET: lp low so the moisture ratio stays at 1.
        _model.AddHydroUnitBrick("soil", "storage");
        _model.AddBrickParameter("capacity", 100.0f);
        _model.AddBrickProcess("et", "et:prevah");
        _model.SetProcessParameterValue("lp", 0.05f);
        _model.AddProcessLogging("output");
        _model.AddBrickProcess("outflow", "outflow:linear", "outlet");
        _model.SetProcessParameterValue("response_factor", 0.0001f);

        _model.AddLoggingToItem("outlet");
    }

    // Day 1: warm rain (fills the soil). Day 2: snowfall (cold). Days 3-5: dry, PET = 2.
    void SetForcing(double tempDay3Plus) {
        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
        precip->SetValues({20.0, 20.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
        temp->SetValues({10.0, -5.0, tempDay3Plus, tempDay3Plus, tempDay3Plus});
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temp));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
        pet->SetValues({0.0, 0.0, 2.0, 2.0, 2.0});
        _tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPet->SetData(std::move(pet));
    }

    // Run over hydro unit(s) with the given land cover fractions; return the ET series.
    axd RunAndGetEt(const std::vector<std::pair<string, double>>& coverFractions) {
        SettingsBasin basinSettings;
        basinSettings.AddHydroUnit(1, 100);
        for (const auto& [name, fraction] : coverFractions) {
            basinSettings.AddLandCover(name, "generic_land_cover", fraction);
        }

        SubBasin subBasin;
        EXPECT_TRUE(subBasin.Initialize(basinSettings));

        ModelHydro model(&subBasin);
        EXPECT_TRUE(model.Initialize(_model, basinSettings));

        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
        EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());

        EXPECT_TRUE(model.Run());

        // The only logged hydro-unit series is the soil ET output.
        vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
        return unitContent[0].col(0);
    }
};

TEST_F(EtPrevahSoil, SnowFreeMatchesHbvEt) {
    SetUpModel({"ground"});
    SetForcing(10.0);  // warm: the day-2 precipitation partly rains, snow melts at T=10?
    // Use a warm day 2 as well: override the temperature series (all warm, no snow at all).
    auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
    temp->SetValues({10.0, 10.0, 10.0, 10.0, 10.0});
    _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
    _tsTemp->SetData(std::move(temp));

    axd et = RunAndGetEt({{"ground", 1.0}});

    // No snow: albedo = albedo_land = 0.2 -> factor (1 - 0.2)/0.8 = 1 -> ET = PET.
    EXPECT_NEAR(et(2), 2.0, 1e-6);
    EXPECT_NEAR(et(3), 2.0, 1e-6);
    EXPECT_NEAR(et(4), 2.0, 1e-6);
}

TEST_F(EtPrevahSoil, SnowCoverReducesEtByAgedAlbedo) {
    SetUpModel({"ground"});
    SetForcing(0.0);  // day 2 snows; days 3-5 at T=0 keep the snow (melting temp 20).

    axd et = RunAndGetEt({{"ground", 1.0}});

    // Snow-covered: ET = PET * (1 - albedo)/0.8 with the age-dependent snow albedo
    // (0.4 .. 0.85), so the soil ET sits between 2*0.1875 = 0.375 and 2*0.75 = 1.5, and
    // rises as the snow ages (the albedo drops).
    for (int j = 2; j <= 4; ++j) {
        EXPECT_GT(et(j), 0.375 - 1e-6);
        EXPECT_LT(et(j), 1.5 + 1e-6);
    }
    EXPECT_GT(et(3), et(2));  // aging snow -> lower albedo -> more ET
    EXPECT_GT(et(4), et(3));
}

TEST_F(EtPrevahSoil, PartialSnowCoverInterpolatesAlbedo) {
    // Two covers (50/50). The second snowpack melts instantly at T = 5 (melting
    // temperature -10, factor 10), so from day 4 only half the unit is snow-covered.
    SetUpModel({"ground1", "ground2"}, -10.0f);
    SetForcing(5.0);

    axd et = RunAndGetEt({{"ground1", 0.5}, {"ground2", 0.5}});

    // The snowpacks are processed before the soil, so the second snowpack's melt
    // (T = 5, instant) empties it before the ET is evaluated: only half the unit stays
    // snow-covered. The albedo is area-weighted (0.2 over the snow-free half, the snow
    // albedo 0.4..0.85 over the other half) -> albedo in [0.3, 0.525] -> factor in
    // [0.59, 0.875] -> ET in [1.19, 1.75]: reduced, but less so than under full cover.
    EXPECT_GT(et(2), 1.0);  // partial cover -> above the full-snow reduction
    EXPECT_LT(et(2), 2.0);  // still below the snow-free rate
}

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
