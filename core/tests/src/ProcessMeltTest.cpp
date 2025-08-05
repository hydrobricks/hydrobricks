#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snowpack model
 */

class SnowpackModel : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsPrecip{};
    TimeSeriesUniform* _tsTemp{};

    void SetUp() override {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        // Snowpack brick
        _model.AddHydroUnitBrick("snowpack", "snowpack");
        _model.AddBrickLogging({"water_content", "snow_content"});

        // Snow melt process
        _model.AddBrickProcess("melt", "melt:degree_day");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 2.0f);
        _model.OutputProcessToSameBrick();
        _model.AddProcessLogging("output");

        // Add process to direct meltwater to the outlet
        _model.AddBrickProcess("meltwater", "outflow:direct");
        _model.AddProcessOutput("outlet");
        _model.AddProcessLogging("output");

        // Rain/snow splitter
        _model.AddHydroUnitSplitter("snow_rain", "snow_rain");
        _model.AddSplitterForcing("precipitation");
        _model.AddSplitterForcing("temperature");
        _model.AddSplitterOutput("outlet");            // rain
        _model.AddSplitterOutput("snowpack", "snow");  // snow
        _model.AddSplitterParameter("transition_start", 0.0f);
        _model.AddSplitterParameter("transition_end", 2.0f);

        _model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        _tsTemp = new TimeSeriesUniform(Temperature);
        _tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
        wxDELETE(_tsTemp);
    }
};

TEST_F(SnowpackModel, DegreeDay) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    Logger* logger = model.GetLogger();
    axd modelOutputs = logger->GetOutletDischarge();
    vecDouble expectedOutputs = {0.0, 0.0, 0.0, 5.0, 10.0, 13.0, 16.0, 19.0, 17.0, 0.0};

    for (int j = 0; j < modelOutputs.size(); ++j) {
        EXPECT_NEAR(modelOutputs[j], expectedOutputs[j], 0.000001);
    }

    // Check melt and swe
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedWaterRetention = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedSWE = {0.0, 10.0, 20.0, 25.0, 25.0, 22.0, 16.0, 7.0, 0.0, 0.0};
    vecDouble expectedMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 7.0, 0.0};

    for (int j = 0; j < expectedSWE.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedWaterRetention[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedSWE[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 0), expectedMelt[j], 0.000001);
    }
}

TEST_F(SnowpackModel, ModelClosesBalance) {
    wxLogNull logNo;

    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 1000);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinProp));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

class SnowpackModelWithAspect : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsPrecip{};
    TimeSeriesUniform* _tsTemp{};

    void SetUp() override {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        // Snowpack brick
        _model.AddHydroUnitBrick("snowpack", "snowpack");
        _model.AddBrickLogging({"water_content", "snow_content"});

        // Snow melt process
        _model.AddBrickProcess("melt", "melt:degree_day_aspect");
        _model.SetProcessParameterValue("degree_day_factor_n", 2.0f);
        _model.SetProcessParameterValue("degree_day_factor_ew", 3.0f);
        _model.SetProcessParameterValue("degree_day_factor_s", 4.0f);
        _model.SetProcessParameterValue("melting_temperature", 2.0f);
        _model.OutputProcessToSameBrick();
        _model.AddProcessLogging("output");

        // Add process to direct meltwater to the outlet
        _model.AddBrickProcess("meltwater", "outflow:direct");
        _model.AddProcessOutput("outlet");
        _model.AddProcessLogging("output");

        // Rain/snow splitter
        _model.AddHydroUnitSplitter("snow_rain", "snow_rain");
        _model.AddSplitterForcing("precipitation");
        _model.AddSplitterForcing("temperature");
        _model.AddSplitterOutput("outlet");            // rain
        _model.AddSplitterOutput("snowpack", "snow");  // snow
        _model.AddSplitterParameter("transition_start", 0.0f);
        _model.AddSplitterParameter("transition_end", 2.0f);

        _model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        _tsTemp = new TimeSeriesUniform(Temperature);
        _tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
        wxDELETE(_tsTemp);
    }
};

TEST_F(SnowpackModelWithAspect, DegreeDayAspect) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddHydroUnit(1, 200);  // Note that it's larger (South facing)

    // Set the aspect of the hydro units
    basinSettings.SelectUnit(0);
    basinSettings.AddHydroUnitPropertyString("aspect_class", "N");
    basinSettings.SelectUnit(1);
    basinSettings.AddHydroUnitPropertyString("aspect_class", "E");
    basinSettings.SelectUnit(2);
    basinSettings.AddHydroUnitPropertyString("aspect_class", "W");
    basinSettings.SelectUnit(3);
    basinSettings.AddHydroUnitPropertyString("aspect_class", "S");

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    Logger* logger = model.GetLogger();
    axd modelOutputs = logger->GetOutletDischarge();
    vecDouble expectedOutputs = {0.0, 0.0, 0.0, 5.0, 10.0, 13.2, 16.4, 19.6, 15.6, 0.2};

    for (int j = 0; j < modelOutputs.size(); ++j) {
        EXPECT_NEAR(modelOutputs[j], expectedOutputs[j], 0.000001);
    }
}

class GlacierModel : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsTemp{};

    void SetUp() override {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        // Glacier brick
        _model.AddHydroUnitBrick("glacier", "glacier");

        // Glacier melt process
        _model.AddBrickProcess("melt", "melt:degree_day");
        _model.AddBrickParameter("infinite_storage", 1.0);
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 2.0f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        _model.AddLoggingToItem("outlet");

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        _tsTemp = new TimeSeriesUniform(Temperature);
        _tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(_tsTemp);
    }
};

TEST_F(GlacierModel, UnlimitedSupply) {
    wxLogNull logNo;

    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("glacier", "glacier", 1);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    Logger* logger = model.GetLogger();
    axd modelOutputs = logger->GetOutletDischarge();
    vecDouble expectedOutputs = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 18.0, 21.0};

    for (int j = 0; j < modelOutputs.size(); ++j) {
        EXPECT_NEAR(modelOutputs[j], expectedOutputs[j], 0.000001);
    }

    // Check melt and swe
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 18.0, 21.0};

    for (int j = 0; j < expectedMelt.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedMelt[j], 0.000001);
    }
}

class GlacierModelWithSnowpack : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsTemp{};
    TimeSeriesUniform* _tsPrecip{};

    void SetUp() override {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-08", 1, "day");

        // Precipitation
        _model.GeneratePrecipitationSplitters(true);

        // Glacier brick
        _model.AddLandCoverBrick("glacier", "glacier");

        // Snowpack brick
        _model.GenerateSnowpacks("melt:degree_day");

        // Snow melt process
        _model.SelectHydroUnitBrick("glacier_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 2.0f);
        _model.AddProcessLogging("output");

        // Glacier melt process
        _model.SelectHydroUnitBrick("glacier");
        _model.AddBrickProcess("melt", "melt:degree_day", "outlet");
        _model.AddBrickParameter("no_melt_when_snow_cover", true);
        _model.AddBrickParameter("infinite_storage", 1.0);
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.AddProcessLogging("output");

        // Add process to direct meltwater to the outlet
        _model.AddBrickProcess("meltwater", "outflow:direct", "outlet", true);

        _model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        precip->SetValues({8.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        temperature->SetValues({-2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0});
        _tsTemp = new TimeSeriesUniform(Temperature);
        _tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(_tsTemp);
        wxDELETE(_tsPrecip);
    }
};

TEST_F(GlacierModelWithSnowpack, NoIceMeltIfSnowCover) {
    wxLogNull logNo;

    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 1000);
    basinProp.AddLandCover("glacier", "glacier", 1);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinProp));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Check resulting discharge
    vecAxd basinOutputs = logger->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 4.0, 10.0, 6.0, 6.0, 6.0, 6.0, 6.0};
    axd modelOutputs = logger->GetOutletDischarge();

    for (int j = 0; j < expectedOutputs.size(); ++j) {
        EXPECT_NEAR(modelOutputs[j], expectedOutputs[j], 0.000001);
    }

    // Check melt and swe
    vecAxxd unitOutput = logger->GetHydroUnitValues();

    vecDouble expectedSnowMelt = {0.0, 4.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedIceMelt = {0.0, 0.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0};

    for (int j = 0; j < expectedIceMelt.size(); ++j) {
        EXPECT_NEAR(unitOutput[0](j, 0), expectedIceMelt[j], 0.000001);
        EXPECT_NEAR(unitOutput[2](j, 0), expectedSnowMelt[j], 0.000001);
    }
}

TEST_F(GlacierModelWithSnowpack, ModelClosesBalance) {
    wxLogNull logNo;

    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 1000);
    basinProp.AddLandCover("glacier", "glacier", 1);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinProp));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 8;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

class MultiGlaciersModelWithTemperatureIndex : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsTemp{};
    TimeSeriesUniform* _tsPrecip{};
    TimeSeriesUniform* _tsRad{};

    void SetUp() override {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-08", 1, "day");
        _model.SetLogAll(true);

        // Precipitation
        _model.GeneratePrecipitationSplitters(true);

        // Glacier land cover bricks
        _model.AddLandCoverBrick("ground", "ground");
        _model.AddLandCoverBrick("glacier_ice", "glacier");
        _model.AddLandCoverBrick("glacier_debris", "glacier");

        // Snowpack brick
        _model.GenerateSnowpacks("melt:temperature_index");

        // Glacier melt process
        _model.SelectHydroUnitBrick("glacier_ice");
        _model.AddBrickProcess("melt", "melt:temperature_index", "outlet");
        _model.AddProcessLogging("output");
        _model.AddBrickProcess("meltwater", "outflow:direct", "outlet", true);
        _model.AddBrickParameter("no_melt_when_snow_cover", true);
        _model.AddBrickParameter("infinite_storage", 1.0);
        _model.SelectHydroUnitBrick("glacier_debris");
        _model.AddBrickProcess("melt", "melt:temperature_index", "outlet");
        _model.AddProcessLogging("output");
        _model.AddBrickProcess("meltwater", "outflow:direct", "outlet", true);
        _model.AddBrickParameter("no_melt_when_snow_cover", true);
        _model.AddBrickParameter("infinite_storage", 1.0);

        // Route to the outlet
        _model.SelectHydroUnitBrick("ground");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");
        _model.SelectHydroUnitBrick("glacier_ice");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");
        _model.SelectHydroUnitBrick("glacier_debris");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");

        _model.AddLoggingToItem("outlet");

        // Set melt process parameters
        EXPECT_TRUE(_model.SetParameterValue("type:snowpack", "radiation_coefficient", 0.0006f));
        EXPECT_TRUE(_model.SetParameterValue("type:glacier", "radiation_coefficient", 0.001f));

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        precip->SetValues({8.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        temperature->SetValues({-2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0});
        _tsTemp = new TimeSeriesUniform(Temperature);
        _tsTemp->SetData(temperature);

        auto radiation = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        radiation->SetValues({100, 110, 120, 130, 140, 150, 160, 170});
        _tsRad = new TimeSeriesUniform(Radiation);
        _tsRad->SetData(radiation);
    }
    void TearDown() override {
        wxDELETE(_tsRad);
        wxDELETE(_tsTemp);
        wxDELETE(_tsPrecip);
    }
};

TEST_F(MultiGlaciersModelWithTemperatureIndex, TemperatureIndexMelt) {
    wxLogNull logNo;

    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 1000);
    basinProp.AddLandCover("ground", "ground", 0.2);
    basinProp.AddLandCover("glacier_ice", "glacier", 0.6);
    basinProp.AddLandCover("glacier_debris", "glacier", 0.2);
    basinProp.AddHydroUnit(2, 1000);
    basinProp.AddLandCover("ground", "ground", 0.5);
    basinProp.AddLandCover("glacier_ice", "glacier", 0.3);
    basinProp.AddLandCover("glacier_debris", "glacier", 0.2);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinProp));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(_tsRad));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 8;
    double totalGlacierIceMelt = logger->GetTotalHydroUnits("glacier_ice:melt:output");
    double totalGlacierDebrisMelt = logger->GetTotalHydroUnits("glacier_debris:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierIceMelt - totalGlacierDebrisMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}
