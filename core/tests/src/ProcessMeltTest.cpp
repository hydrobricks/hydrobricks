#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snowpack model
 */

class SnowpackModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};

    void SetUp() override {
        m_model.SetSolver("heun_explicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        // Snowpack brick
        m_model.AddHydroUnitBrick("snowpack", "snowpack");
        m_model.AddBrickLogging({"content", "snow"});

        // Snow melt process
        m_model.AddBrickProcess("melt", "melt:degree_day");
        m_model.SetProcessParameterValue("degree_day_factor", 3.0f);
        m_model.SetProcessParameterValue("melting_temperature", 2.0f);
        m_model.OutputProcessToSameBrick();
        m_model.AddProcessLogging("output");

        // Add process to direct meltwater to the outlet
        m_model.AddBrickProcess("meltwater", "outflow:direct");
        m_model.AddProcessOutput("outlet");
        m_model.AddProcessLogging("output");

        // Rain/snow splitter
        m_model.AddHydroUnitSplitter("snow_rain", "snow_rain");
        m_model.AddSplitterForcing("precipitation");
        m_model.AddSplitterForcing("temperature");
        m_model.AddSplitterOutput("outlet");            // rain
        m_model.AddSplitterOutput("snowpack", "snow");  // snow
        m_model.AddSplitterParameter("transition_start", 0.0f);
        m_model.AddSplitterParameter("transition_end", 2.0f);

        m_model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(m_tsPrecip);
        wxDELETE(m_tsTemp);
    }
};

TEST_F(SnowpackModel, DegreeDay) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
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
    EXPECT_TRUE(model.Initialize(m_model, basinProp));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
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
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};

    void SetUp() override {
        m_model.SetSolver("heun_explicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        // Snowpack brick
        m_model.AddHydroUnitBrick("snowpack", "snowpack");
        m_model.AddBrickLogging({"content", "snow"});

        // Snow melt process
        m_model.AddBrickProcess("melt", "melt:degree_day_aspect");
        m_model.SetProcessParameterValue("degree_day_factor_n", 2.0f);
        m_model.SetProcessParameterValue("degree_day_factor_ew", 3.0f);
        m_model.SetProcessParameterValue("degree_day_factor_s", 4.0f);
        m_model.SetProcessParameterValue("melting_temperature", 2.0f);
        m_model.OutputProcessToSameBrick();
        m_model.AddProcessLogging("output");

        // Add process to direct meltwater to the outlet
        m_model.AddBrickProcess("meltwater", "outflow:direct");
        m_model.AddProcessOutput("outlet");
        m_model.AddProcessLogging("output");

        // Rain/snow splitter
        m_model.AddHydroUnitSplitter("snow_rain", "snow_rain");
        m_model.AddSplitterForcing("precipitation");
        m_model.AddSplitterForcing("temperature");
        m_model.AddSplitterOutput("outlet");            // rain
        m_model.AddSplitterOutput("snowpack", "snow");  // snow
        m_model.AddSplitterParameter("transition_start", 0.0f);
        m_model.AddSplitterParameter("transition_end", 2.0f);

        m_model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(m_tsPrecip);
        wxDELETE(m_tsTemp);
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
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
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
    SettingsModel m_model;
    TimeSeriesUniform* m_tsTemp{};

    void SetUp() override {
        m_model.SetSolver("heun_explicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        // Glacier brick
        m_model.AddHydroUnitBrick("glacier", "glacier");

        // Glacier melt process
        m_model.AddBrickProcess("melt", "melt:degree_day");
        m_model.SetProcessParameterValue("degree_day_factor", 3.0f);
        m_model.SetProcessParameterValue("melting_temperature", 2.0f);
        m_model.AddProcessLogging("output");
        m_model.AddProcessOutput("outlet");

        m_model.AddLoggingToItem("outlet");

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(m_tsTemp);
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
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
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
    SettingsModel m_model;
    TimeSeriesUniform* m_tsTemp{};
    TimeSeriesUniform* m_tsPrecip{};

    void SetUp() override {
        m_model.SetSolver("heun_explicit");
        m_model.SetTimer("2020-01-01", "2020-01-08", 1, "day");

        // Precipitation
        m_model.GeneratePrecipitationSplitters(true);

        // Glacier brick
        m_model.AddLandCoverBrick("glacier", "glacier");

        // Snowpack brick
        m_model.GenerateSnowpacks("melt:degree_day");

        // Snow melt process
        m_model.SelectHydroUnitBrick("glacier_snowpack");
        m_model.SetProcessParameterValue("degree_day_factor", 2.0f);
        m_model.AddProcessLogging("output");

        // Glacier melt process
        m_model.SelectHydroUnitBrick("glacier");
        m_model.AddBrickProcess("melt", "melt:degree_day", "outlet");
        m_model.AddBrickParameter("no_melt_when_snow_cover", true);
        m_model.AddBrickParameter("infinite_storage", 1.0);
        m_model.SetProcessParameterValue("degree_day_factor", 3.0f);
        m_model.AddProcessLogging("output");

        // Add process to direct meltwater to the outlet
        m_model.AddBrickProcess("meltwater", "outflow:direct", "outlet", true);

        m_model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        precip->SetValues({8.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        temperature->SetValues({-2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(m_tsTemp);
        wxDELETE(m_tsPrecip);
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
    EXPECT_TRUE(model.Initialize(m_model, basinProp));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
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
    EXPECT_TRUE(model.Initialize(m_model, basinProp));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
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
    SettingsModel m_model;
    TimeSeriesUniform* m_tsTemp{};
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsRad{};

    void SetUp() override {
        m_model.SetSolver("heun_explicit");
        m_model.SetTimer("2020-01-01", "2020-01-08", 1, "day");
        m_model.SetLogAll(true);

        // Precipitation
        m_model.GeneratePrecipitationSplitters(true);

        // Glacier land cover bricks
        m_model.AddLandCoverBrick("ground", "ground");
        m_model.AddLandCoverBrick("glacier_ice", "glacier");
        m_model.AddLandCoverBrick("glacier_debris", "glacier");

        // Snowpack brick
        m_model.GenerateSnowpacks("melt:temperature_index");

        // Glacier melt process
        m_model.SelectHydroUnitBrick("glacier_ice");
        m_model.AddBrickProcess("melt", "melt:temperature_index", "outlet");
        m_model.AddProcessLogging("output");
        m_model.AddBrickProcess("meltwater", "outflow:direct", "outlet", true);
        m_model.AddBrickParameter("no_melt_when_snow_cover", true);
        m_model.AddBrickParameter("infinite_storage", 1.0);
        m_model.SelectHydroUnitBrick("glacier_debris");
        m_model.AddBrickProcess("melt", "melt:temperature_index", "outlet");
        m_model.AddProcessLogging("output");
        m_model.AddBrickProcess("meltwater", "outflow:direct", "outlet", true);
        m_model.AddBrickParameter("no_melt_when_snow_cover", true);
        m_model.AddBrickParameter("infinite_storage", 1.0);

        // Route to the outlet
        m_model.SelectHydroUnitBrick("ground");
        m_model.AddBrickProcess("outflow", "outflow:direct", "outlet");
        m_model.SelectHydroUnitBrick("glacier_ice");
        m_model.AddBrickProcess("outflow", "outflow:direct", "outlet");
        m_model.SelectHydroUnitBrick("glacier_debris");
        m_model.AddBrickProcess("outflow", "outflow:direct", "outlet");

        m_model.AddLoggingToItem("outlet");

        // Set melt process parameters
        EXPECT_TRUE(m_model.SetParameterValue("type:snowpack", "radiation_coefficient", 0.0006f));
        EXPECT_TRUE(m_model.SetParameterValue("type:glacier", "radiation_coefficient", 0.001f));

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        precip->SetValues({8.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        temperature->SetValues({-2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);

        auto radiation = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        radiation->SetValues({100, 110, 120, 130, 140, 150, 160, 170});
        m_tsRad = new TimeSeriesUniform(Radiation);
        m_tsRad->SetData(radiation);
    }
    void TearDown() override {
        wxDELETE(m_tsRad);
        wxDELETE(m_tsTemp);
        wxDELETE(m_tsPrecip);
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
    EXPECT_TRUE(model.Initialize(m_model, basinProp));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsRad));
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
