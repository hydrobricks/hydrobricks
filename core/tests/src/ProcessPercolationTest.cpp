#include <gtest/gtest.h>

#include <memory>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: a store with a capacity drained by a power-law percolation to the outlet,
 * outflow = percolation_rate * (S / capacity)^exponent.
 */
class PercolationPowerLaw : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

        _model.AddHydroUnitBrick("storage", "storage");
        _model.AddBrickParameter("capacity", 100.0f);
        _model.AddBrickForcing("precipitation");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("percolation", "percolation:power_law");
        _model.SetProcessParameterValue("percolation_rate", 2.0f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        _model.AddLoggingToItem("outlet");

        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
        data->SetValues({0.0, 50.0, 0.0, 0.0, 30.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(data));
    }

    void RunAndCheck(const vecDouble& expectedOutputs) {
        SettingsBasin basinSettings;
        basinSettings.AddHydroUnit(1, 100);

        Subbasin subbasin;
        EXPECT_TRUE(subbasin.Initialize(basinSettings));

        ModelHydro model(&subbasin);
        ASSERT_TRUE(model.Initialize(_model, basinSettings));

        ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
        ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

        EXPECT_TRUE(model.Run());

        vecAxd basinOutputs = model.GetLogger()->GetSubbasinValues();
        for (auto& basinOutput : basinOutputs) {
            ASSERT_EQ(basinOutput.size(), expectedOutputs.size());
            for (int j = 0; j < basinOutput.size(); ++j) {
                EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
            }
        }

        // The water balance closes.
        vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
        double storageContent = unitContent[0](9, 0);
        EXPECT_NEAR(80.0 - basinOutputs[0].sum() - storageContent, 0, 0.0000000001);
    }
};

TEST_F(PercolationPowerLaw, LinearInTheFillingByDefault) {
    // Explicit Euler, daily: out_t = 2 (S_t / 100); S_{t+1} = S_t + P_t - out_t
    RunAndCheck({0.000000, 0.000000, 1.000000, 0.980000, 0.960400, 1.541192, 1.510368, 1.480161, 1.450558, 1.421546});
}

TEST_F(PercolationPowerLaw, ExponentReducesThePercolationOfAPartlyFilledStore) {
    _model.SelectHydroUnitBrickByName("storage");
    _model.SelectProcess("percolation");
    _model.SetProcessParameterValue("exponent", 2.0f);

    // out_t = 2 (S_t / 100)^2
    RunAndCheck({0.000000, 0.000000, 0.500000, 0.490050, 0.480395, 1.233378, 1.194940, 1.158280, 1.123289, 1.089869});
}

TEST_F(PercolationPowerLaw, StoreWithoutCapacityFailsInitialization) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    Subbasin subbasin;
    EXPECT_TRUE(subbasin.Initialize(basinSettings));

    SettingsModel settings;
    settings.SetSolver("euler_explicit");
    settings.SetTimer("2020-01-01", "2020-01-10", 1, "day");
    settings.AddHydroUnitBrick("storage", "storage");
    settings.AddBrickForcing("precipitation");
    settings.AddBrickProcess("percolation", "percolation:power_law");
    settings.SetProcessParameterValue("percolation_rate", 2.0f);
    settings.AddProcessOutput("outlet");
    settings.AddLoggingToItem("outlet");

    ModelHydro model(&subbasin);
    EXPECT_FALSE(model.Initialize(settings, basinSettings));
}
