#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"
#include <memory>

/**
 * Model: simple model without any brick
 */

class Splitters : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;

    void SetUp() override {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(Temperature);
        _tsTemp->SetData(std::move(temperature));
    }
    void TearDown() override {
        // Automatic cleanup via unique_ptr
    }
};

TEST_F(Splitters, SnowRain) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.AddHydroUnitSplitter("snow_rain", "snow_rain");
    _model.AddSplitterForcing("precipitation");
    _model.AddSplitterForcing("temperature");
    _model.AddSplitterOutput("outlet");  // rain
    _model.AddSplitterOutput("outlet");  // snow
    _model.AddSplitterLogging("rain");
    _model.AddSplitterLogging("snow");
    _model.AddSplitterParameter("transition_start", 0.0f);
    _model.AddSplitterParameter("transition_end", 2.0f);
    _model.AddLoggingToItem("outlet");

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting sum (only the sum of rain and snow)
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check rain/snow splitting
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedRain = {0.0, 0.0, 0.0, 0.0, 2.5, 5.0, 7.5, 10.0, 10.0, 0.0};
    vecDouble expectedSnow = {0.0, 10.0, 10.0, 10.0, 7.5, 5.0, 2.5, 0.0, 0.0, 0.0};

    for (int j = 0; j < expectedRain.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedRain[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedSnow[j], 0.000001);
    }
}
