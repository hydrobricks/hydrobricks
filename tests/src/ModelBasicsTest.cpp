#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "ProcessOutflowLinear.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

class ModelBasics : public ::testing::Test {
  protected:
    SettingsModel m_model1;
    SettingsModel m_model2;
    TimeSeriesUniform* m_tsPrecip{};

    virtual void SetUp() {
        // Model 1: simple linear storage
        m_model1.SetSolver("EulerExplicit");
        m_model1.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_model1.AddBrick("storage", "Storage");
        m_model1.AddForcingToCurrentBrick("Precipitation");
        m_model1.AddLoggingToCurrentBrick("content");
        m_model1.AddProcessToCurrentBrick("outflow", "Outflow:linear");
        m_model1.AddParameterToCurrentProcess("responseFactor", 0.3f);
        m_model1.AddLoggingToCurrentProcess("output");
        m_model1.AddOutputToCurrentProcess("outlet");
        m_model1.AddLoggingToItem("outlet");

        // Model 2: 2 linear storages in cascade
        m_model2.SetSolver("EulerExplicit");
        m_model2.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_model2.AddBrick("storage-1", "Storage");
        m_model2.AddForcingToCurrentBrick("Precipitation");
        m_model2.AddLoggingToCurrentBrick("content");
        m_model2.AddProcessToCurrentBrick("outflow", "Outflow:linear");
        m_model2.AddParameterToCurrentProcess("responseFactor", 0.5f);
        m_model2.AddLoggingToCurrentProcess("output");
        m_model2.AddOutputToCurrentProcess("storage-2");
        m_model2.AddBrick("storage-2", "Storage");
        m_model2.AddLoggingToCurrentBrick("content");
        m_model2.AddProcessToCurrentBrick("outflow", "Outflow:linear");
        m_model2.AddParameterToCurrentProcess("responseFactor", 0.3f);
        m_model2.AddLoggingToCurrentProcess("output");
        m_model2.AddOutputToCurrentProcess("outlet");
        m_model2.AddLoggingToItem("outlet");

        auto data = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                              wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        data->SetValues({0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(data);
    }
    virtual void TearDown() {
        wxDELETE(m_tsPrecip);
    }
};

TEST_F(ModelBasics, Model1BuildsCorrectly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    model.Initialize(m_model1);

    EXPECT_TRUE(model.IsOk());
}

TEST_F(ModelBasics, Model1RunsCorrectly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    model.Initialize(m_model1);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelBasics, Model2BuildsCorrectly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    model.Initialize(m_model2);

    EXPECT_TRUE(model.IsOk());
}

TEST_F(ModelBasics, Model2RunsCorrectly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    model.Initialize(m_model2);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelBasics, TimeSeriesEndsTooEarly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    model.Initialize(m_model1);

    auto data = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                          wxDateTime(9, wxDateTime::Jan, 2020), 1, Day);
    data->SetValues({0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecipSingleRainyDay = new TimeSeriesUniform(Precipitation);
    tsPrecipSingleRainyDay->SetData(data);

    wxLogNull logNo;
    ASSERT_FALSE(model.AddTimeSeries(tsPrecipSingleRainyDay));
}

TEST_F(ModelBasics, TimeSeriesStartsTooLate) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    model.Initialize(m_model1);

    auto data = new TimeSeriesDataRegular(wxDateTime(2, wxDateTime::Jan, 2020),
                                          wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
    data->SetValues({0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecipSingleRainyDay = new TimeSeriesUniform(Precipitation);
    tsPrecipSingleRainyDay->SetData(data);

    wxLogNull logNo;
    ASSERT_FALSE(model.AddTimeSeries(tsPrecipSingleRainyDay));
}

