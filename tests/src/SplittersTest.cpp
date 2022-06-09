#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple model without any brick
 */

class Splitters : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};

    void SetUp() override {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");

        auto precip = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                     wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        temperature->SetValues({-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(m_tsPrecip);
        wxDELETE(m_tsTemp);
    }
};

TEST_F(Splitters, SnowRain) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.AddSplitter("snow-rain", "SnowRain");
    m_model.AddSplitterForcing("Precipitation");
    m_model.AddSplitterForcing("Temperature");
    m_model.AddSplitterOutput("outlet"); // rain
    m_model.AddSplitterOutput("outlet"); // snow
    m_model.AddSplitterLogging("rain");
    m_model.AddSplitterLogging("snow");
    m_model.AddSplitterParameter("transitionStart", 0.0f);
    m_model.AddSplitterParameter("transitionEnd", 2.0f);
    m_model.AddLoggingToItem("outlet");

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting sum (only the sum of rain and snow)
    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0};

    for (auto & basinOutput : basinOutputs) {
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
