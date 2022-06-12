#include <gtest/gtest.h>
#include <wx/stdpaths.h>

#include "ModelHydro.h"
#include "ProcessOutflowLinear.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

class ModelBasics : public ::testing::Test {
  protected:
    SettingsModel m_model1;
    SettingsModel m_model2;
    TimeSeriesUniform* m_tsPrecip{};

    void SetUp() override {
        // Model 1: simple linear storage
        m_model1.SetSolver("EulerExplicit");
        m_model1.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_model1.AddHydroUnitBrick("storage", "Storage");
        m_model1.AddBrickForcing("Precipitation");
        m_model1.AddBrickLogging("content");
        m_model1.AddBrickProcess("outflow", "Outflow:linear");
        m_model1.AddProcessParameter("responseFactor", 0.3f);
        m_model1.AddProcessLogging("output");
        m_model1.AddProcessOutput("outlet");
        m_model1.AddLoggingToItem("outlet");

        // Model 2: 2 linear storages in cascade
        m_model2.SetSolver("EulerExplicit");
        m_model2.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_model2.AddHydroUnitBrick("storage-1", "Storage");
        m_model2.AddBrickForcing("Precipitation");
        m_model2.AddBrickLogging("content");
        m_model2.AddBrickProcess("outflow", "Outflow:linear");
        m_model2.AddProcessParameter("responseFactor", 0.5f);
        m_model2.AddProcessLogging("output");
        m_model2.AddProcessOutput("storage-2");
        m_model2.AddHydroUnitBrick("storage-2", "Storage");
        m_model2.AddBrickLogging("content");
        m_model2.AddBrickProcess("outflow", "Outflow:linear");
        m_model2.AddProcessParameter("responseFactor", 0.3f);
        m_model2.AddProcessLogging("output");
        m_model2.AddProcessOutput("outlet");
        m_model2.AddLoggingToItem("outlet");

        auto data = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                              wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        data->SetValues({0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(data);
    }
    void TearDown() override {
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

TEST_F(ModelBasics, ModelBuildsCorrectlyFromFile) {
    SettingsBasin basinProp;
    ASSERT_TRUE(basinProp.Parse("files/hydro-units-2-glaciers.nc"));

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));
    EXPECT_NEAR(subBasin.GetArea(), 264328000, 1050); // Difference due to storage as float in netcdf file.

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

TEST_F(ModelBasics, ModelDumpsOutputs) {
    SettingsBasin basinProp;
    ASSERT_TRUE(basinProp.Parse("files/hydro-units-2-glaciers.nc"));

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    model.Initialize(m_model2);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    EXPECT_TRUE(model.DumpOutputs(wxStandardPaths::Get().GetTempDir()));
}
