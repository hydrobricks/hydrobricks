#include <gtest/gtest.h>
#include <wx/stdpaths.h>

#include "ModelHydro.h"
#include "ProcessOutflowLinear.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

class ModelBasics : public ::testing::Test {
  protected:
    SettingsModel _model1;
    SettingsModel _model2;
    TimeSeriesUniform* _tsPrecip{};

    void SetUp() override {
        // Model 1: simple linear storage
        _model1.SetLogAll(true);
        _model1.SetSolver("euler_explicit");
        _model1.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model1.AddHydroUnitBrick("storage", "storage");
        _model1.AddBrickForcing("precipitation");
        _model1.AddBrickLogging("water_content");
        _model1.AddBrickProcess("outflow", "outflow:linear");
        _model1.SetProcessParameterValue("response_factor", 0.3f);
        _model1.AddProcessLogging("output");
        _model1.AddProcessOutput("outlet");
        _model1.AddLoggingToItem("outlet");

        // Model 2: 2 linear storages in cascade
        _model2.SetLogAll(true);
        _model2.SetSolver("euler_explicit");
        _model2.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model2.AddHydroUnitBrick("storage_1", "storage");
        _model2.AddBrickForcing("precipitation");
        _model2.AddBrickLogging("water_content");
        _model2.AddBrickProcess("outflow", "outflow:linear");
        _model2.SetProcessParameterValue("response_factor", 0.5f);
        _model2.AddProcessLogging("output");
        _model2.AddProcessOutput("storage_2");
        _model2.AddHydroUnitBrick("storage_2", "storage");
        _model2.AddBrickLogging("water_content");
        _model2.AddBrickProcess("outflow", "outflow:linear");
        _model2.SetProcessParameterValue("response_factor", 0.3f);
        _model2.AddProcessLogging("output");
        _model2.AddProcessOutput("outlet");
        _model2.AddLoggingToItem("outlet");

        auto data = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        data->SetValues({0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(data);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
    }
};

TEST_F(ModelBasics, Model1BuildsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    model.Initialize(_model1, basinSettings);

    EXPECT_TRUE(model.IsOk());
}

TEST_F(ModelBasics, Model1RunsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    model.Initialize(_model1, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelBasics, Model2BuildsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    model.Initialize(_model2, basinSettings);

    EXPECT_TRUE(model.IsOk());
}

TEST_F(ModelBasics, Model2RunsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    model.Initialize(_model2, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelBasics, TimeSeriesEndsTooEarly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    model.Initialize(_model1, basinSettings);

    auto data = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 9), 1, Day);
    data->SetValues({0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecipSingleRainyDay = new TimeSeriesUniform(Precipitation);
    tsPrecipSingleRainyDay->SetData(data);

    wxLogNull logNo;
    ASSERT_FALSE(model.AddTimeSeries(tsPrecipSingleRainyDay));
}

TEST_F(ModelBasics, TimeSeriesStartsTooLate) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    model.Initialize(_model1, basinSettings);

    auto data = new TimeSeriesDataRegular(GetMJD(2020, 1, 2), GetMJD(2020, 1, 10), 1, Day);
    data->SetValues({0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecipSingleRainyDay = new TimeSeriesUniform(Precipitation);
    tsPrecipSingleRainyDay->SetData(data);

    wxLogNull logNo;
    ASSERT_FALSE(model.AddTimeSeries(tsPrecipSingleRainyDay));
}

TEST_F(ModelBasics, ModelDumpsOutputs) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    model.Initialize(_model2, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    EXPECT_TRUE(model.DumpOutputs(wxStandardPaths::Get().GetTempDir().ToStdString()));
}

TEST_F(ModelBasics, Model1WithEulerExplicitWithNoOutflowClosesBalance) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    SettingsModel settingsModel = _model1;
    settingsModel.SetSolver("euler_explicit");
    settingsModel.SelectHydroUnitBrick("storage");
    settingsModel.SelectProcess("outflow");
    settingsModel.SetProcessParameterValue("response_factor", 0.0f);

    model.Initialize(settingsModel, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.IsOk());
    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 10;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelBasics, Model1WithHeunExplicitWithNoOutflowClosesBalance) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    SettingsModel settingsModel = _model1;
    settingsModel.SetSolver("heun_explicit");
    settingsModel.SelectHydroUnitBrick("storage");
    settingsModel.SelectProcess("outflow");
    settingsModel.SetProcessParameterValue("response_factor", 0.0f);

    model.Initialize(settingsModel, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.IsOk());
    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 10;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelBasics, Model1WithRungeKuttaWithNoOutflowClosesBalance) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    SettingsModel settingsModel = _model1;
    settingsModel.SetSolver("runge_kutta");
    settingsModel.SelectHydroUnitBrick("storage");
    settingsModel.SelectProcess("outflow");
    settingsModel.SetProcessParameterValue("response_factor", 0.0f);

    model.Initialize(settingsModel, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.IsOk());
    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 10;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelBasics, Model1WithEulerExplicitClosesBalance) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    SettingsModel settingsModel = _model1;
    settingsModel.SetSolver("euler_explicit");
    model.Initialize(settingsModel, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.IsOk());
    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 10;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelBasics, Model1WithHeunExplicitClosesBalance) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    SettingsModel settingsModel = _model1;
    settingsModel.SetSolver("heun_explicit");
    model.Initialize(settingsModel, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.IsOk());
    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 10;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelBasics, Model2ClosesBalance) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    model.Initialize(_model2, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.IsOk());
    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 10;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}
