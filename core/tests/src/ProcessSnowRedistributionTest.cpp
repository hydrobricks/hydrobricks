#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snow and ice model
 */

bool printContentValues = true;

class SnowRedistributionModel : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsPrecip{};
    TimeSeriesUniform* _tsTemp{};

    void SetUp() override {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model.SetLogAll(true);

        // Precipitation
        _model.GeneratePrecipitationSplitters(true);

        // Add default ground land cover
        _model.AddLandCoverBrick("ground", "ground");
        _model.AddLandCoverBrick("glacier", "glacier");

        // Snowpacks
        _model.GenerateSnowpacks("melt:degree_day");
        _model.AddSnowRedistribution("transport:snow_slide");
        _model.SelectHydroUnitBrick("glacier_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);

        // Ice melt
        _model.SelectHydroUnitBrick("glacier");
        // We don't care about the glacier melt in this test

        // Add process to direct meltwater to the outlet
        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.AddBrickProcess("meltwater", "outflow:direct");
        _model.AddProcessOutput("outlet");
        _model.SelectHydroUnitBrick("glacier_snowpack");
        _model.AddBrickProcess("meltwater", "outflow:direct");
        _model.AddProcessOutput("outlet");

        // Add process to direct water to the outlet
        _model.SelectHydroUnitBrick("ground");
        _model.AddBrickProcess("outflow", "outflow:direct");
        _model.AddProcessOutput("outlet");
        _model.SelectHydroUnitBrick("glacier");
        _model.AddBrickProcess("outflow", "outflow:direct");
        _model.AddProcessOutput("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0});
        _tsTemp = new TimeSeriesUniform(Temperature);
        _tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
        wxDELETE(_tsTemp);
    }
};

void logContent(const vecAxxd& unitContent, int entryId, int nbHydroUnits, const string& title) {
    if (!printContentValues) return;

    std::cout << "\n" << title << "\n";

    for (int hu = 0; hu < nbHydroUnits; ++hu) {
        axxd arr = unitContent[entryId].col(hu);
        std::cout << "SWE hydro unit " << hu << ": [";
        for (int i = 0; i < arr.size(); ++i) {
            std::cout << arr(i);
            if (i != arr.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }
}

TEST_F(SnowRedistributionModel, SnowRedistributionSimple) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100, 2400);
    basinSettings.AddHydroUnitPropertyDouble("slope", 80, "degree");
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(2, 100, 2300);
    basinSettings.AddHydroUnitPropertyDouble("slope", 60, "degree");
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(3, 100, 2200);
    basinSettings.AddHydroUnitPropertyDouble("slope", 40, "degree");
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(4, 100, 2100);
    basinSettings.AddHydroUnitPropertyDouble("slope", 20, "degree");
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(5, 100, 2000);
    basinSettings.AddHydroUnitPropertyDouble("slope", 0, "degree");
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    // Add lateral connections
    basinSettings.AddLateralConnection(1, 2, 1.0);
    basinSettings.AddLateralConnection(2, 3, 1.0);
    basinSettings.AddLateralConnection(3, 4, 1.0);
    basinSettings.AddLateralConnection(4, 5, 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check model values
    Logger* logger = model.GetLogger();
    vecAxxd unitContent = logger->GetHydroUnitValues();

    // Items that should be zero
    for (int j = 0; j < 10; ++j) {
        vecInt indx = {0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12};
        for (int i = 0; i < indx.size(); ++i) {
            EXPECT_NEAR(unitContent[indx[i]](j, 0), 0.0, 0.000001);
        }
    }

    logContent(unitContent, 6, 5, "Simple, ground snowpack");
    // logContent(unitContent, 10, 5, "Simple, glacier snowpack");

    // Check the water balance
    double totSwe = logger->GetTotalSnowStorageChanges();
    double totSnowInput = 8 * 100.0;  // 8 days of 100 mm precipitation

    EXPECT_NEAR(totSnowInput, totSwe, 0.01);
}

TEST_F(SnowRedistributionModel, SnowRedistributionDifferentLandCoverFractions) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100, 2400);
    basinSettings.AddHydroUnitPropertyDouble("slope", 80, "degree");
    basinSettings.AddLandCover("ground", "", 0.2);
    basinSettings.AddLandCover("glacier", "", 0.8);
    basinSettings.AddHydroUnit(2, 100, 2300);
    basinSettings.AddHydroUnitPropertyDouble("slope", 60, "degree");
    basinSettings.AddLandCover("ground", "", 0.0);
    basinSettings.AddLandCover("glacier", "", 1.0);
    basinSettings.AddHydroUnit(3, 100, 2200);
    basinSettings.AddHydroUnitPropertyDouble("slope", 40, "degree");
    basinSettings.AddLandCover("ground", "", 0.0);
    basinSettings.AddLandCover("glacier", "", 1.0);
    basinSettings.AddHydroUnit(4, 100, 2100);
    basinSettings.AddHydroUnitPropertyDouble("slope", 20, "degree");
    basinSettings.AddLandCover("ground", "", 0.9);
    basinSettings.AddLandCover("glacier", "", 0.1);
    basinSettings.AddHydroUnit(5, 100, 2000);
    basinSettings.AddHydroUnitPropertyDouble("slope", 0, "degree");
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    // Add lateral connections
    basinSettings.AddLateralConnection(1, 2, 1.0);
    basinSettings.AddLateralConnection(2, 3, 1.0);
    basinSettings.AddLateralConnection(3, 4, 1.0);
    basinSettings.AddLateralConnection(4, 5, 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check model values
    Logger* logger = model.GetLogger();
    vecAxxd unitContent = logger->GetHydroUnitValues();

    // Items that should be zero
    for (int j = 0; j < 10; ++j) {
        vecInt indx = {0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12};
        for (int i = 0; i < indx.size(); ++i) {
            EXPECT_NEAR(unitContent[indx[i]](j, 0), 0.0, 0.000001);
        }
    }

    logContent(unitContent, 6, 5, "Diff. landcover fractions, ground snowpack");
    logContent(unitContent, 10, 5, "Diff. landcover fractions, glacier snowpack");

    // Check the water balance
    double totSwe = logger->GetTotalSnowStorageChanges();
    double totSnowInput = 8 * 100.0;  // 8 days of 100 mm precipitation

    EXPECT_NEAR(totSnowInput, totSwe, 0.01);
}

TEST_F(SnowRedistributionModel, SnowRedistributionComplex) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100, 2400);
    basinSettings.AddHydroUnitPropertyDouble("slope", 80, "degree");
    basinSettings.AddLandCover("ground", "", 0.2);
    basinSettings.AddLandCover("glacier", "", 0.8);
    basinSettings.AddHydroUnit(2, 200, 2300);
    basinSettings.AddHydroUnitPropertyDouble("slope", 60, "degree");
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(3, 50, 2200);
    basinSettings.AddHydroUnitPropertyDouble("slope", 40, "degree");
    basinSettings.AddLandCover("ground", "", 1.0);
    basinSettings.AddLandCover("glacier", "", 0.0);
    basinSettings.AddHydroUnit(4, 150, 2100);
    basinSettings.AddHydroUnitPropertyDouble("slope", 20, "degree");
    basinSettings.AddLandCover("ground", "", 0.0);
    basinSettings.AddLandCover("glacier", "", 1.0);
    basinSettings.AddHydroUnit(5, 100, 2000);
    basinSettings.AddHydroUnitPropertyDouble("slope", 0, "degree");
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    // Add lateral connections
    basinSettings.AddLateralConnection(1, 2, 0.6);
    basinSettings.AddLateralConnection(1, 3, 0.4);
    basinSettings.AddLateralConnection(2, 3, 0.4);
    basinSettings.AddLateralConnection(2, 4, 0.3);
    basinSettings.AddLateralConnection(2, 5, 0.3);
    basinSettings.AddLateralConnection(3, 4, 0.8);
    basinSettings.AddLateralConnection(3, 5, 0.2);
    basinSettings.AddLateralConnection(4, 5, 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check model values
    Logger* logger = model.GetLogger();
    vecAxxd unitContent = logger->GetHydroUnitValues();

    // Items that should be zero
    for (int j = 0; j < 10; ++j) {
        vecInt indx = {0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12};
        for (int i = 0; i < indx.size(); ++i) {
            EXPECT_NEAR(unitContent[indx[i]](j, 0), 0.0, 0.000001);
        }
    }

    logContent(unitContent, 6, 5, "Diff. areas, ground snowpack");
    logContent(unitContent, 10, 5, "Diff. areas, glacier snowpack");

    // Check the water balance
    double totSwe = logger->GetTotalSnowStorageChanges();
    double totSnowInput = 8 * 100.0;  // 8 days of 100 mm precipitation

    EXPECT_NEAR(totSnowInput, totSwe, 0.01);
}