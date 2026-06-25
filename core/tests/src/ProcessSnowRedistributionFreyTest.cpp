#include <gtest/gtest.h>

#include <memory>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model for the conceptual snow redistribution (Frey & Holzmann, 2015).
 *
 * The structure is identical to the SnowSlide test model; only the redistribution process kind differs and
 * is chosen per test through BuildModel().
 */
class SnowRedistributionFreyModel : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;

    void SetUp() override {
        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                              TimeUnit::Day);
        precip->SetValues({0.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                                   TimeUnit::Day);
        temperature->SetValues({-10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temperature));
    }

    // Build the model structure with the given snow redistribution process kind.
    void BuildModel(const string& redistributionProcess) {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model.SetLogAll(true);

        // Precipitation
        _model.GeneratePrecipitationSplitters(true);

        // Land covers
        _model.AddLandCoverBrick("ground", "ground");
        _model.AddLandCoverBrick("glacier", "glacier");

        // Snowpacks and redistribution
        _model.GenerateSnowpacks("melt:degree_day");
        _model.AddSnowRedistribution(redistributionProcess);
        _model.SelectHydroUnitBrick("glacier_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);

        // Direct the meltwater to the outlet
        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.AddBrickProcess("meltwater", "outflow:direct");
        _model.AddProcessOutput("outlet");
        _model.SelectHydroUnitBrick("glacier_snowpack");
        _model.AddBrickProcess("meltwater", "outflow:direct");
        _model.AddProcessOutput("outlet");

        // Direct the land cover water to the outlet
        _model.SelectHydroUnitBrick("ground");
        _model.AddBrickProcess("outflow", "outflow:direct");
        _model.AddProcessOutput("outlet");
        _model.SelectHydroUnitBrick("glacier");
        _model.AddBrickProcess("outflow", "outflow:direct");
        _model.AddProcessOutput("outlet");
    }

    // A linear chain of 5 hydro units with decreasing slope and equal areas / cover fractions.
    static SettingsBasin BuildBasin() {
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

        basinSettings.AddLateralConnection(1, 2, 1.0);
        basinSettings.AddLateralConnection(2, 3, 1.0);
        basinSettings.AddLateralConnection(3, 4, 1.0);
        basinSettings.AddLateralConnection(4, 5, 1.0);

        return basinSettings;
    }
};

TEST_F(SnowRedistributionFreyModel, ConstantDensityConservesMass) {
    BuildModel("transport:snow_redistribution_frey");

    SettingsBasin basinSettings = BuildBasin();

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings, false));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    vecAxxd unitContent = logger->GetHydroUnitValues();

    // Items that should be zero (water containers, etc.)
    for (int j = 0; j < 10; ++j) {
        vecInt idx = {0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12};
        for (int i = 0; i < idx.size(); ++i) {
            EXPECT_NEAR(unitContent[idx[i]](j, 0), 0.0, 0.000001);
        }
    }

    // Snow must move downslope: the steep summit (unit 0) ends up with less snow than the flat outlet
    // (unit 4) in the ground snowpack (entry id 6).
    double summitSwe = unitContent[6](9, 0);
    double valleySwe = unitContent[6](9, 4);
    EXPECT_LT(summitSwe, valleySwe);

    // Mass conservation: total snow storage change equals the snow input.
    double totSwe = logger->GetTotalSnowStorageChanges();
    double totSnowInput = 8 * 100.0;  // 8 days of 100 mm precipitation
    EXPECT_NEAR(totSnowInput, totSwe, 0.01);
}

TEST_F(SnowRedistributionFreyModel, DynamicDensityConservesMass) {
    BuildModel("transport:snow_redistribution_frey_dynamic");

    SettingsBasin basinSettings = BuildBasin();

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings, false));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    vecAxxd unitContent = logger->GetHydroUnitValues();

    // Items that should be zero (water containers, etc.)
    for (int j = 0; j < 10; ++j) {
        vecInt idx = {0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12};
        for (int i = 0; i < idx.size(); ++i) {
            EXPECT_NEAR(unitContent[idx[i]](j, 0), 0.0, 0.000001);
        }
    }

    // Snow must move downslope.
    double summitSwe = unitContent[6](9, 0);
    double valleySwe = unitContent[6](9, 4);
    EXPECT_LT(summitSwe, valleySwe);

    // Mass conservation.
    double totSwe = logger->GetTotalSnowStorageChanges();
    double totSnowInput = 8 * 100.0;
    EXPECT_NEAR(totSnowInput, totSwe, 0.01);
}
