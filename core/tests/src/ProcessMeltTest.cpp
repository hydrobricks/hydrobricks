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
        m_model.AddProcessForcing("temperature");
        m_model.AddProcessParameter("degree_day_factor", 3.0f);
        m_model.AddProcessParameter("melting_temperature", 2.0f);
        m_model.OutputProcessToSameBrick();
        m_model.AddProcessLogging("output");

        // Add process to direct meltwater to the outlet
        m_model.AddBrickProcess("meltwater", "outflow:direct");
        m_model.AddProcessOutput("outlet");
        m_model.AddProcessLogging("output");

        // Rain/snow splitter
        m_model.AddHydroUnitSplitter("snow-rain", "snow_rain");
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
    basinSettings.AddLandCover("ground", "", 1);

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
    basinProp.AddLandCover("ground", "ground", 1);

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
    double storage = logger->GetTotalStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
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
        m_model.AddProcessForcing("temperature");
        m_model.AddProcessParameter("degree_day_factor", 3.0f);
        m_model.AddProcessParameter("melting_temperature", 2.0f);
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
    basinSettings.AddLandCover("ground", "", 1);

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
        m_model.SelectHydroUnitBrick("glacier-snowpack");
        m_model.AddProcessParameter("degree_day_factor", 2.0f);
        m_model.AddProcessParameter("melting_temperature", 0.0f);
        m_model.AddProcessLogging("output");

        // Glacier melt process
        m_model.SelectHydroUnitBrick("glacier");
        m_model.AddBrickProcess("melt", "melt:degree_day", "outlet");
        m_model.AddBrickParameter("no_melt_when_snow_cover", true);
        m_model.AddBrickParameter("infinite_storage", 1.0);
        m_model.AddProcessForcing("temperature");
        m_model.AddProcessParameter("degree_day_factor", 3.0f);
        m_model.AddProcessParameter("melting_temperature", 0.0f);
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
    double storage = logger->GetTotalStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}
